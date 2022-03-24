#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

#include <aem/nfa.h>
#include <aem/regex.h>
#include <aem/translate.h>

static void test_regex_compile(struct aem_nfa *nfa, const char *pattern, unsigned int match, int rc_expect)
{
	aem_logf_ctx(AEM_LOG_INFO, "regex_compile(\"%s\", %d) expect (%d)", pattern, match, rc_expect);

	struct aem_stringslice in = aem_stringslice_new_cstr(pattern);
	int rc = aem_nfa_add_regex(nfa, in, match, aem_stringslice_new_cstr("d"));

	if (rc != rc_expect) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "regex_compile(\"%s\", %d) returned (%d), expected (%d)!", pattern, match, rc, rc_expect);
	}
}

static void test_nfa_run(struct aem_nfa *nfa, const char *input, int rc_expect, const char *input_remain)
{
	aem_logf_ctx(AEM_LOG_INFO, "nfa_run(\"%s\") expect (%d, \"%s\")", input, rc_expect, input_remain);

	struct aem_stringslice in = aem_stringslice_new_cstr(input);

	// Make sure testcase is sane
	{
		struct aem_stringslice in2 = in;
		if (!aem_stringslice_match_suffix(&in2, aem_stringslice_new_cstr(input_remain)))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: remaining input is not a suffix of original input");
	}

	struct aem_nfa_match match = {0};
	int rc = aem_nfa_run(nfa, &in, &match);
#if AEM_NFA_CAPTURES
	if (match.captures) {
		AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
			aem_stringbuf_puts(out, "Captures:");
			int first = 1;
			for (size_t i = 0; i < nfa->n_captures; i++) {
#if 0
				if (!aem_stringslice_ok(match.captures[i]))
					continue;
#endif
				if (!first)
					aem_stringbuf_puts(out, ";");
				aem_stringbuf_printf(out, " %zd: \"", i);
				aem_string_escape(out, match.captures[i]);
				aem_stringbuf_puts(out, "\"");
				first = 0;
			}
		}
	}
	free(match.captures);
#endif
	if (match.visited) {
		//aem_nfa_show_trace(&run, thr_matched);
#if 0
		AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
			aem_stringbuf_puts(out, "Trace:");
			aem_nfa_disas(out, nfa, match.visited);
		}
#endif
	}
	free(match.visited);

	int input_match = aem_stringslice_eq(in, input_remain);

	if (rc != rc_expect || !input_match) {
		test_errors++;
		AEM_LOG_MULTI(out, AEM_LOG_BUG) {
			aem_stringbuf_puts(out, "nfa_run(\"");
			aem_string_escape(out, aem_stringslice_new_cstr(input));
			aem_stringbuf_printf(out, "\") returned (%d, \"", rc);
			aem_string_escape(out, in);
			aem_stringbuf_printf(out, "\"), expected (%d, \"", rc_expect);
			aem_string_escape(out, aem_stringslice_new_cstr(input_remain));
			aem_stringbuf_puts(out, "\")!");
		}
	}
}

