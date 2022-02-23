#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <unistd.h>

#include "test_common.h"

#include <aem/nfa.h>
#include <aem/regex.h>
#include <aem/translate.h>

static int test_add_regex(struct aem_nfa *nfa, const char *pattern)
{
	int rc_expect = 0;
	aem_logf_ctx(AEM_LOG_INFO, "add_regex(\"%s\") expect (%d)", pattern, rc_expect);

	struct aem_stringslice in = aem_stringslice_new_cstr(pattern);
	int rc = aem_nfa_add_regex(nfa, in, -1, AEM_REGEX_FLAG_DEBUG | AEM_REGEX_FLAG_BINARY);

	if (rc_expect < 0 ? rc != rc_expect : rc < 0) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "add_regex(\"%s\") returned (%d), expected (%d)!", pattern, rc, rc_expect);
		aem_assert(0);
		return -1;
	}

	return rc;
}

static int test_add_string(struct aem_nfa *nfa, const char *pattern)
{
	int rc_expect = 0;
	aem_logf_ctx(AEM_LOG_INFO, "add_string(\"%s\") expect (%d)", pattern, rc_expect);

	struct aem_stringslice in = aem_stringslice_new_cstr(pattern);
	int rc = aem_nfa_add_string(nfa, in, -1, AEM_REGEX_FLAG_DEBUG);

	if (rc_expect < 0 ? rc != rc_expect : rc < 0) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "add_string(\"%s\") returned (%d), expected (%d)!", pattern, rc, rc_expect);
		aem_assert(0);
		return -1;
	}

	return rc;
}

static void test_nfa_lex(struct aem_nfa *nfa, struct aem_stringslice input, struct aem_stringslice input_remain)
{
	AEM_LOG_MULTI(out, AEM_LOG_INFO) {
		aem_stringbuf_puts(out, "repeated nfa_run(\"");
		aem_string_escape(out, input);
		aem_stringbuf_printf(out, "\") expect (\"");
		aem_string_escape(out, input_remain);
		aem_stringbuf_puts(out, "\")");
	}

	// Make sure testcase is sane
	{
		struct aem_stringslice in2 = input;
		if (!aem_stringslice_match_suffix(&in2, input_remain))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: remaining input is not a suffix of original input");
	}

	struct aem_stringslice input_ret = input;
	int rc = 0;
	for (;;) {
		struct aem_stringslice text = input_ret;
		int tok = aem_nfa_run(nfa, &input_ret, NULL);
		text.end = input_ret.start;
		if (tok < 0) {
			rc = tok;
			break;
		}

		if (tok <= 0)
			continue;

		AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
			aem_stringbuf_printf(out, "tok %d: ", tok);
			//aem_string_escape(out, text);
			aem_stringbuf_putss(out, aem_stringslice_trim(text));
		}
	}

	if (rc == -2) {
		aem_logf_ctx(AEM_LOG_BUG, "NFA engine error!");
	}

	int input_match = !aem_stringslice_cmp(input_ret, input_remain);

	if (rc == -2 || !input_match) {
		test_errors++;
		AEM_LOG_MULTI(out, AEM_LOG_BUG) {
			aem_stringbuf_puts(out, "repeated nfa_run(\"");
			//aem_string_escape(out, input);
			aem_stringbuf_puts(out, "...");
			aem_stringbuf_printf(out, "\") returned (%d, \"", rc);
			aem_string_escape(out, input_ret);
			// TODO: 0 if we expected a token, -1 if we got all we were hoping for
			aem_stringbuf_printf(out, "\"), expected (%d, \"", -1);
			aem_string_escape(out, input_remain);
			aem_stringbuf_puts(out, "\")!");
		}
	}
}

void usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s [<options>]\n", cmd);
	fprintf(stderr, "   %-20s%s\n", "[-h]", "show this help");
	fprintf(stderr, "   %-20s%s\n", "[-v<loglevel>]", "set log level (default: debug)");
	fprintf(stderr, "   %-20s%s\n", "[-l<logfile>]", "set log file");
}
int main(int argc, char **argv)
{
	aem_log_stderr();
	test_log_module.loglevel = AEM_LOG_DEBUG;
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	const char *path = "../regex.c";

	int opt;
	while ((opt = getopt(argc, argv, "l:v:h")) != -1)
	{
		switch (opt)
		{
			case 'l': aem_log_fopen(optarg); break;
			case 'v': aem_log_level_parse_set(optarg); break;
			case 'h':
			default:
				usage(argv[0]);
				exit(1);
		}
	}

	argv += optind;
	argc -= optind;

	if (argc) {
		path = argv[0];
		argv++;
		argc--;
	}

	aem_logf_ctx(AEM_LOG_NOTICE, "init");

	struct aem_nfa nfa = AEM_NFA_EMPTY;

	aem_logf_ctx(AEM_LOG_NOTICE, "construct nfa");

	test_add_regex(&nfa, "\\n\\r?|\\r");
	test_add_regex(&nfa, "(\\s|\\\\$)+");
	test_add_regex(&nfa, "//.*$");
	test_add_regex(&nfa, "/\\*([^*]|\\*[^/]|\\n|\\r)*\\*/");
	test_add_regex(&nfa, "^\\s*#\\s*\\w+([^\\n\\r]|\\\\(\\n\\r?|\\r))*$");
	test_add_regex(&nfa, "(\\a|_)(\\w|_)*");
	test_add_regex(&nfa, "-?(\\d|_)*\\d(\\d|_)*");
	test_add_string(&nfa, "(");
	test_add_string(&nfa, ")");
	test_add_string(&nfa, "[");
	test_add_string(&nfa, "]");
	test_add_string(&nfa, "->");
	test_add_string(&nfa, ".");
	test_add_string(&nfa, "++");
	test_add_string(&nfa, "--");
	test_add_string(&nfa, "!");
	test_add_string(&nfa, "~");
	test_add_string(&nfa, "+");
	test_add_string(&nfa, "-");
	test_add_string(&nfa, "*");
	test_add_string(&nfa, "&");
	test_add_string(&nfa, "sizeof");
	test_add_string(&nfa, "/");
	test_add_string(&nfa, "%");
	test_add_string(&nfa, "<<");
	test_add_string(&nfa, ">>");
	test_add_string(&nfa, "<");
	test_add_string(&nfa, "<=");
	test_add_string(&nfa, ">");
	test_add_string(&nfa, ">=");
	test_add_string(&nfa, "==");
	test_add_string(&nfa, "!=");
	test_add_string(&nfa, "^");
	test_add_string(&nfa, "|");
	test_add_string(&nfa, "&&");
	test_add_string(&nfa, "||");
	test_add_string(&nfa, "?");
	test_add_string(&nfa, ":");
	test_add_string(&nfa, "=");
	test_add_string(&nfa, "+=");
	test_add_string(&nfa, "-=");
	test_add_string(&nfa, "*=");
	test_add_string(&nfa, "/=");
	test_add_string(&nfa, "%=");
	test_add_string(&nfa, "<<=");
	test_add_string(&nfa, ">>=");
	test_add_string(&nfa, "&=");
	test_add_string(&nfa, "^=");
	test_add_string(&nfa, "|=");
	test_add_string(&nfa, ",");
	test_add_string(&nfa, ";");
	test_add_string(&nfa, "{");
	test_add_string(&nfa, "}");
	test_add_regex(&nfa, "\"(([^\"\\n\\r]|\\\\.)*)\"");
	test_add_regex(&nfa, "'(([^\"\\n\\r]|\\\\.)*)'");

	aem_nfa_optimize(&nfa);

	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		aem_stringbuf_puts(out, "NFA VM disassembly:\n");
		aem_nfa_disas(out, &nfa, nfa.thr_init);
	}

	aem_logf_ctx(AEM_LOG_NOTICE, "run lexer");

	FILE *fp = fopen(path, "r");
	if (!fp) {
		aem_logf_ctx(AEM_LOG_FATAL, "couldn't open %s", path);
		return 1;
	}

	struct aem_stringbuf src = {0};
	aem_stringbuf_file_read_all(&src, fp);
	fclose(fp);

	test_nfa_lex(&nfa, aem_stringslice_new_str(&src), AEM_STRINGSLICE_EMPTY);

	aem_stringbuf_dtor(&src);

	aem_logf_ctx(AEM_LOG_NOTICE, "dtor");

	aem_nfa_dtor(&nfa);

	return show_test_results();
}
