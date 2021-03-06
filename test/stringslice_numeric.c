#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

#define NO_OUTPUTl (-0xDEADBEEFBADC0FEEl)
#define NO_OUTPUT (-0xDEADBEEF)
static void test_stringslice_match_long_base(struct aem_stringslice slice, int base, struct aem_stringslice slice_expect, int result_expect, long int out_expect)
{
	if (aem_log_header(&aem_log_buf, AEM_LOG_INFO)) {
		aem_stringbuf_puts(&aem_log_buf, "stringslice_match_long_base(");
		debug_slice(&aem_log_buf, slice);
		aem_stringbuf_printf(&aem_log_buf, ", %d) expect (", base);
		debug_slice(&aem_log_buf, slice_expect);
		aem_stringbuf_printf(&aem_log_buf, ", %d, %ld)\n", result_expect, out_expect);
		aem_log_str(&aem_log_buf);
	}

	if (aem_stringslice_len(slice_expect) > aem_stringslice_len(slice))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: remaining input length (%zd) <= original input length (%zd)", aem_stringslice_len(slice_expect), aem_stringslice_len(slice));

	if ((!!result_expect) == (ss_eq(slice, slice_expect)))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: failure <=> input unchanged");

	if ((!!result_expect) != (out_expect != NO_OUTPUTl))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: failure <=> output unchanged");

	struct aem_stringslice slice_ret = slice;
	long int out = NO_OUTPUTl;
	int result = aem_stringslice_match_long_base(&slice_ret, base, &out);

	if (result != result_expect || !ss_eq(slice_ret, slice_expect) || out != out_expect) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "stringslice_match_long_base(");
			debug_slice(&aem_log_buf, slice);
			aem_stringbuf_printf(&aem_log_buf, ", %d) returned (", base);
			debug_slice(&aem_log_buf, slice_ret);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %ld), expected (", result, out);
			debug_slice(&aem_log_buf, slice_expect);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %ld)\n", result_expect, out_expect);
			aem_log_str(&aem_log_buf);
		}
	}
}
static void test_stringslice_match_uint_base(struct aem_stringslice slice, int base, struct aem_stringslice slice_expect, int result_expect, unsigned int out_expect)
{
	if (aem_log_header(&aem_log_buf, AEM_LOG_INFO)) {
		aem_stringbuf_puts(&aem_log_buf, "stringslice_match_uint_base(");
		debug_slice(&aem_log_buf, slice);
		aem_stringbuf_printf(&aem_log_buf, ", %d) expect (", base);
		debug_slice(&aem_log_buf, slice_expect);
		aem_stringbuf_printf(&aem_log_buf, ", %d, %d)\n", result_expect, out_expect);
		aem_log_str(&aem_log_buf);
	}

	if (aem_stringslice_len(slice_expect) > aem_stringslice_len(slice))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: remaining input length (%zd) <= original input length (%zd)", aem_stringslice_len(slice_expect), aem_stringslice_len(slice));

	if ((!!result_expect) == (ss_eq(slice, slice_expect)))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: failure <=> input unchanged");

	if ((!!result_expect) != (out_expect != NO_OUTPUT))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: failure <=> output unchanged");

	struct aem_stringslice slice_ret = slice;
	unsigned int out = NO_OUTPUT;
	int result = aem_stringslice_match_uint_base(&slice_ret, base, &out);

	if (result != result_expect || !ss_eq(slice_ret, slice_expect) || out != out_expect) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "stringslice_match_uint_base(");
			debug_slice(&aem_log_buf, slice);
			aem_stringbuf_printf(&aem_log_buf, ", %d) returned (", base);
			debug_slice(&aem_log_buf, slice_ret);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %d), expected (", result, out);
			debug_slice(&aem_log_buf, slice_expect);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %d)\n", result_expect, out_expect);
			aem_log_str(&aem_log_buf);
		}
	}
}

static void test_stringslice_match_long_auto(struct aem_stringslice slice, struct aem_stringslice slice_expect, int result_expect, long int out_expect)
{
	if (aem_log_header(&aem_log_buf, AEM_LOG_INFO)) {
		aem_stringbuf_puts(&aem_log_buf, "stringslice_match_long_auto(");
		debug_slice(&aem_log_buf, slice);
		aem_stringbuf_printf(&aem_log_buf, ") expect (");
		debug_slice(&aem_log_buf, slice_expect);
		aem_stringbuf_printf(&aem_log_buf, ", %d, %ld)\n", result_expect, out_expect);
		aem_log_str(&aem_log_buf);
	}

	if (aem_stringslice_len(slice_expect) > aem_stringslice_len(slice))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: remaining input length (%zd) <= original input length (%zd)", aem_stringslice_len(slice_expect), aem_stringslice_len(slice));

	if ((!!result_expect) != (out_expect != NO_OUTPUTl))
		aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: return failure <=> output unchanged");

	struct aem_stringslice slice_ret = slice;
	long int out = NO_OUTPUTl;
	int result = aem_stringslice_match_long_auto(&slice_ret, &out);

	if (result != result_expect || !ss_eq(slice_ret, slice_expect) || out != out_expect) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "stringslice_match_long_auto(");
			debug_slice(&aem_log_buf, slice);
			aem_stringbuf_printf(&aem_log_buf, ") returned (");
			debug_slice(&aem_log_buf, slice_ret);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %ld), expected (", result, out);
			debug_slice(&aem_log_buf, slice_expect);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %ld)\n", result_expect, out_expect);
			aem_log_str(&aem_log_buf);
		}
	}
}

