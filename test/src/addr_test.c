#include "unittest.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netdb.h>


#include "../../src/mdnsd.h"


/*
 * Mock-ups
 */
int __wrap_getifaddrs(struct ifaddrs **ifap)
{
	*ifap = mock_ptr_type(struct ifaddrs *);
	return mock_type(int);
}

void __wrap_freeifaddrs(struct ifaddrs *ifa)
{
	/* Nothing allocated, nothing to free. */
}



struct sockaddr_in ipv4_10_0_20_1         = { AF_INET, 0, { 0x0114000a } };
struct sockaddr_in ipv4_LL_169_254_100_32 = { AF_INET, 0, { 0x2064fea9 } };
struct sockaddr_in ipv4_255_255_255_0     = { AF_INET, 0, { 0x00ffffff } };
struct sockaddr_in ipv4_192_168_2_100     = { AF_INET, 0, { 0x6402a8c0 } };

struct sockaddr_in6 ipv6_global       = { AF_INET6, 0, 0,
	{ 0x20,0x01, 0x0d,0xb8, 0x00,0x61, 0xfe,0x01, 0x00,0x00, 0x00,0x00, 0xca,0xfe, 0xba,0xbe}, 0 };
struct sockaddr_in6 ipv6_unique_local = { AF_INET6, 0, 0,
	{ 0xfc,0x00, 0x01,0xaa, 0xbb,0xcc, 0xfe,0x01, 0x00,0x00, 0x00,0x00, 0xca,0xfe, 0xba,0xbe}, 0 };
struct sockaddr_in6 ipv6_site_local   = { AF_INET6, 0, 0,
	{ 0xfe,0xc0, 0x51,0xac, 0xb0,0x0b, 0x52,0x01, 0x00,0x00, 0x00,0x00, 0xca,0xfe, 0xba,0xbe}, 0 };
struct sockaddr_in6 ipv6_link_local   = { AF_INET6, 0, 0,
	{ 0xfe,0x80, 0x11,0xac, 0xb0,0x0b, 0x52,0x01, 0x00,0x00, 0x00,0x00, 0xca,0xfe, 0xba,0xbe}, 0 };
struct sockaddr_in6 ipv6_FF           = { AF_INET6, 0, 0,
	{ 0xff,0xff, 0xff,0xff, 0xff,0xff, 0xff,0xff, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00}, 0 };
struct sockaddr_in6 ipv6_global_2      = { AF_INET6, 0, 0,
	{ 0x20,0x01, 0x0d,0xb8, 0x00,0x00, 0xfe,0x02, 0x00,0x00, 0x00,0x00, 0xde,0xad, 0xc0,0xce}, 0 };




/*
 * Tests
 */

/*
 * Test behaviour when no interfaces exist.
 */
static void test_iface_init_no_ifc(__attribute__((__unused__)) void **state)
{
	will_return(__wrap_getifaddrs, NULL);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	struct iface *iface = iface_iterator(1);
	assert_null(iface);
}



/* Helper function testing that one IPv4 is set on one interface. */
static void check_one_iface_one_global_ipv4(char* ifname)
{
	struct iface *iface = iface_iterator(1);
	assert_non_null(iface);

	assert_int_equal(0, iface->unused);
	assert_int_equal(1, iface->changed);
	assert_string_equal(ifname, iface->ifname);
	/* Ignoring the (undeterminable) ifindex. */

	assert_int_equal(ipv4_10_0_20_1.sin_addr.s_addr, iface->inaddr.s_addr);
	assert_int_equal(0x00000000, iface->inaddr_old.s_addr);
	assert_true(IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr));
	assert_true(IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr_old));

	assert_int_equal(-1, iface->sd);
	assert_null(iface->mdns);
	assert_int_equal(1, iface->hostid);


	iface = iface_iterator(0);
	assert_null(iface);
}

/*
 * Test that one IPv4 is set.
 */
