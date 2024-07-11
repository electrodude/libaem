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
extern int tests_count;
extern int tests_failed;
extern int test_bugs;    // AEM_LOG_BUG or AEM_LOG_NYI
extern int test_errors;  // Unexpected errors
extern int test_xerrors; // Expected error countdown

#define TEST_EXPECT(err_str, ok) if (ok) { tests_count++; } else for (struct aem_stringbuf *err_str = aem_log_header(&aem_log_buf, AEM_LOG_BUG); err_str ? tests_count++, tests_failed++, aem_stringbuf_printf(out, "Test %zd failed: ", tests_count), 1 : 0; aem_log_submit(&test_log_module, AEM_LOG_BUG, err_str), test_bugs--, err_str = NULL)


/// Test infrastructure
int test_main(int argc, char **argv);
extern const char *test_name;
#define TEST_MAIN(argc, argv) \
	const char *test_name = __FILE__; \
	int test_main(argc, argv)

#endif /* AEM_TEST_COMMON_H */
