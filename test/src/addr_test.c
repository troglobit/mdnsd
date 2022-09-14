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
struct sockaddr_in ipv4_255_255_255_0     = { AF_INET, 0, { 0x00ffffff } };
struct sockaddr_in ipv4_LL_169_254_100_32 = { AF_INET, 0, { 0x2064fea9 } };

struct ifaddrs one_ifc_one_ip4 = {
	NULL,
	"if0c1ip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST, 
	(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
};


struct ifaddrs one_ifc_global_ip4_ll_ip4[] = {
	{
		one_ifc_global_ip4_ll_ip4 + 1,
		"if0Gip4LLip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST, 
		(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
	},
	{
		NULL,
		"if0Gip4LLip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST, 
		(struct sockaddr*)&ipv4_LL_169_254_100_32, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
	}
};


struct ifaddrs one_ifc_ll_ip4_global_ip4[] = {
	{
		one_ifc_ll_ip4_global_ip4 + 1,
		"if0LLip4Gip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST, 
		(struct sockaddr*)&ipv4_LL_169_254_100_32, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
	},
	{
		NULL,
		"if0LLip4Gip4", IFF_UP | IFF_BROADCAST | IFF_MULTICAST, 
		(struct sockaddr*)&ipv4_10_0_20_1, (struct sockaddr*)&ipv4_255_255_255_0, NULL, NULL
	}
};




/*
 * Tests
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

	/* Ignoring the (undeterminable) ifindex. */

	assert_int_equal(0, iface->unused);
	assert_int_equal(1, iface->changed);
	assert_string_equal(ifname, iface->ifname);

	assert_int_equal(ipv4_10_0_20_1.sin_addr.s_addr, iface->inaddr.s_addr);
	assert_int_equal(0x00000000, iface->inaddr_old.s_addr);

	assert_int_equal(-1, iface->sd);
	assert_null(iface->mdns);
	assert_int_equal(1, iface->hostid);


	iface = iface_iterator(0);
	assert_null(iface);
}

/*
 * Test behaviour when no interfaces exist.
 */
static void test_iface_init_one_ifc_ipv4(__attribute__((__unused__)) void **state)
{
	will_return(__wrap_getifaddrs, &one_ifc_one_ip4);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv4(one_ifc_one_ip4.ifa_name);
}

/*
 * Test that a global IPv4 address is not overwritten with a link local one.
 * One interface, only IPv4.
 */
static void test_iface_init_one_ifc_global_ipv4_LL_ipv4(__attribute__((__unused__)) void **state)
{
	will_return(__wrap_getifaddrs, one_ifc_global_ip4_ll_ip4);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv4(one_ifc_global_ip4_ll_ip4->ifa_name);
}

/*
 * Test that a link local IPv4 address is overwritten by a global one.
 * One interface, only IPv4.
 */
static void test_iface_init_one_ifc_LL_ipv4_global_ipv4(__attribute__((__unused__)) void **state)
{
	will_return(__wrap_getifaddrs, one_ifc_ll_ip4_global_ip4);
	will_return(__wrap_getifaddrs, 0);

	iface_init(NULL);

	check_one_iface_one_global_ipv4(one_ifc_ll_ip4_global_ip4->ifa_name);
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
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