int main(int argc, char **argv)
{
	test_log_module.loglevel = AEM_LOG_DEBUG;
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	test_init(argc, argv);

	aem_logf_ctx(AEM_LOG_NOTICE, "init");

	struct aem_nfa nfa = AEM_NFA_EMPTY;

	aem_logf_ctx(AEM_LOG_NOTICE, "construct nfa");

	test_regex_compile(&nfa, "[b\\0a]([a-fP-Z]{6})", 1, 0);
	test_regex_compile(&nfa, "[^d\\0-a\\x63]([a-fP-Z]{6})", 1, 0);
	test_regex_compile(&nfa, "[^ba\\U000fffff-\\U00ffffff]((:[a-fP-Z]{3}){2})$", 1, 0);
	//test_regex_compile(&nfa, "", 2, 0);
	test_regex_compile(&nfa, "(chicken soup)", 2, 0);
	test_regex_compile(&nfa, "asdf", 3, 0);
	test_regex_compile(&nfa, ".+efg", 4, 0);
	test_regex_compile(&nfa, "a+a+b", 5, 0);

	struct aem_nfa nfa2 = {0};
	nfa2 = nfa;
	aem_nfa_dup(&nfa, &nfa2);

	// Make sure the VM doesn't get stuck on infinite loops without making progress.
	test_regex_compile(&nfa, "((()+))ignore", 6, 0);
	test_regex_compile(&nfa, "(()(?:)()(()))+ignore", 7, 0);
	test_regex_compile(&nfa, "(((()+)*)*)*ignore", 6, 0);
	test_regex_compile(&nfa, "(?c:((a.)+)((.b)+))", 8, 0);
	//test_regex_compile(&nfa, "[^-\\]_a-zA-Z0-9]+!", 9, 0);

	test_regex_compile(&nfa, "pfx(1|(2))*sf?x", 10, 0);
	test_regex_compile(&nfa, "\\w\\W([[:^lower:]]|\\d)+", 11, 0);
	test_regex_compile(&nfa, ".*\\<word\\>.*(\\<begin|end\\>)", 12, 0);

	// Test bounds and classes
	test_regex_compile(&nfa, "bound[[:alnum:]]{6}", 13, 0);
	test_regex_compile(&nfa, "bound[[:alpha:]]{7,}", 14, 0);
	test_regex_compile(&nfa, "bound[[:xdigit:]]{,8}", 15, 0);
	test_regex_compile(&nfa, "bound[[:lower:]]{5,9}", 16, 0);
	test_regex_compile(&nfa, "bound[[:digit:]]{,}", 17, 0);

	aem_nfa_optimize(&nfa);

	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		aem_stringbuf_printf(out, "NFA VM disassembly (%zd insns, %zd captures):\n", nfa.n_insns, nfa.n_captures);
		aem_nfa_disas(out, &nfa, nfa.thr_init);
	}

	aem_logf_ctx(AEM_LOG_NOTICE, "run nfa");

	test_nfa_run(&nfa, "chicklet", -1, "chicklet");
	test_nfa_run(&nfa, " :eUf:VcQ", 1, "");
	test_nfa_run(&nfa, "chicken soup", 2, "");
	test_nfa_run(&nfa, "chicken souq", -1, "chicken souq");
	test_nfa_run(&nfa, "asdf", 3, "");
	test_nfa_run(&nfa, "abcdefg", 4, "");
	test_nfa_run(&nfa, "aaaaaaaaaabZ", 5, "Z");
	test_nfa_run(&nfa, "abZ", 11, "");
	test_nfa_run(&nfa, "asaaaabbab.bb", 8, "b");
	test_nfa_run(&nfa, "asaaaaabab.bb", 8, "b");

	test_nfa_run(&nfa, "pfx1sfx", 10, "");
	test_nfa_run(&nfa, "pfx2sx", 10, "");

	test_nfa_run(&nfa, " word0begin0", -1, " word0begin0");
	test_nfa_run(&nfa, " word0end0", -1, " word0end0");
	test_nfa_run(&nfa, " word0begin", -1, " word0begin");
	test_nfa_run(&nfa, " word0endd", -1, " word0endd");
	test_nfa_run(&nfa, " word endd", -1, " word endd");
	test_nfa_run(&nfa, "word begin", 12, "");
	test_nfa_run(&nfa, "word 0end", 12, "");
	test_nfa_run(&nfa, " word begin", 12, "");
	test_nfa_run(&nfa, "word 0end ", 12, " ");

	test_nfa_run(&nfa, "bound0Xcvbn", 13, "");
	test_nfa_run(&nfa, "boundAbcdEfg", 14, "");
	test_nfa_run(&nfa, "bound012abcde", 15, "");
	test_nfa_run(&nfa, "boundabcdef", 16, "");
	test_nfa_run(&nfa, "bound0123456", 17, "");

	// TODO: Unicode tests


	aem_logf_ctx(AEM_LOG_NOTICE, "dtor");

	aem_nfa_dtor(&nfa);
	aem_nfa_dtor(&nfa2);

	return show_test_results();
}
