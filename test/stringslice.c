#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

static void test_stringslice_match(struct aem_stringslice slice, const char *s, int result_expect, struct aem_stringslice slice_expect)
{
	struct aem_stringslice slice_ret = slice;
	int result = aem_stringslice_match(&slice_ret, s);

	if (result != result_expect || !ss_eq(slice_ret, slice_expect)) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "stringslice_match(");
			debug_slice(&aem_log_buf, slice);
			aem_stringbuf_printf(&aem_log_buf, ", \"%s\") returned (%d, ", s, result);
			debug_slice(&aem_log_buf, slice_ret);
			aem_stringbuf_printf(&aem_log_buf, "), expected (%d, ", result_expect);
			debug_slice(&aem_log_buf, slice_expect);
			aem_stringbuf_puts(&aem_log_buf, ")\n");
			aem_log_str(&aem_log_buf);
		}
	}
}

static void test_stringslice_match_end(struct aem_stringslice slice, const char *s, int result_expect, struct aem_stringslice slice_expect)
{
	struct aem_stringslice slice_ret = slice;
	int result = aem_stringslice_match_end(&slice_ret, s);

	if (result != result_expect || !ss_eq(slice_ret, slice_expect)) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "stringslice_match_end(");
			debug_slice(&aem_log_buf, slice);
			aem_stringbuf_printf(&aem_log_buf, ", \"%s\") returned (%d, ", s, result);
			debug_slice(&aem_log_buf, slice_ret);
			aem_stringbuf_printf(&aem_log_buf, "), expected (%d, ", result_expect);
			debug_slice(&aem_log_buf, slice_expect);
			aem_stringbuf_puts(&aem_log_buf, ")\n");
			aem_log_str(&aem_log_buf);
		}
	}
}
static void test_stringslice_match_line_multi(struct aem_stringslice slice, int state, int finish, struct aem_stringslice result_expect, struct aem_stringslice slice_expect)
{
	// - If finish flag is set, state is set to zero.
	// - Otherwise:
	//   - If no input is available, state remains the same.
	//   - Otherwise:
	//     - If any input remains, state is set to zero.
	//     - Otherwise, state is set iff the input ended with \r.
	int state_expect = !finish && (aem_stringslice_ok(slice) ? slice.end[-1] == '\r' && !aem_stringslice_ok(slice_expect) : state);

	if (aem_log_header(&aem_log_buf, AEM_LOG_INFO)) {
		aem_stringbuf_puts(&aem_log_buf, "stringslice_match_line_multi(");
		debug_slice(&aem_log_buf, slice);
		aem_stringbuf_printf(&aem_log_buf, ", %d, %d) expect (", state, finish);
		debug_slice(&aem_log_buf, result_expect);
		aem_stringbuf_puts(&aem_log_buf, ", ");
		debug_slice(&aem_log_buf, slice_expect);
		aem_stringbuf_printf(&aem_log_buf, ", %d)", state_expect);
		aem_stringbuf_puts(&aem_log_buf, "\n");
		aem_log_str(&aem_log_buf);
	}

	// Make sure testcase is sane
	{
		struct aem_stringslice slice_consumed = slice;
		if (!aem_stringslice_match_suffix(&slice_consumed, slice_expect))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: remaining input is not a suffix of original input");

		size_t consumed = aem_stringslice_len(slice_consumed);
		aem_assert(consumed == aem_stringslice_len(slice) - aem_stringslice_len(slice_expect));

		if (aem_stringslice_len(result_expect) > consumed)
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: result length (%zd) <= consumed bytes", aem_stringslice_len(result_expect), consumed);

		if (finish && !(consumed || !aem_stringslice_ok(slice)))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: finish => no input or make progress");

		if (!slice_expect.start && state_expect)
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: no output => !state_expect");

		if (state_expect && finish)
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: finish => !state_expect");

		if (state_expect != state && !finish && !aem_stringslice_ok(slice))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: !finish && no input => state remains the same");

		if (state_expect && aem_stringslice_ok(slice_expect))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: input remains => !state_expect");

		if (state_expect && aem_stringslice_ok(slice) && slice.end[-1] != '\r')
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: slice doesn't end with \\r => !state_expect");
	}

	// Run testcase
	struct aem_stringslice slice_ret = slice;
	int state_ret = state;
	struct aem_stringslice result = aem_stringslice_match_line_multi(&slice_ret, &state_ret, finish);

	if (!ss_eq(result, result_expect) || !ss_eq(slice_ret, slice_expect) || state_ret != state_expect || slice.end != slice_ret.end) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "stringslice_match_line_multi(");
			debug_slice(&aem_log_buf, slice);
			aem_stringbuf_printf(&aem_log_buf, ", %d, %d) returned (", state, finish);
			debug_slice(&aem_log_buf, result);
			aem_stringbuf_puts(&aem_log_buf, ", ");
			debug_slice(&aem_log_buf, slice_ret);
			aem_stringbuf_printf(&aem_log_buf, ", %d), expected (", state_ret);
			debug_slice(&aem_log_buf, result_expect);
			aem_stringbuf_puts(&aem_log_buf, ", ");
			debug_slice(&aem_log_buf, slice_expect);
			aem_stringbuf_printf(&aem_log_buf, ", %d)", state_expect);
			if (slice.end != slice_ret.end)
				aem_stringbuf_puts(&aem_log_buf, " (input slice end moved)");
			aem_stringbuf_puts(&aem_log_buf, "\n");
			aem_log_str(&aem_log_buf);
		}
	}
}

