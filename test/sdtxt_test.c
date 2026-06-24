#include "unittest.h"

#include <stdlib.h>
#include <string.h>

#include "libmdnsd/sdtxt.h"

/*
 * Regression test for a txt2sd() over-read: the parse loop dereferenced
 * the next length byte before checking that any bytes were left, and did
 * not count the length byte itself, so it stepped one past the record
 * buffer.  The buffer here is sized exactly to the records so the run
 * under AddressSanitizer faults on the over-read.
 */
static void test_txt2sd_no_overread(__attribute__((__unused__)) void **state)
{
	unsigned char raw[] = {
		7, 'k','e','y','=','v','a','l',
		7, 'f','o','o','=','b','a','r',
	};
	unsigned char *buf = malloc(sizeof(raw));
	xht_t *h;

	assert_non_null(buf);
	memcpy(buf, raw, sizeof(raw));

	h = txt2sd(buf, (int)sizeof(raw));
	assert_non_null(h);
	assert_string_equal("val", (char *)xht_get(h, "key"));
	assert_string_equal("bar", (char *)xht_get(h, "foo"));

	xht_free(h);
	free(buf);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_txt2sd_no_overread),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