static void test_iface_init_one_ifc_ipv4(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs = {
		NULL,
		"if0c1ip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
		(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
	};

	will_return(__wrap_getifaddrs, &addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv4(addrs.ifa_name);
}

/*
 * Test that a global IPv4 address is not overwritten with a link local one.
 * One interface, only IPv4.
 */
static void test_iface_init_one_ifc_global_ipv4_LL_ipv4(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0Gip4LLip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		},
		{
			NULL,
			"if0Gip4LLip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_LL_169_254_100_32, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		}
	};

	will_return(__wrap_getifaddrs, addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv4(addrs->ifa_name);
}

/*
 * Test that a link local IPv4 address is overwritten by a global one.
 * One interface, only IPv4.
 */
static void test_iface_init_one_ifc_LL_ipv4_global_ipv4(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0LLip4Gip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_LL_169_254_100_32, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		},
		{
			NULL,
			"if0LLip4Gip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		}
	};

	will_return(__wrap_getifaddrs, addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv4(addrs->ifa_name);
}




/* Helper function testing that one IPv6 is set on one interface. */
static void check_one_iface_one_global_ipv6(char* ifname)
{
	struct iface *iface = iface_iterator(1);
	assert_non_null(iface);

	assert_int_equal(0, iface->unused);
	assert_int_equal(1, iface->changed);
	assert_string_equal(ifname, iface->ifname);
	/* Ignoring the (undeterminable) ifindex. */

	assert_int_equal(0x00000000, iface->inaddr.s_addr);
	assert_int_equal(0x00000000, iface->inaddr_old.s_addr);
	assert_true(IN6_ARE_ADDR_EQUAL(&ipv6_global.sin6_addr, &iface->in6addr));
	assert_true(IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr_old));

	assert_int_equal(-1, iface->sd);
	assert_null(iface->mdns);
	assert_int_equal(1, iface->hostid);


	iface = iface_iterator(0);
	assert_null(iface);
}

/*
 * Test that one IPv6 is set.
 */
static void test_iface_init_one_ifc_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs = {
		NULL,
		"if0c1ip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
		(struct sockaddr*)&ipv6_global, (struct sockaddr*)&ipv6_FF, NULL, NULL
	};

	will_return(__wrap_getifaddrs, &addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv6(addrs.ifa_name);
}

/*
 * Test that a global IPv6 address is not overwritten with a link local one.
 * One interface, only IPv6.
 */
static void test_iface_init_one_ifc_global_ipv6_LL_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0Gip6LLip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_global, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			NULL,
			"if0Gip6LLip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		}
	};

	will_return(__wrap_getifaddrs, addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv6(addrs->ifa_name);
}

/*
 * Test that a link local IPv6 address is overwritten by a global one.
 * One interface, only IPv6.
 */
static void test_iface_init_one_ifc_LL_ipv6_global_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0LLip6Gip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			NULL,
			"if0LLip6Gip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_global, (struct sockaddr*)&ipv6_FF, NULL, NULL
		}
	};

	will_return(__wrap_getifaddrs, addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv6(addrs->ifa_name);
}

/*
 * Test that IPv6 addresses of lower preference get overwritten by addresses with higher one.
 * null < link local < site local < unique local < global
 * One interface, only IPv6.
 */
static void test_iface_init_one_ifc_LL_SL_UL_global_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0LLSLULGip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			addrs + 2,
			"if0LLSLULGip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_site_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			addrs + 3,
			"if0LLSLULGip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_unique_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			NULL,
			"if0LLSLULGip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_global, (struct sockaddr*)&ipv6_FF, NULL, NULL
		}
	};

	will_return(__wrap_getifaddrs, addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv6(addrs->ifa_name);
}

/*
 * Test that IPv6 addresses of higher preference do not get overwritten by addresses with lower one.
 * null < link local < site local < unique local < global
 * One interface, only IPv6.
 */
static void test_iface_init_one_ifc_global_UL_SL_LL_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0GULSLLLip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_global, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			addrs + 2,
			"if0GULSLLLip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_unique_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			addrs + 3,
			"if0GULSLLLip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_site_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			NULL,
			"if0GULSLLLip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		}
	};

	will_return(__wrap_getifaddrs, addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv6(addrs->ifa_name);
}



/* Helper function testing that one IPv4 and one IPv6 is set on one interface. */
static void check_one_iface_global_ipv4_ipv6(char* ifname)
{
	struct iface *iface = iface_iterator(1);
	assert_non_null(iface);

	assert_int_equal(0, iface->unused);
	assert_int_equal(1, iface->changed);
	assert_string_equal(ifname, iface->ifname);
	/* Ignoring the (undeterminable) ifindex. */

	assert_int_equal(ipv4_10_0_20_1.sin_addr.s_addr, iface->inaddr.s_addr);
	assert_int_equal(0x00000000, iface->inaddr_old.s_addr);
	assert_true(IN6_ARE_ADDR_EQUAL(&ipv6_global.sin6_addr, &iface->in6addr));
	assert_true(IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr_old));

	assert_int_equal(-1, iface->sd);
	assert_null(iface->mdns);
	assert_int_equal(1, iface->hostid);


	iface = iface_iterator(0);
	assert_null(iface);
}