int main(int argc, char **argv)
{
	aem_log_stderr();
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	aem_logf_ctx(AEM_LOG_NOTICE, "test aem_stringslice_match{,_end}");

	test_stringslice_match(aem_ss_cstr(""), "", 1, aem_ss_cstr(""));
	test_stringslice_match_end(aem_ss_cstr(""), "", 1, aem_ss_cstr(""));

	test_stringslice_match(aem_ss_cstr("abcd"), "", 1, aem_ss_cstr("abcd"));
	test_stringslice_match_end(aem_ss_cstr("abcd"), "", 1, aem_ss_cstr("abcd"));

	test_stringslice_match(aem_ss_cstr("ab"), "ab", 1, aem_ss_cstr(""));
	test_stringslice_match_end(aem_ss_cstr("cd"), "cd", 1, aem_ss_cstr(""));

	test_stringslice_match(aem_ss_cstr("ab"), "cd", 0, aem_ss_cstr("ab"));
	test_stringslice_match_end(aem_ss_cstr("cd"), "ab", 0, aem_ss_cstr("cd"));

	test_stringslice_match(aem_ss_cstr("ab"), "ac", 0, aem_ss_cstr("ab"));
	test_stringslice_match_end(aem_ss_cstr("cd"), "bd", 0, aem_ss_cstr("cd"));

	test_stringslice_match(aem_ss_cstr("abcd"), "ab", 1, aem_ss_cstr("cd"));
	test_stringslice_match_end(aem_ss_cstr("abcd"), "cd", 1, aem_ss_cstr("ab"));


	aem_logf_ctx(AEM_LOG_NOTICE, "test aem_stringslice_match_line_multi");

	// Empty input
	test_stringslice_match_line_multi(aem_ss_cstr(""            ), 0, 0, AEM_STRINGSLICE_EMPTY, aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr(""            ), 1, 0, AEM_STRINGSLICE_EMPTY, aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr(""            ), 0, 1, AEM_STRINGSLICE_EMPTY, aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr(""            ), 1, 1, AEM_STRINGSLICE_EMPTY, aem_ss_cstr(""      ));

	// No newline
	test_stringslice_match_line_multi(aem_ss_cstr("line"        ), 0, 0, AEM_STRINGSLICE_EMPTY, aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line"        ), 1, 0, AEM_STRINGSLICE_EMPTY, aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line"        ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line"        ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	// Just newline
	test_stringslice_match_line_multi(aem_ss_cstr("\n"          ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n"          ), 1, 0, AEM_STRINGSLICE_EMPTY, aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n"          ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n"          ), 1, 1, AEM_STRINGSLICE_EMPTY, aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\r"          ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r"          ), 1, 0, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r"          ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r"          ), 1, 1, aem_ss_cstr(""      ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\r\n"        ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r\n"        ), 1, 0, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r\n"        ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r\n"        ), 1, 1, aem_ss_cstr(""      ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\n\r"        ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr("\r"    ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n\r"        ), 1, 0, aem_ss_cstr(""      ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n\r"        ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr("\r"    ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n\r"        ), 1, 1, aem_ss_cstr(""      ), aem_ss_cstr(""      ));

	// Trailing newline
	test_stringslice_match_line_multi(aem_ss_cstr("line\n"      ), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n"      ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n"      ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n"      ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("line\r"      ), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r"      ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r"      ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r"      ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("line\r\n"    ), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r\n"    ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r\n"    ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r\n"    ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("line\n\r"    ), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr("\r"    ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n\r"    ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr("\r"    ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n\r"    ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr("\r"    ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n\r"    ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr("\r"    ));

	// Leading newline
	test_stringslice_match_line_multi(aem_ss_cstr("\nline"      ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline"      ), 1, 0, AEM_STRINGSLICE_EMPTY, aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline"      ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline"      ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\rline"      ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\rline"      ), 1, 0, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\rline"      ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\rline"      ), 1, 1, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));

	test_stringslice_match_line_multi(aem_ss_cstr("\r\nline"    ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r\nline"    ), 1, 0, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r\nline"    ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\r\nline"    ), 1, 1, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));

	test_stringslice_match_line_multi(aem_ss_cstr("\n\rline"    ), 0, 0, aem_ss_cstr(""      ), aem_ss_cstr("\rline"));
	test_stringslice_match_line_multi(aem_ss_cstr("\n\rline"    ), 1, 0, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("\n\rline"    ), 0, 1, aem_ss_cstr(""      ), aem_ss_cstr("\rline"));
	test_stringslice_match_line_multi(aem_ss_cstr("\n\rline"    ), 1, 1, aem_ss_cstr(""      ), aem_ss_cstr("line"  ));

	// Central newline
	test_stringslice_match_line_multi(aem_ss_cstr("line\nLINE"  ), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\nLINE"  ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\nLINE"  ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\nLINE"  ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));

	test_stringslice_match_line_multi(aem_ss_cstr("line\rLINE"  ), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\rLINE"  ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\rLINE"  ), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\rLINE"  ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));

	test_stringslice_match_line_multi(aem_ss_cstr("line\r\nLINE"), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r\nLINE"), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r\nLINE"), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));
	test_stringslice_match_line_multi(aem_ss_cstr("line\r\nLINE"), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr("LINE"  ));

	test_stringslice_match_line_multi(aem_ss_cstr("line\n\rLINE"), 0, 0, aem_ss_cstr("line"  ), aem_ss_cstr("\rLINE"));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n\rLINE"), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr("\rLINE"));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n\rLINE"), 0, 1, aem_ss_cstr("line"  ), aem_ss_cstr("\rLINE"));
	test_stringslice_match_line_multi(aem_ss_cstr("line\n\rLINE"), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr("\rLINE"));

	// Surrounding newlines
	test_stringslice_match_line_multi(aem_ss_cstr("\nline\n"    ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline\n"    ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\nline\r"    ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline\r"    ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\nline\r\n"  ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline\r\n"  ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr(""      ));

	test_stringslice_match_line_multi(aem_ss_cstr("\nline\n\r"  ), 1, 0, aem_ss_cstr("line"  ), aem_ss_cstr("\r"    ));
	test_stringslice_match_line_multi(aem_ss_cstr("\nline\n\r"  ), 1, 1, aem_ss_cstr("line"  ), aem_ss_cstr("\r"    ));


	return show_test_results();
}
