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

/*
 * Feature test for issue #76 (RFC 6763 §12): a PTR query response must
 * carry the instance's SRV and TXT, plus the SRV target's A/AAAA, in the
 * additional section.
 */
static void test_additional_records_for_ptr(__attribute__((__unused__)) void **state)
{
	char *type = "_qotd._tcp.local.";
	char *inst = "qotd._qotd._tcp.local.";
	char *host = "qotd-host.local.";
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	struct answered seen = { 0 };
	struct message m, resp;
	struct in_addr ip;
	struct in6_addr ip6;
	mdns_record_t *ptr, *r;
	int i, srv = 0, txt = 0, a = 0, aaaa = 0;

	assert_non_null(d);

	/* Publish a full service: PTR -> SRV/TXT (instance) -> A/AAAA (host) */
	ptr = mdnsd_shared(d, type, QTYPE_PTR, 120);
	mdnsd_set_host(d, ptr, inst);
	r = mdnsd_shared(d, inst, QTYPE_SRV, 120);
	mdnsd_set_srv(d, r, 0, 0, 9, host);
	r = mdnsd_shared(d, inst, QTYPE_TXT, 4500);
	mdnsd_set_raw(d, r, "\011txtvers=1", 10);
	r = mdnsd_shared(d, host, QTYPE_A, 120);
	inet_pton(AF_INET, "192.168.0.1", &ip);
	mdnsd_set_ip(d, r, ip);
	r = mdnsd_shared(d, host, QTYPE_AAAA, 120);
	inet_pton(AF_INET6, "fe80::1", &ip6);
	mdnsd_set_ipv6(d, r, ip6);

	/* Emit the PTR as an answer, then append §12 additional records the
	 * way mdnsd_out() does, and re-parse to check the result. */
	memset(&m, 0, sizeof(m));
	message_an(&m, ptr->rr.name, QTYPE_PTR, QCLASS_IN, ptr->rr.ttl);
	_a_copy(&m, &ptr->rr);
	_answered_add(&seen, ptr);
	_additional(d, &m, ptr, &seen);

	memset(&resp, 0, sizeof(resp));
	assert_int_equal(0, message_parse(&resp, message_packet(&m)));

	assert_int_equal(1, resp.ancount);
	assert_int_equal(QTYPE_PTR, resp.an[0].type);
	assert_int_equal(4, resp.arcount);	/* SRV + TXT + A + AAAA, no repeats */

	for (i = 0; i < resp.arcount; i++) {
		switch (resp.ar[i].type) {
		case QTYPE_SRV:  srv++;  break;
		case QTYPE_TXT:  txt++;  break;
		case QTYPE_A:    a++;    break;
		case QTYPE_AAAA: aaaa++; break;
		}
	}
	assert_int_equal(1, srv);
	assert_int_equal(1, txt);
	assert_int_equal(1, a);
	assert_int_equal(1, aaaa);

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

/*
 * Two instances of one service type that share a target host must not
 * repeat that host's address record in the additional section.
 */
static void test_additional_records_dedup(__attribute__((__unused__)) void **state)
{
	char *type = "_http._tcp.local.";
	char *i1   = "i1._http._tcp.local.";
	char *i2   = "i2._http._tcp.local.";
	char *host = "shared.local.";
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	struct answered seen = { 0 };
	struct message m, resp;
	struct in_addr ip;
	mdns_record_t *ptr1, *ptr2, *r;
	int i, a = 0, srv = 0;

	assert_non_null(d);

	ptr1 = mdnsd_shared(d, type, QTYPE_PTR, 120);
	mdnsd_set_host(d, ptr1, i1);
	ptr2 = mdnsd_shared(d, type, QTYPE_PTR, 120);
	mdnsd_set_host(d, ptr2, i2);
	r = mdnsd_shared(d, i1, QTYPE_SRV, 120);
	mdnsd_set_srv(d, r, 0, 0, 80, host);
	r = mdnsd_shared(d, i2, QTYPE_SRV, 120);
	mdnsd_set_srv(d, r, 0, 0, 81, host);
	r = mdnsd_shared(d, host, QTYPE_A, 120);
	inet_pton(AF_INET, "192.168.0.1", &ip);
	mdnsd_set_ip(d, r, ip);

	/* Both PTRs are answers; expand each like mdnsd_out()'s final pass. */
	memset(&m, 0, sizeof(m));
	message_an(&m, ptr1->rr.name, QTYPE_PTR, QCLASS_IN, ptr1->rr.ttl);
	_a_copy(&m, &ptr1->rr);
	message_an(&m, ptr2->rr.name, QTYPE_PTR, QCLASS_IN, ptr2->rr.ttl);
	_a_copy(&m, &ptr2->rr);
	_answered_add(&seen, ptr1);
	_answered_add(&seen, ptr2);
	_additional(d, &m, ptr1, &seen);
	_additional(d, &m, ptr2, &seen);

	memset(&resp, 0, sizeof(resp));
	assert_int_equal(0, message_parse(&resp, message_packet(&m)));
	assert_int_equal(3, resp.arcount);	/* two SRV + one shared A */

	for (i = 0; i < resp.arcount; i++) {
		switch (resp.ar[i].type) {
		case QTYPE_A:   a++;   break;
		case QTYPE_SRV: srv++; break;
		}
	}
	assert_int_equal(2, srv);	/* one SRV per instance */
	assert_int_equal(1, a);		/* shared host's A only once */

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

/*
 * Issue #94: _a_match() compared rdata and names without guarding NULL, so
 * an empty-rdata record -- or a PTR/NS/CNAME with no decoded name -- handed
 * a NULL to memcmp()/strcmp(), both declared nonnull.  @evverx hit it with
 * two empty-rdata records sent back to back, which collide in the cache via
 * _a_match().  Exercise both branches directly under the sanitizer.
 */
static void test_a_match_empty_rdata(__attribute__((__unused__)) void **state)
{
	struct resource r;
	mdns_answer_t a;

	memset(&r, 0, sizeof(r));
	memset(&a, 0, sizeof(a));

	/* TXT has no special case, so it lands in the default rdata compare;
	 * both records carry empty rdata (rdata == NULL, rdlen == 0). */
	r.name = "yo"; r.type = QTYPE_TXT;
	a.name = "yo"; a.type = QTYPE_TXT;

	/* Equal empty rdata is a match, without memcmp(NULL, NULL, 0). */
	assert_true(_a_match(&r, &a));
}

/* A PTR with no decoded name must not reach strcmp(NULL, ...). */
static void test_a_match_null_rdname(__attribute__((__unused__)) void **state)
{
	struct resource r;
	mdns_answer_t a;

	memset(&r, 0, sizeof(r));
	memset(&a, 0, sizeof(a));

	r.name = "yo"; r.type = QTYPE_PTR;	/* r.known.ns.name == NULL */
	a.name = "yo"; a.type = QTYPE_PTR;	/* a.rdname == NULL */

	assert_false(_a_match(&r, &a));
}

/*
 * Issue #97 (companion to #94): a name or SRV answer whose target did not
 * decode leaves known.*.name NULL, which _cache() handed to strdup().
 * Exercise the guard directly, like the #94 _a_match() checks.
 */
static void test_cache_null_rdname(__attribute__((__unused__)) void **state)
{
	unsigned short type[] = { QTYPE_NS, QTYPE_CNAME, QTYPE_PTR };
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	inet_addr_t from;
	struct resource r;
	size_t i;

	assert_non_null(d);
	memset(&from, 0, sizeof(from));

	for (i = 0; i < sizeof(type) / sizeof(type[0]); i++) {
		memset(&r, 0, sizeof(r));	/* known.ns.name == NULL */
		r.name  = "x.local.";
		r.type  = type[i];
		r.class = QCLASS_IN;
		r.ttl   = 120;			/* >0, else the delete path returns first */

		assert_int_equal(1, _cache(d, &r, &from));
	}

	memset(&r, 0, sizeof(r));
	r.name  = "x.local.";
	r.type  = QTYPE_PTR;
	r.class = QCLASS_IN;
	r.ttl   = 120;
	r.known.ns.name = "target.local.";
	assert_int_equal(0, _cache(d, &r, &from));

	mdnsd_free(d);
}

/* Same guard on the SRV arm, whose target lives at a different union offset. */
static void test_cache_null_srv(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	inet_addr_t from;
	struct resource r;

	assert_non_null(d);
	memset(&from, 0, sizeof(from));

	memset(&r, 0, sizeof(r));		/* known.srv.name == NULL */
	r.name  = "x._svc._tcp.local.";
	r.type  = QTYPE_SRV;
	r.class = QCLASS_IN;
	r.ttl   = 120;
	assert_int_equal(1, _cache(d, &r, &from));

	memset(&r, 0, sizeof(r));
	r.name  = "x._svc._tcp.local.";
	r.type  = QTYPE_SRV;
	r.class = QCLASS_IN;
	r.ttl   = 120;
	r.known.srv.name = "target.local.";
	assert_int_equal(0, _cache(d, &r, &from));

	mdnsd_free(d);
}

/*
 * RFC 6762 §5.4: the top bit of qclass in a question is the unicast-response
 * (QU) bit.  Such a question must be answered by unicast to the querier,
 * even when it comes from port 5353, with all matching records in one
 * normal mDNS response.  §6.7: a query from another port is a legacy one,
 * its reply echoes the ID and question and has no cache-flush bits.
 */
static int feed_question(mdns_daemon_t *d, unsigned short id, unsigned short class, unsigned short port)
{
	inet_addr_t from = { 0 };
	struct sockaddr_in *sin = (struct sockaddr_in *)&from;
	struct message out = { 0 }, in = { 0 };

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(port);
	inet_pton(AF_INET, "192.0.2.10", &sin->sin_addr);

	out.id = id;
	message_qd(&out, "host.local.", QTYPE_A, class);
	assert_int_equal(0, message_parse(&in, message_packet(&out)));

	return mdnsd_in(d, &in, &from);
}

/* host.local. with two A records: one shared, one unique and done probing */
static mdns_daemon_t *publish_two_a(void)
{
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	struct in_addr ip;
	mdns_record_t *r;

	assert_non_null(d);
	r = mdnsd_shared(d, "host.local.", QTYPE_A, 120);
	inet_pton(AF_INET, "192.0.2.1", &ip);
	mdnsd_set_ip(d, r, ip);

	r = mdnsd_unique(d, "host.local.", QTYPE_A, 120, NULL, NULL);
	inet_pton(AF_INET, "192.0.2.2", &ip);
	mdnsd_set_ip(d, r, ip);
	r->unique = 5;				/* probing done */

	return d;
}

/* Take the next outgoing packet, it must be a unicast one to port */
static void reply(mdns_daemon_t *d, unsigned short port, struct message *resp)
{
	struct message out;
	inet_addr_t to;

	assert_int_equal(1, mdnsd_out(d, &out, &to));
	assert_int_equal(port, inet_port(&to));
	memset(resp, 0, sizeof(*resp));
	assert_int_equal(0, message_parse(resp, message_packet(&out)));
}

static void test_qm_question(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = publish_two_a();

	assert_int_equal(0, feed_question(d, 0, QCLASS_IN, 5353));
	assert_null(d->uanswers);		/* QM from 5353: multicast answer only */

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

static void test_qu_question(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = publish_two_a();
	struct message resp;
	int i, flush = 0;

	assert_int_equal(0, feed_question(d, 0, QCLASS_IN | 0x8000, 5353));
	reply(d, 5353, &resp);

	assert_int_equal(0, resp.id);
	assert_int_equal(0, resp.qdcount);
	assert_int_equal(2, resp.ancount);	/* both records, one packet */
	for (i = 0; i < resp.ancount; i++) {
		assert_int_equal(QTYPE_A, resp.an[i].type);
		if (resp.an[i].class == (QCLASS_IN | 0x8000))
			flush++;
		else
			assert_int_equal(QCLASS_IN, resp.an[i].class);
	}
	assert_int_equal(1, flush);		/* cache-flush on the unique one */
	assert_null(d->uanswers);

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

static void test_legacy_question(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = publish_two_a();
	struct message resp;
	int i;

	assert_int_equal(0, feed_question(d, 0x1234, QCLASS_IN, 40000));
	reply(d, 40000, &resp);

	assert_int_equal(0x1234, resp.id);
	assert_int_equal(1, resp.qdcount);
	assert_string_equal("host.local.", resp.qd[0].name);
	assert_int_equal(QTYPE_A, resp.qd[0].type);
	assert_int_equal(2, resp.ancount);
	for (i = 0; i < resp.ancount; i++)
		assert_int_equal(QCLASS_IN, resp.an[i].class);	/* no cache-flush */
	assert_null(d->uanswers);

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

/*
 * Legacy reply to a PTR query: the RFC 6763 §12 additional records must
 * not carry the cache-flush bit either.  A unicast DNS parser, e.g. BIND's
 * in dig(1), rejects a message with mixed classes as malformed.
 */
static void test_legacy_additional(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	inet_addr_t from = { 0 };
	struct sockaddr_in *sin = (struct sockaddr_in *)&from;
	struct message out = { 0 }, in = { 0 }, resp;
	struct in_addr ip;
	mdns_record_t *r;
	int i;

	assert_non_null(d);
	r = mdnsd_shared(d, "_qotd._tcp.local.", QTYPE_PTR, 120);
	mdnsd_set_host(d, r, "qotd._qotd._tcp.local.");
	r = mdnsd_unique(d, "qotd._qotd._tcp.local.", QTYPE_SRV, 120, NULL, NULL);
	mdnsd_set_srv(d, r, 0, 0, 17, "host.local.");
	r->unique = 5;
	r = mdnsd_unique(d, "host.local.", QTYPE_A, 120, NULL, NULL);
	inet_pton(AF_INET, "192.0.2.1", &ip);
	mdnsd_set_ip(d, r, ip);
	r->unique = 5;

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(40000);
	inet_pton(AF_INET, "192.0.2.10", &sin->sin_addr);
	out.id = 0x4242;
	message_qd(&out, "_qotd._tcp.local.", QTYPE_PTR, QCLASS_IN);
	assert_int_equal(0, message_parse(&in, message_packet(&out)));
	assert_int_equal(0, mdnsd_in(d, &in, &from));

	reply(d, 40000, &resp);
	assert_int_equal(1, resp.ancount);
	assert_int_equal(2, resp.arcount);	/* SRV + A */
	for (i = 0; i < resp.arcount; i++)
		assert_int_equal(QCLASS_IN, resp.ar[i].class);

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

/* A unique record still probing is not ours yet, no unicast answer with it */
static void test_qu_question_probing(__attribute__((__unused__)) void **state)
{
	mdns_daemon_t *d = mdnsd_new(QCLASS_IN, 1000);
	struct in_addr ip;
	mdns_record_t *r;

	assert_non_null(d);
	r = mdnsd_unique(d, "host.local.", QTYPE_A, 120, NULL, NULL);
	inet_pton(AF_INET, "192.0.2.2", &ip);
	mdnsd_set_ip(d, r, ip);
	assert_true(r->unique < 5);

	assert_int_equal(0, feed_question(d, 0, QCLASS_IN | 0x8000, 5353));
	assert_null(d->uanswers);

	mdnsd_shutdown(d);
	mdnsd_free(d);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_known_answer_ptr_compression),
		cmocka_unit_test(test_known_answer_srv_compression),
		cmocka_unit_test(test_additional_records_for_ptr),
		cmocka_unit_test(test_additional_records_dedup),
		cmocka_unit_test(test_a_match_empty_rdata),
		cmocka_unit_test(test_a_match_null_rdname),
		cmocka_unit_test(test_cache_null_rdname),
		cmocka_unit_test(test_cache_null_srv),
		cmocka_unit_test(test_qm_question),
		cmocka_unit_test(test_qu_question),
		cmocka_unit_test(test_legacy_question),
		cmocka_unit_test(test_legacy_additional),
		cmocka_unit_test(test_qu_question_probing),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
