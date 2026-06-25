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

/*
 * Issue #58: a TXT string with no '=' is a boolean key (RFC 6763 §6.4)
 * and must be kept with an empty value, not dropped as RFC 1464 would.
 */
static void test_txt2sd_key_only(__attribute__((__unused__)) void **state)
{
	unsigned char raw[] = {
		7, 'k','e','y','=','v','a','l',
		4, 'f','l','a','g',
	};
	unsigned char *buf = malloc(sizeof(raw));
	char *flag;
	xht_t *h;

	assert_non_null(buf);
	memcpy(buf, raw, sizeof(raw));

	h = txt2sd(buf, (int)sizeof(raw));
	assert_non_null(h);
	assert_string_equal("val", (char *)xht_get(h, "key"));

	flag = xht_get(h, "flag");
	assert_non_null(flag);		/* present, not dropped */
	assert_string_equal("", flag);	/* boolean key, empty value */

	xht_free(h);
	free(buf);
}

/* A boolean key survives the encode/decode round-trip. */
static void test_sd2txt_key_only_roundtrip(__attribute__((__unused__)) void **state)
{
	xht_t *h = xht_new(11);
	xht_t *back;
	unsigned char *raw;
	int len;

	assert_non_null(h);
	xht_set(h, "flag", "");		/* empty value -> key-only on the wire */

	raw = sd2txt(h, &len);
	assert_non_null(raw);

	back = txt2sd(raw, len);
	assert_non_null(back);
	assert_non_null(xht_get(back, "flag"));
	assert_string_equal("", (char *)xht_get(back, "flag"));

	xht_free(back);
	free(raw);
	xht_free(h);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_txt2sd_no_overread),
		cmocka_unit_test(test_txt2sd_key_only),
		cmocka_unit_test(test_sd2txt_key_only_roundtrip),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
