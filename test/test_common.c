#define _POSIX_C_SOURCE 199309L

#include <unistd.h>

#include <aem/ansi-term.h>
#include <aem/translate.h>

#include "test_common.h"

struct aem_log_module test_log_module = {.name = "test", .loglevel = AEM_LOG_NOTICE};


/// Utilities
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


/// Tests
struct test test_total = {
	.name = "test_total",
	.file = __FILE__,
};

void test_start(struct test *test)
{
	test->count -= test_total.count;
	test->failed -= test_total.failed;
	test->bugs -= test_total.bugs;
	test->errors -= test_total.errors;
}

void test_end(struct test *test)
{
	// Copy into per-test counters
	test->count += test_total.count;
	test->failed += test_total.failed;
	test->bugs += test_total.bugs;
	test->errors += test_total.errors;
}

int test_show_results(struct test *test)
{
	// Report results
	enum aem_log_level level = test->rc     ? AEM_LOG_FATAL
		                 : !test->count ? AEM_LOG_BUG
	                         : test->failed ? AEM_LOG_ERROR
	                                        : AEM_LOG_GOOD;

	AEM_LOG_MULTI_BUF_MOD_IMPL(str, &aem_log_buf, aem_log_module_current, level, test->name, test->line, test->file) {
		if (!test->count) {
			aem_stringbuf_printf(str, "No tests!");
		} else if (test->failed) {
			aem_stringbuf_printf(str, "%zd/%zd test%s failed!", test->failed, test->count, test->count != 1 ? "s" : "");
		} else {
			aem_stringbuf_printf(str, "All %zd test%s passed", test->count, test->count != 1 ? "s" : "");
		}
		if (test->rc)
			aem_stringbuf_printf(str, " %s(Test returned %d)" AEM_SGR("0"), aem_log_level_color(AEM_LOG_FATAL),  test->rc);
		if (test->bugs)
			aem_stringbuf_printf(str, " %s(%zd bugs/NYIs)" AEM_SGR("0"), aem_log_level_color(AEM_LOG_BUG), test->bugs);
		if (test->errors)
			aem_stringbuf_printf(str, " %s(%zd unexpected errors)" AEM_SGR("0"), aem_log_level_color(AEM_LOG_ERROR), test->errors);
	}

	if (!test->count)
		return -1;

	return test->failed != 0;
}

int test_xerrors = 0; // Expected error countdown


/// Test infrastructure
static struct aem_log_dest *log_dest_orig;
struct aem_stringbuf test_log_buf = {0};
static void test_log_cb(struct aem_log_dest *dst, struct aem_log_module *mod, enum aem_log_level level, struct aem_stringslice msg)
{
	aem_stringbuf_reset(&test_log_buf);

	if (level == AEM_LOG_BUG || level == AEM_LOG_NYI) {
		test_total.bugs++;
	} else if (level <= AEM_LOG_ERROR) {
		if (test_xerrors) {
			test_xerrors--;
			// dim
			aem_stringbuf_puts(&test_log_buf, AEM_SGR("2") "(xfail) ");
		} else {
			test_total.errors++;
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

static void usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s [<options>]\n", cmd);
	fprintf(stderr, "   %-20s%s\n", "[-l<logfile>]", "set log file");
	fprintf(stderr, "   %-20s%s\n", "[-v<loglevel>]", "set log level (default: debug)");
	fprintf(stderr, "   %-20s%s\n", "[-h]", "show this help");
}
int test_init(int *argc_p, char ***argv_p)
{
	aem_log_stderr();

	aem_log_module_default.loglevel = AEM_LOG_GOOD;
	aem_log_module_default_internal.loglevel = AEM_LOG_NOTICE;

	// Parse arguments, if provided.
	if (argc_p || argv_p) {
		aem_assert(argc_p);
		aem_assert(argv_p);
		int argc = *argc_p;
		char **argv = *argv_p;
		int opt;
		while ((opt = getopt(argc, argv, "l:v:h")) != -1) {
			switch (opt) {
				case 'l': aem_log_fopen(optarg); break;
				case 'v': aem_log_level_parse_set(optarg); break;
				case 'h':
				default:
					usage(argv[0]);
					return 1;
			}
		}
		argc -= optind;
		argv += optind;

		*argc_p = argc;
		*argv_p = argv;
	}

	aem_logf_ctx(AEM_LOG_DEBUG, "Installing test log dest");
	// Save original log destination
	log_dest_orig = aem_log_default;
	// Route all logs through test log destination
	aem_log_module_default_internal.dst =
	aem_log_module_default.dst          =
	test_log_module.dst                 =
	aem_log_default                     = &test_log_dest;

	return 0;
}

__attribute__((weak)) int main(int argc, char **argv)
{
	// Initialize
	int rc = test_init(&argc, &argv);
	if (rc)
		return rc;

	test_total.name = __func__;
	test_total.file = __FILE__;
	test_total.line = __LINE__;

	// Run tests
	for (struct test *test = tests; test->fn; test++) {
		test->argc = argc;
		test->argv = argv;
		test_start(test);
		test->rc = test->fn(test);
		if (test->rc) {
			aem_logf_ctx(AEM_LOG_FATAL, "%s failed: %d", test->name, test->rc);
			rc = test->rc;
		}
		test_end(test);
		test_show_results(test);
		if (test->rc)
			rc = test->rc;
	}

	// Report overall results
	test_total.rc = rc;
	rc = test_show_results(&test_total);

	// Make valgrind happy
	aem_stringbuf_dtor(&aem_log_buf);
	aem_stringbuf_dtor(&test_log_buf);

	return rc;
}
