#define _POSIX_C_SOURCE 199309L

#include <aem/translate.h>

#include "test_common.h"

struct aem_log_module test_log_module = {.name = "test", .loglevel = AEM_LOG_NOTICE};

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

void test_init(int argc, char **argv)
{
	(void)argc; (void)argv;
	aem_log_stderr();

	// TODO: Parse at least -v and -l
}

int show_test_results_impl(const char *file, int line, const char *func)
{
	int loglevel = test_errors ? AEM_LOG_ERROR : AEM_LOG_GOOD;

	struct aem_stringbuf *str = aem_log_header_mod_impl(&aem_log_buf, aem_log_module_current, loglevel, file, line, func);
	if (!str)
		return test_errors;

	if (!test_errors)
		aem_stringbuf_puts(str, "All tests passed");
	else
		aem_stringbuf_printf(str, "%zd test%s failed!", test_errors, test_errors != 1 ? "s" : "");

	aem_stringbuf_putc(str, '\n');
	aem_log_str(str);

	// Make valgrind happy
	aem_stringbuf_dtor(&aem_log_buf);

	return test_errors > 0;
}


/// Timing
void tic(struct timespec *t_start)
{
	aem_assert(t_start);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, t_start);
}
void toc(const struct timespec t_start)
{
	struct timespec t_end;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
	if (t_end.tv_nsec < t_start.tv_nsec) {
		t_end.tv_nsec += 1000000000;
		t_end.tv_sec -= 1;
	}
	int sec  = t_end.tv_sec - t_start.tv_sec;
	int nsec = t_end.tv_nsec - t_start.tv_nsec;
	aem_logf_ctx(AEM_LOG_NOTICE, "Took %d.%09d s", sec, nsec);
}