int main(int argc, char **argv)
{
	aem_log_stderr();
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	aem_logf_ctx(AEM_LOG_NOTICE, "test aem_stringslice_match_long_base");

	test_stringslice_match_uint_base(aem_ss_cstr(   "100000000"), 16, aem_ss_cstr(   "100000000"), 0, NO_OUTPUT  );
	test_stringslice_match_uint_base(aem_ss_cstr(    "FFFFFFFF"), 16, aem_ss_cstr(            ""), 1,  0xFFFFFFFF);
	test_stringslice_match_uint_base(aem_ss_cstr(    "80000000"), 16, aem_ss_cstr(            ""), 1,  0x80000000);
	test_stringslice_match_uint_base(aem_ss_cstr(   "-80000000"), 16, aem_ss_cstr(   "-80000000"), 0, NO_OUTPUT  );
	test_stringslice_match_uint_base(aem_ss_cstr(    "7FFFFFFF"), 16, aem_ss_cstr(            ""), 1,  0x7FFFFFFF);
	test_stringslice_match_uint_base(aem_ss_cstr(   "-7FFFFFFF"), 16, aem_ss_cstr(   "-7FFFFFFF"), 0, NO_OUTPUT  );

	test_stringslice_match_long_base(aem_ss_cstr(""  ), 10, aem_ss_cstr("" ), 0, NO_OUTPUTl);
	test_stringslice_match_long_base(aem_ss_cstr(" " ), 10, aem_ss_cstr(" "), 0, NO_OUTPUTl);
	test_stringslice_match_long_base(aem_ss_cstr("_" ), 10, aem_ss_cstr("_"), 0, NO_OUTPUTl);
	test_stringslice_match_long_base(aem_ss_cstr("-" ), 10, aem_ss_cstr("-"), 0, NO_OUTPUTl);
	test_stringslice_match_long_base(aem_ss_cstr("a" ), 10, aem_ss_cstr("a"), 0, NO_OUTPUTl);
	test_stringslice_match_long_base(aem_ss_cstr("0" ), 10, aem_ss_cstr("" ), 1,  0       );
	test_stringslice_match_long_base(aem_ss_cstr( "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001-1"), 10, aem_ss_cstr("-1" ), 1,  1);
	test_stringslice_match_long_base(aem_ss_cstr("-0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001-1"), 10, aem_ss_cstr("-1" ), 1,  -1);
	test_stringslice_match_long_base(aem_ss_cstr("0 "), 10, aem_ss_cstr(" "), 1,  0       );
	test_stringslice_match_long_base(aem_ss_cstr("1" ), 10, aem_ss_cstr("" ), 1,  1       );
	test_stringslice_match_long_base(aem_ss_cstr("-1"), 10, aem_ss_cstr("" ), 1, -1       );
	test_stringslice_match_long_base(aem_ss_cstr("10"), 10, aem_ss_cstr("" ), 1, 10       );
	test_stringslice_match_long_base(aem_ss_cstr("10"),  2, aem_ss_cstr("" ), 1,  2       );
	test_stringslice_match_long_base(aem_ss_cstr("10"), 36, aem_ss_cstr("" ), 1, 36       );
	test_stringslice_match_long_base(aem_ss_cstr("0123456789abcdefghijklmnopqrstuvwxyz"),  2, aem_ss_cstr("23456789abcdefghijklmnopqrstuvwxyz"), 1, 1                  );
	test_stringslice_match_long_base(aem_ss_cstr("0123456789abcdefghijklmnopqrstuvwxyz"), 10, aem_ss_cstr(        "abcdefghijklmnopqrstuvwxyz"), 1, 123456789          );
	test_stringslice_match_long_base(aem_ss_cstr("0123456789abcdefghijklmnopqrstuvwxyz"), 16, aem_ss_cstr(              "ghijklmnopqrstuvwxyz"), 1, 0x0123456789abcdefl);
	test_stringslice_match_long_base(aem_ss_cstr("0123456789abcdefghijklmnopqrstuvwxyz"), 17, aem_ss_cstr(               "hijklmnopqrstuvwxyz"), 1, 0x2cd843cb47643708l);
	test_stringslice_match_long_base(aem_ss_cstr( "zzzzzzzzzzzz"                       ), 36, aem_ss_cstr(               ""), 1,  0x41c21cb8e0ffffffl);
	test_stringslice_match_long_base(aem_ss_cstr("-zzzzzzzzzzzz"                       ), 36, aem_ss_cstr(               ""), 1, -0x41c21cb8e0ffffffl);

	test_stringslice_match_long_base(aem_ss_cstr( "9223372036854775808"), 10, aem_ss_cstr( "9223372036854775808"), 0, NO_OUTPUTl           );
	test_stringslice_match_long_base(aem_ss_cstr("-9223372036854775808"), 10, aem_ss_cstr(                    ""), 1,  0x8000000000000000l);
	test_stringslice_match_long_base(aem_ss_cstr( "9223372036854775807"), 10, aem_ss_cstr(                    ""), 1,  0x7FFFFFFFFFFFFFFFl);
	test_stringslice_match_long_base(aem_ss_cstr("-9223372036854775807"), 10, aem_ss_cstr(                    ""), 1,  0x8000000000000001l);

	test_stringslice_match_long_base(aem_ss_cstr(   "10000000000000000"), 16, aem_ss_cstr(   "10000000000000000"), 0, NO_OUTPUTl           );
	test_stringslice_match_long_base(aem_ss_cstr(    "FFFFFFFFFFFFFFFF"), 16, aem_ss_cstr(    "FFFFFFFFFFFFFFFF"), 0, NO_OUTPUTl           );
	test_stringslice_match_long_base(aem_ss_cstr(    "8000000000000000"), 16, aem_ss_cstr(    "8000000000000000"), 0, NO_OUTPUTl           );
	test_stringslice_match_long_base(aem_ss_cstr(   "-8000000000000000"), 16, aem_ss_cstr(                    ""), 1,  0x8000000000000000l);
	test_stringslice_match_long_base(aem_ss_cstr(    "7FFFFFFFFFFFFFFF"), 16, aem_ss_cstr(                    ""), 1,  0x7FFFFFFFFFFFFFFFl);
	test_stringslice_match_long_base(aem_ss_cstr(   "-7FFFFFFFFFFFFFFF"), 16, aem_ss_cstr(                    ""), 1,  0x8000000000000001l);

	test_stringslice_match_long_base(aem_ss_cstr(       "1y2p0ij32e8e8"), 36, aem_ss_cstr(       "1y2p0ij32e8e8"), 0, NO_OUTPUTl           );
	test_stringslice_match_long_base(aem_ss_cstr(      "-1y2p0ij32e8e8"), 36, aem_ss_cstr(                    ""), 1,  0x8000000000000000l);
	test_stringslice_match_long_base(aem_ss_cstr(       "1y2p0ij32e8e7"), 36, aem_ss_cstr(                    ""), 1,  0x7FFFFFFFFFFFFFFFl);
	test_stringslice_match_long_base(aem_ss_cstr(      "-1y2p0ij32e8e7"), 36, aem_ss_cstr(                    ""), 1,  0x8000000000000001l);

	aem_logf_ctx(AEM_LOG_NOTICE, "test aem_stringslice_match_long_auto");

	test_stringslice_match_long_auto(aem_ss_cstr(""     ), aem_ss_cstr(""     ), 0, NO_OUTPUTl);
	test_stringslice_match_long_auto(aem_ss_cstr( "10"  ), aem_ss_cstr(""     ), 1,  10      );
	test_stringslice_match_long_auto(aem_ss_cstr("-10"  ), aem_ss_cstr(""     ), 1, -10      );
	test_stringslice_match_long_auto(aem_ss_cstr("0x"   ), aem_ss_cstr("0x"   ), 0, NO_OUTPUTl);
	test_stringslice_match_long_auto(aem_ss_cstr("-0x"  ), aem_ss_cstr("-0x"  ), 0, NO_OUTPUTl);
	test_stringslice_match_long_auto(aem_ss_cstr("0x10" ), aem_ss_cstr(""     ), 1,  16);
	test_stringslice_match_long_auto(aem_ss_cstr("-0x10"), aem_ss_cstr(""     ), 1, -16);
	test_stringslice_match_long_auto(aem_ss_cstr("0b"   ), aem_ss_cstr("0b"   ), 0, NO_OUTPUTl);
	test_stringslice_match_long_auto(aem_ss_cstr("0x-1" ), aem_ss_cstr("0x-1" ), 0, NO_OUTPUTl);
	test_stringslice_match_long_auto(aem_ss_cstr("-0x-1"), aem_ss_cstr("-0x-1"), 0, NO_OUTPUTl);


	return show_test_results();
}
