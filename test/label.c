#include "unittest.h"

#include <stdlib.h>

/* White-box: _lmatch() is static, so pull in the codec source directly. */
#include "libmdnsd/1035.c"

/*
 * Regression test for issue #37.  A compression pointer that resolves to
 * the root label (0), matched against another root label, made _lmatch()
 * step past the terminating 0 and dereference one byte beyond the name.
 * Each root label sits at the end of its own heap buffer so the run under
 * AddressSanitizer faults on any over-read; the result must be a match.
 */
static void test_lmatch_pointer_to_root(__attribute__((__unused__)) void **state)
{
	struct message *m = calloc(1, sizeof(*m));
	unsigned char *buf = malloc(3);
	unsigned char *l1  = malloc(2);
	unsigned char *l2  = malloc(1);

	assert_non_null(m);
	assert_non_null(buf);
	assert_non_null(l1);
	assert_non_null(l2);

	buf[0] = 0xff;
	buf[1] = 0xff;
	buf[2] = 0x00;		/* root label, last byte of the buffer */
	m->_buf = buf;

	l1[0] = 0xc0;		/* compression pointer, _ldecomp() -> offset 2 */
	l1[1] = 0x02;
	l2[0] = 0x00;		/* bare root label, last byte of the buffer */

	assert_int_equal(1, _lmatch(m, (char *)l1, (char *)l2));

	free(l2);
	free(l1);
	free(buf);
	free(m);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lmatch_pointer_to_root),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
