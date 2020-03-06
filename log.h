#ifndef AEM_LOG_H
#define AEM_LOG_H

#include <stdarg.h>
#include <stdio.h>

enum aem_log_level {
	AEM_LOG_FATAL,
	AEM_LOG_BUG,
	AEM_LOG_SECURITY,
	AEM_LOG_ERROR,
	AEM_LOG_WARN,
	AEM_LOG_NOTICE,
	AEM_LOG_INFO,
	AEM_LOG_DEBUG,
};


// log file

extern FILE *aem_log_fp; // public use is deprecated; use aem_log_{set,open,get}

FILE *aem_log_fset(FILE *fp_new, int autoclose_new);

static inline FILE *aem_log_stderr(void)
{
	return aem_log_fset(stderr, 0);
}
FILE *aem_log_fopen(const char *path_new);
FILE *aem_log_fget(void);


// log level

extern enum aem_log_level aem_log_level_curr;

const char *aem_log_level_describe(enum aem_log_level loglevel);
char aem_log_level_letter(enum aem_log_level loglevel);

enum aem_log_level aem_log_level_parse(const char *p);
static inline enum aem_log_level aem_log_level_parse_set(const char *p)
{
	return aem_log_level_curr = aem_log_level_parse(p);
}


// logging

int aem_logf(enum aem_log_level loglevel, const char *fmt, ...);
int aem_dprintf(const char *fmt, ...);
int aem_vdprintf(const char *fmt, va_list ap);

#if AEM_LOGF_LOGLEVEL_WORD
#define aem_logf_ctx(loglevel, fmt, ...) aem_logf((loglevel), "%s:%d(%s): %s: " fmt, __FILE__, __LINE__, __func__, aem_log_level_describe(loglevel), ##__VA_ARGS__)
#else
#define aem_logf_ctx(loglevel, fmt, ...) aem_logf((loglevel), "%c %s:%d(%s): " fmt, aem_log_level_letter(loglevel), __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif

#ifndef aem_assert
#define aem_assert(condition) if (!(condition)) { aem_logf_ctx(AEM_LOG_BUG, "assertion failed: %s\n", #condition); abort(); }
#endif

#endif /* AEM_LOG_H */
