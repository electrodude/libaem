#ifndef AEM_TEST_COMMON_H
#define AEM_TEST_COMMON_H

#include <time.h>

#include <aem/log.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

// TODO: This macro belongs in stringslice.h
#define aem_ss_cstr aem_stringslice_new_cstr

extern struct aem_log_module test_log_module;
#undef aem_log_module_current
#define aem_log_module_current (&test_log_module)

extern int test_errors;

int ss_eq(struct aem_stringslice s1, struct aem_stringslice s2);
void debug_slice(struct aem_stringbuf *out, struct aem_stringslice in);

int show_test_results_impl(const char *file, int line, const char *func);
#define show_test_results() show_test_results_impl(__FILE__, __LINE__, __func__)

void tic(struct timespec *t_start);
void toc(struct timespec t_start);

#endif /* AEM_TEST_COMMON_H */
