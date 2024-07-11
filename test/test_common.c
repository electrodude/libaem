#define _POSIX_C_SOURCE 199309L

#include <unistd.h>

#include <aem/ansi-term.h>
#include <aem/translate.h>

#include "test_common.h"

struct aem_log_module test_log_module = {.name = "test", .loglevel = AEM_LOG_NOTICE};

int tests_count = 0;
int tests_failed = 0;
int test_bugs = 0;    // AEM_LOG_BUG or AEM_LOG_NYI
int test_errors = 0;  // Unexpected errors
int test_xerrors = 0; // Expected error countdown

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

static struct aem_log_dest *log_dest_orig;
struct aem_stringbuf test_log_buf = {0};
static void test_log_cb(struct aem_log_dest *dst, struct aem_log_module *mod, enum aem_log_level level, struct aem_stringslice msg)
{
	aem_stringbuf_reset(&test_log_buf);

	if (level == AEM_LOG_BUG || level == AEM_LOG_NYI) {
		test_bugs++;
	} else if (level <= AEM_LOG_ERROR) {
		if (test_xerrors) {
			test_xerrors--;
			// dim
			aem_stringbuf_puts(&test_log_buf, AEM_SGR("2") "(xfail) ");
		} else {
			test_errors++;
		}
	}

	aem_assert(dst != log_dest_orig);
	// Strip color if test dst wants it and actual one doesn't
	if ((dst->flags & AEM_LOG_FLAG_COLOR) && !(log_dest_orig->flags & AEM_LOG_FLAG_COLOR))
		aem_ansi_strip(&test_log_buf, msg);
	else
		aem_stringbuf_putss(&test_log_buf, msg);
	log_dest_orig->log(log_dest_orig, mod, level, aem_stringslice_new_str(&test_log_buf));
}
struct aem_log_dest test_log_dest = {
	.log = test_log_cb,
	.flags = AEM_LOG_FLAG_COLOR,
};

void test_init(int argc, char **argv)
{
	(void)argc; (void)argv;
	aem_log_stderr();

	aem_log_module_default.loglevel = AEM_LOG_GOOD;
	aem_log_module_default_internal.loglevel = AEM_LOG_NOTICE;

	// TODO: Parse at least -v and -l

	aem_logf_ctx(AEM_LOG_DEBUG, "Installing test log dest");
	// Save original log destination
	log_dest_orig = aem_log_default;
	// Route all logs through test log destination
	aem_log_module_default_internal.dst =
	aem_log_module_default.dst          =
	test_log_module.dst                 =
	aem_log_default                     = &test_log_dest;
}

int show_test_results_impl(const char *file, int line, const char *func)
{
	enum {
		NONE,
		PASS,
		FAIL,
	} result = !tests_count  ? NONE
	         :  tests_failed ? FAIL
	                         : PASS;

	enum aem_log_level level = result == PASS ? AEM_LOG_GOOD
		                 : result == FAIL ? AEM_LOG_ERROR
		                                  : AEM_LOG_BUG;

	AEM_LOG_MULTI_BUF_MOD_IMPL(str, &aem_log_buf, aem_log_module_current, level, file, line, func) {
		switch (result) {
			case NONE: aem_stringbuf_printf(str, "No tests!"); break;
			case PASS: aem_stringbuf_printf(str, "All %zd test%s passed", tests_count, tests_count != 1 ? "s" : ""); break;
			default  : aem_stringbuf_printf(str, "%zd/%zd test%s failed!", tests_failed, tests_count, tests_count != 1 ? "s" : ""); break;
		}
		if (test_bugs)
			aem_stringbuf_printf(str, " %s(%zd bugs/NYIs)" AEM_SGR("0"), aem_log_level_color(AEM_LOG_BUG), test_bugs);
		if (test_errors)
			aem_stringbuf_printf(str, " %s(%zd unexpected errors)" AEM_SGR("0"), aem_log_level_color(AEM_LOG_ERROR), test_errors);
	}

	// Make valgrind happy
	aem_stringbuf_dtor(&aem_log_buf);
	aem_stringbuf_dtor(&test_log_buf);

	if (!tests_count)
		return -1;

	return tests_failed;
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
