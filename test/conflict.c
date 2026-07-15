#include "unittest.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

#include "libmdnsd/mdnsd.h"
#include "libmdnsd/1035.h"

static int conflicts;

static void conflict(__attribute__((__unused__)) char *name,
		     __attribute__((__unused__)) int type,
		     __attribute__((__unused__)) void *arg)
{
	conflicts++;
}

/* Publish the record set conf.c installs for one service: shared PTRs plus
 * the probed (unique) SRV/A/AAAA/TXT records the daemon defends. */
static void publish(mdns_daemon_t *d)
{
	struct in_addr ip = { .s_addr = htonl(0xc0a82a65) };	/* 192.168.42.101 */
	struct in6_addr ip6;
	mdns_record_t *r;

	inet_pton(AF_INET6, "fe80::2a:65", &ip6);

	r = mdnsd_shared(d, "_ssh._tcp.local.", QTYPE_PTR, 120);
	mdnsd_set_host(d, r, "host._ssh._tcp.local.");

	r = mdnsd_unique(d, "host._ssh._tcp.local.", QTYPE_SRV, 120, conflict, NULL);
	mdnsd_set_srv(d, r, 0, 0, 22, "host.local.");

	r = mdnsd_unique(d, "host.local.", QTYPE_A, 120, conflict, NULL);
	mdnsd_set_ip(d, r, ip);

	r = mdnsd_unique(d, "host.local.", QTYPE_AAAA, 120, conflict, NULL);
	mdnsd_set_ipv6(d, r, ip6);

	r = mdnsd_unique(d, "host._ssh._tcp.local.", QTYPE_TXT, 4500, conflict, NULL);
	mdnsd_set_raw(d, r, "\011txtvers=1", 10);
}

/*
 * Issue #93: a name conflict reloads the config, which calls
 * records_clear() to drop the published records before re-publishing under
 * a new name.  records_clear() unlinked each record from the queues but
 * never freed it, so every conflict leaked a full generation of records.
 * Cycle a few reloads so the leak surfaces under AddressSanitizer.
 */
static void test_records_clear_frees(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);

	assert_non_null(d);

	for (int i = 0; i < 3; i++) {
		publish(d);
		records_clear(d);
	}

	mdnsd_free(d);
}

#define HOST_IP 0xc0a80001	/* 192.168.0.1, both published and answered */

/* Publish one probed A record host.local. = HOST_IP */
static mdns_daemon_t *publish_unique(void)
{
	struct in_addr ip = { .s_addr = htonl(HOST_IP) };
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	mdns_record_t *r;

	assert_non_null(d);
	r = mdnsd_unique(d, "host.local.", QTYPE_A, 120, conflict, NULL);
	mdnsd_set_ip(d, r, ip);

	return d;
}

/* Feed an A-record answer for host.local. = HOST_IP, sourced from from_ip */
static void feed_answer(mdns_daemon_t *d, const char *from_ip)
{
	struct in_addr ip = { .s_addr = htonl(HOST_IP) };
	inet_addr_t from = { 0 };
	struct sockaddr_in *sin = (struct sockaddr_in *)&from;
	struct message out = { 0 }, in = { 0 };

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(5353);
	inet_pton(AF_INET, from_ip, &sin->sin_addr);

	out.header.qr = 1;
	message_an(&out, "host.local.", QTYPE_A, QCLASS_IN, 120);
	message_rdata_ipv4(&out, ip);
	assert_int_equal(0, message_parse(&in, message_packet(&out)));

	mdnsd_in(d, &in, &from);
}

/*
 * A matching answer for our unique record is a conflict only when another
 * host sends it; the same reply looped back from one of our own addresses
 * must be ignored -- the self-conflict loop that commit 1dbcbb0 first
 * papered over, now guarded at the _conflict() sites instead.
 */
static void check_conflict(const char *from_ip, int expected)
{
	mdns_daemon_t *d = publish_unique();

	conflicts = 0;
	feed_answer(d, from_ip);
	assert_int_equal(expected, conflicts);

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

static void test_conflict_remote(__attribute__((__unused__)) void **state)
{
	check_conflict("203.0.113.5", 1);
}

static void test_conflict_local_suppressed(__attribute__((__unused__)) void **state)
{
	check_conflict("127.0.0.1", 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_records_clear_frees),
		cmocka_unit_test(test_conflict_remote),
		cmocka_unit_test(test_conflict_local_suppressed),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
