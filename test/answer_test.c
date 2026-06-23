#include "unittest.h"

#include <string.h>

/* White-box: _a_copy() is static, so pull in the library source directly. */
#include "libmdnsd/mdnsd.c"

/*
 * Regression test for issue #79.  A cached name record (PTR/SRV/...) is
 * stored with both a decompressed rdname and the raw on-wire rdata, which
 * holds compression pointers into the packet it arrived in.  When that
 * answer is replayed as a known answer, _a_copy() must re-encode the name
 * from rdname; replaying the raw rdata verbatim carries stale pointers and
 * produces a malformed packet.
 */
static void test_known_answer_ptr_compression(__attribute__((__unused__)) void **state)
{
	unsigned char rdata[2] = { 0xc0, 0x0c };	/* pointer to offset 12 */
	struct message m, check;
	mdns_answer_t a;

	memset(&a, 0, sizeof(a));
	a.name   = "_qotd._tcp.local.";
	a.type   = QTYPE_PTR;
	a.ttl    = 120;
	a.rdname = "host._qotd._tcp.local.";
	a.rdata  = rdata;
	a.rdlen  = sizeof(rdata);

	memset(&m, 0, sizeof(m));
	message_qd(&m, a.name, QTYPE_PTR, QCLASS_IN);
	message_an(&m, a.name, a.type, QCLASS_IN, a.ttl);
	_a_copy(&m, &a);

	/* The packet must parse and the PTR must resolve to the real name. */
	memset(&check, 0, sizeof(check));
	assert_int_equal(0, message_parse(&check, message_packet(&m)));
	assert_int_equal(1, check.ancount);
	assert_non_null(check.an[0].known.ns.name);
	assert_string_equal(a.rdname, check.an[0].known.ns.name);
}

/* Same bug via an SRV known answer: the target name must re-compress. */
static void test_known_answer_srv_compression(__attribute__((__unused__)) void **state)
{
	unsigned char rdata[8] = { 0, 0, 0, 0, 0, 9, 0xc0, 0x0c };  /* prio/weight/port + ptr */
	struct message m, check;
	mdns_answer_t a;

	memset(&a, 0, sizeof(a));
	a.name = "host._qotd._tcp.local.";
	a.type = QTYPE_SRV;
	a.ttl  = 120;
	a.rdname = "target.local.";
	a.srv.port = 9;
	a.rdata = rdata;
	a.rdlen = sizeof(rdata);

	memset(&m, 0, sizeof(m));
	message_qd(&m, a.name, QTYPE_SRV, QCLASS_IN);
	message_an(&m, a.name, a.type, QCLASS_IN, a.ttl);
	_a_copy(&m, &a);

	memset(&check, 0, sizeof(check));
	assert_int_equal(0, message_parse(&check, message_packet(&m)));
	assert_int_equal(1, check.ancount);
	assert_string_equal(a.rdname, check.an[0].known.srv.name);
	assert_int_equal(a.srv.port, check.an[0].known.srv.port);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_known_answer_ptr_compression),
		cmocka_unit_test(test_known_answer_srv_compression),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
