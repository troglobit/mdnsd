#include "unittest.h"

void test1()
{
	xht_t *ptr;

	ptr = xht_new(1);
	assert_non_null(ptr);
	xht_free(ptr);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test1),
//		cmocka_unit_test(test2),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
