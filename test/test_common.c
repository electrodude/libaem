#include <aem/translate.h>

#include "test_common.h"

int test_errors = 0;

int ss_eq(struct aem_stringslice s1, struct aem_stringslice s2)
{
	if (s1.start && s1.end && s1.start && s2.end) {
		return !aem_stringslice_cmp(s1, s2);
	} else {
		return s1.start == s2.start && s1.end == s2.end;
	}
}
void debug_slice(struct aem_stringbuf *out, struct aem_stringslice in)
{
	aem_assert(out);
	if (in.start && in.end) {
		aem_stringbuf_puts(out, "\"");
		aem_string_escape(out, in);
		aem_stringbuf_puts(out, "\"");
	} else {
		aem_stringbuf_puts(out, "(null)");
	}
}

int show_test_results_impl(const char *file, int line, const char *func)
{
	int loglevel = test_errors ? AEM_LOG_ERROR : AEM_LOG_NOTICE;
	struct aem_stringbuf *str = aem_log_header_mod_impl(&aem_log_buf, aem_log_module_current, loglevel, file, line, func);

	if (!test_errors)
		aem_stringbuf_puts(str, "Tests succeeded");
	else
		aem_stringbuf_printf(str, "%zd test%s failed!", test_errors, test_errors != 1 ? "s" : "");

	aem_stringbuf_putc(str, '\n');
	aem_log_str(str);

	return test_errors;
}
