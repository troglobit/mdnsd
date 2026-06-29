#include "unittest.h"

#include <arpa/inet.h>
#include <string.h>

#include "libmdnsd/mdnsd.h"

static void conflict(__attribute__((__unused__)) char *name,
		     __attribute__((__unused__)) int type,
		     __attribute__((__unused__)) void *arg)
{
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

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_records_clear_frees),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
