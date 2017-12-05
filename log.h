#ifndef AEM_LOG_H
#define AEM_LOG_H

#include <stdarg.h>
#include <stdio.h>

enum aem_log_level
{
	AEM_LOG_FATAL,
	AEM_LOG_BUG,
	AEM_LOG_SECURITY,
	AEM_LOG_ERROR,
	AEM_LOG_WARN,
	AEM_LOG_NOTICE,
	AEM_LOG_INFO,
	AEM_LOG_DEBUG,
};

extern enum aem_log_level aem_log_level_curr;

extern FILE *aem_log_fp;

int aem_logf(enum aem_log_level loglevel, const char *fmt, ...);
int aem_dprintf(const char *fmt, ...);
int aem_vdprintf(const char *fmt, va_list ap);

const char *aem_log_level_describe(enum aem_log_level loglevel);
char aem_log_level_letter(enum aem_log_level loglevel);

#if AEM_LOGF_LOGLEVEL_WORD
#define aem_logf_ctx(loglevel, fmt, ...) aem_logf((loglevel), "%s:%d(%s): %s: " fmt, __FILE__, __LINE__, __func__, aem_log_level_describe(loglevel), ##__VA_ARGS__)
#else
#define aem_logf_ctx(loglevel, fmt, ...) aem_logf((loglevel), "%c %s:%d(%s): " fmt, aem_log_level_letter(loglevel), __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif

#ifndef aem_assert
#define aem_assert(condition) if (condition) { aem_logf_ctx("assertion failed: %s", #condition); abort(); }
#endif

#endif /* AEM_LOG_H */