/*
 * Test that one IPv4 and one IPv6 is set.
 * One interface, IPv4 and IPv6
 */
static void test_iface_init_one_ifc_ipv4_ll_global_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs +1,
			"if0c2ip4llgip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		},
		{
			addrs + 2,
			"if0c2ip4llgip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			NULL,
			"if0c2ip4llgip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_global, (struct sockaddr*)&ipv6_FF, NULL, NULL
		}

	};

	will_return(__wrap_getifaddrs, &addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_global_ipv4_ipv6(addrs->ifa_name);
}


/*
 * Test with two interfaces, IPv4 and IPv6
 */
static void test_iface_init_two_ifc_ipv4_ll_global_ipv6_ipv4_ipv6(__attribute__((__unused__)) void **state)
{
	struct ifaddrs addrs[] = {
		{
			addrs + 1,
			"if0c3ip4llgip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		},
		{
			addrs + 2,
			"if1c2ip4gip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv4_192_168_2_100, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
		},
		{
			addrs + 3,
			"if0c3ip4llgip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			addrs + 4,
			"if0c3ip4llgip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_link_local, (struct sockaddr*)&ipv6_FF, NULL, NULL
		},
		{
			NULL,
			"if1c2ip4gip6", IFF_UP | IFF_BROADCAST | IFF_MULTICAST,
			(struct sockaddr*)&ipv6_global_2, (struct sockaddr*)&ipv6_FF, NULL, NULL
		}

	};

	will_return(__wrap_getifaddrs, &addrs);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_global_ipv4_ipv6(addrs->ifa_name);
	struct iface *iface = iface_iterator(1);
	assert_non_null(iface);

	assert_int_equal(0, iface->unused);
	assert_int_equal(1, iface->changed);
	assert_string_equal(addrs[0].ifa_name, iface->ifname);
	/* Ignoring the (undeterminable) ifindex. */

	assert_int_equal(ipv4_10_0_20_1.sin_addr.s_addr, iface->inaddr.s_addr);
	assert_int_equal(0x00000000, iface->inaddr_old.s_addr);
	assert_true(IN6_ARE_ADDR_EQUAL(&ipv6_global.sin6_addr, &iface->in6addr));
	assert_true(IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr_old));

	assert_int_equal(-1, iface->sd);
	assert_null(iface->mdns);
	assert_int_equal(1, iface->hostid);


	iface = iface_iterator(0);
	assert_non_null(iface);

	assert_int_equal(0, iface->unused);
	assert_int_equal(1, iface->changed);
	assert_string_equal(addrs[1].ifa_name, iface->ifname);
	/* Ignoring the (undeterminable) ifindex. */

	assert_int_equal(ipv4_192_168_2_100.sin_addr.s_addr, iface->inaddr.s_addr);
	assert_int_equal(0x00000000, iface->inaddr_old.s_addr);
	assert_true(IN6_ARE_ADDR_EQUAL(&ipv6_global_2.sin6_addr, &iface->in6addr));
	assert_true(IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr_old));

	assert_int_equal(-1, iface->sd);
	assert_null(iface->mdns);
	assert_int_equal(1, iface->hostid);


	iface = iface_iterator(0);
	assert_null(iface);
}




static int teardown(__attribute__((__unused__)) void **state)
{
	iface_exit();
}


int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_iface_init_no_ifc),

		cmocka_unit_test_teardown(test_iface_init_one_ifc_ipv4, teardown),
		cmocka_unit_test_teardown(test_iface_init_one_ifc_global_ipv4_LL_ipv4, teardown),
		cmocka_unit_test_teardown(test_iface_init_one_ifc_LL_ipv4_global_ipv4, teardown),

		cmocka_unit_test_teardown(test_iface_init_one_ifc_ipv6, teardown),
		cmocka_unit_test_teardown(test_iface_init_one_ifc_global_ipv6_LL_ipv6, teardown),
		cmocka_unit_test_teardown(test_iface_init_one_ifc_LL_ipv6_global_ipv6, teardown),
		cmocka_unit_test_teardown(test_iface_init_one_ifc_LL_SL_UL_global_ipv6, teardown),
		cmocka_unit_test_teardown(test_iface_init_one_ifc_global_UL_SL_LL_ipv6, teardown),

		cmocka_unit_test_teardown(test_iface_init_one_ifc_ipv4_ll_global_ipv6, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
