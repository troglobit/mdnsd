#include "unittest.h"

#include <limits.h>
#include <string.h>
#include <sys/time.h>
#include "libmdnsd/mdnsd.h"

/*
 * Regression tests for issue #99: a wall-clock jump while the daemon is
 * probing leaves it mute.  All deadlines are absolute gettimeofday()
 * stamps, so the test takes over the clock.  A macro rather than a linker
 * --wrap, since glibc renames the symbol on 32-bit time64 builds.
 */
static struct timeval fake_now;

static int fake_gettimeofday(struct timeval *tv, void *tz)
{
	(void)tz;
	*tv = fake_now;

	return 0;
}

#define gettimeofday fake_gettimeofday

/* White-box: _tvdiff() and the daemon timers are static. */
#include "libmdnsd/mdnsd.c"

static void clock_set(time_t sec, suseconds_t usec)
{
	fake_now.tv_sec = sec;
	fake_now.tv_usec = usec;
}

/*
 * _tvdiff() multiplies the seconds delta by 1e6 in a long.  A delta larger
 * than LONG_MAX / 1e6 (~35 min on 32-bit) wraps, and the sign of the result
 * is then meaningless.  Size the delta from LONG_MAX so the test bites on
 * every host, not only the 32-bit ones the bug hit.
 */
static void test_tvdiff_overflow(__attribute__((__unused__)) void **state)
{
	struct timeval past = { 0, 0 };
	struct timeval future = { LONG_MAX / 1000000 + 60, 0 };
	struct timeval small = { 10, 500000 };
	struct timeval later = { 12, 250000 };

	/* Sanity: ordinary deltas keep their exact value */
	assert_int_equal(1750000, _tvdiff(small, later));
	assert_int_equal(-1750000, _tvdiff(later, small));

	/* A deadline in the past is due, one in the future is not */
	assert_true(_tvdiff(future, past) < 0);
	assert_true(_tvdiff(past, future) > 0);
}

/*
 * The Buildroot scenario: mdnsd starts at epoch, sends its first probe,
 * then the test harness runs `date -s` and the clock jumps to 2026.  The
 * probe deadline is now ~56 years overdue and must fire immediately.
 */
static void test_probe_after_clock_jump(__attribute__((__unused__)) void **state)
{
	struct in_addr ip = { .s_addr = htonl(0x0a000001) };
	struct timeval *tv;
	struct message m;
	mdns_daemon_t *d;
	mdns_record_t *r;
	inet_addr_t to;
	int i;

	/* Boot without an RTC: it is 1970 */
	clock_set(5, 0);
	d = mdnsd_new(QCLASS_IN, 1000);
	assert_non_null(d);

	r = mdnsd_unique(d, "host.local.", QTYPE_A, 120, NULL, NULL);
	assert_non_null(r);
	mdnsd_set_ip(d, r, ip);

	/* First probe goes out right away */
	assert_true(mdnsd_out(d, &m, &to) > 0);
	assert_non_null(d->probing);

	/* date -s @1788032864 */
	clock_set(1788032864, 0);

	/* Overdue deadline, the daemon must not want to sleep */
	tv = mdnsd_sleep(d);
	assert_int_equal(0, tv->tv_sec);
	assert_int_equal(0, tv->tv_usec);

	/* Remaining probes and the announcement follow within a few ticks */
	for (i = 0; i < 10 && d->probing; i++) {
		mdnsd_out(d, &m, &to);
		clock_set(fake_now.tv_sec + 1, 0);
	}
	assert_null(d->probing);
	assert_int_equal(5, r->unique);

	mdnsd_out(d, &m, &to);
	assert_true(r->tries > 0);

	mdnsd_free(d);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_tvdiff_overflow),
		cmocka_unit_test(test_probe_after_clock_jump),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
