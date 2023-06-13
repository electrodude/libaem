#ifndef AEM_TEST_COMMON_H
#define AEM_TEST_COMMON_H

#include <errno.h>
#include <string.h>
#include <time.h>

#include <aem/log.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

// TODO: This macro belongs in stringslice.h
#define aem_ss_cstr aem_stringslice_new_cstr

extern struct aem_log_module test_log_module;
#undef aem_log_module_current
#define aem_log_module_current (&test_log_module)

extern int tests_count;
extern int tests_failed;

int ss_eq(struct aem_stringslice s1, struct aem_stringslice s2);
void debug_slice(struct aem_stringbuf *out, struct aem_stringslice in);

void test_init(int argc, char **argv);

#define TEST_EXPECT(err_str, ok) if (ok) { tests_count++; } else for (struct aem_stringbuf *err_str = aem_log_header(&aem_log_buf, AEM_LOG_BUG); err_str ? tests_count++, tests_failed++, aem_stringbuf_printf(out, "Test %zd failed: ", tests_count), 1 : 0; aem_log_submit(&test_log_module, err_str), err_str = NULL)

int show_test_results_impl(const char *file, int line, const char *func);
#define show_test_results() show_test_results_impl(__FILE__, __LINE__, __func__)

void tic(struct timespec *t_start);
void toc(struct timespec t_start);

#endif /* AEM_TEST_COMMON_H */
