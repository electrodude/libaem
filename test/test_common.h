#ifndef AEM_TEST_COMMON_H
#define AEM_TEST_COMMON_H

#include <errno.h>
#include <string.h>
#include <time.h>

#include <aem/log.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>


extern struct aem_log_module test_log_module;
#undef aem_log_module_current
#define aem_log_module_current (&test_log_module)


/// Utilities
// TODO: This macro belongs in stringslice.h
#define aem_ss_cstr aem_stringslice_new_cstr
int ss_eq(struct aem_stringslice s1, struct aem_stringslice s2);
void debug_slice(struct aem_stringbuf *out, struct aem_stringslice in);


/// Timing
void tic(struct timespec *t_start);
void toc(struct timespec t_start);


/// Tests
struct test {
	// Callback
	int (*fn)(int argc, char **argv);

	// Context
	const char *name;
	const char *file;
	int line;

	// Results
	int count;   // Total tests
	int failed;  // Failed tests
	int bugs;    // AEM_LOG_BUG or AEM_LOG_NYI
	int errors;  // Unexpected errors
};
extern struct test test_total;

extern int test_xerrors; // Expected error countdown

// Start/stop per-test counters
void test_start(struct test *test);
void test_end(struct test *test);

// Show test results
int test_show_results(struct test *test, int test_rc);

#define TEST_EXPECT(err_str, ok) if (ok) { test_total.count++; } else for (struct aem_stringbuf *err_str = aem_log_header(&aem_log_buf, AEM_LOG_BUG); err_str ? test_total.count++, test_total.failed++, aem_stringbuf_printf(out, "Test %zd failed: ", test_total.count), 1 : 0; aem_log_submit(&test_log_module, AEM_LOG_BUG, err_str), test_total.bugs--, err_str = NULL)


/// Test infrastructure
int test_init(int *argc_p, char ***argv_p);

// Test table (last element must be {0})
extern struct test tests[];
#define TESTS __attribute__((weak)) struct test tests[] =
#define TEST(_func) {.name = AEM_STRINGIFY(_func), .file = __FILE__, .line = __LINE__, .fn = (_func)}
#define TEST_MAIN(_func) \
	int _func(int argc, char **argv); \
	TESTS { TEST(_func), {0}, }; \
	int _func

#endif /* AEM_TEST_COMMON_H */
