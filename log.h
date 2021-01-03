#ifndef AEM_LOG_H
#define AEM_LOG_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <aem/aem.h>

enum aem_log_level {
	AEM_LOG_FATAL,    // Fatal error: program execution must cease as a result.
	AEM_LOG_SECURITY, // Security error: Occurrence of security error
	AEM_LOG_BUG,      // Non-fatal bug: Occurrence of something which should never happen, but program execution may continue.
	AEM_LOG_NYI,      // Use of unimplemented feature
	AEM_LOG_ERROR,    // Non-fatal error
	AEM_LOG_WARN,     // Warning: Probable misconfiguration, past or future errors likely, performance is bad, etc.
	AEM_LOG_NOTICE,   // Noteworthy event
	AEM_LOG_INFO,     // Misc. information
	AEM_LOG_DEBUG,    // Debug info: Useless or trivial during normal execution
	AEM_LOG_DEBUG2,   // Fine debug info: Painful levels of detail
	AEM_LOG_DEBUG3,   // Very fine debug info: Probably drowns out all normal messages during normal execution
};

struct aem_log_module
{
	enum aem_log_level loglevel;
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

extern struct aem_log_module aem_log_module_default;
#ifndef aem_log_module_current
#define aem_log_module_current (&aem_log_module_default)
#endif

const char *aem_log_level_describe(enum aem_log_level loglevel);
char aem_log_level_letter(enum aem_log_level loglevel);

enum aem_log_level aem_log_level_parse(const char *p);
static inline enum aem_log_level aem_log_level_parse_set(const char *p)
{
	return aem_log_module_default.loglevel = aem_log_level_parse(p);
}


// logging

int aem_logmf(struct aem_log_module *module, enum aem_log_level loglevel, const char *fmt, ...);
#define aem_logf(loglevel, fmt, ...) aem_logmf((aem_log_module_current), (loglevel), fmt, ##__VA_ARGS__)
int aem_dprintf(const char *fmt, ...);
int aem_vdprintf(const char *fmt, va_list ap);

#if AEM_LOGF_LOGLEVEL_WORD
#define aem_logf_ctx(loglevel, fmt, ...) aem_logf((loglevel), "%s:%d(%s): %s: " fmt, __FILE__, __LINE__, __func__, aem_log_level_describe(loglevel), ##__VA_ARGS__)
#define aem_logmf_ctx(module, loglevel, fmt, ...) aem_logf((module), (loglevel), "%s:%d(%s): %s: " fmt, __FILE__, __LINE__, __func__, aem_log_level_describe(loglevel), ##__VA_ARGS__)
#else
#define aem_logf_ctx(loglevel, fmt, ...) aem_logf((loglevel), "%c %s:%d(%s): " fmt, aem_log_level_letter(loglevel), __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define aem_logmf_ctx(module, loglevel, fmt, ...) aem_logmf((module), (loglevel), "%c %s:%d(%s): " fmt, aem_log_level_letter(loglevel), __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif

#ifndef aem_assert
#define aem_assert(condition) do { \
	if (!(condition)) { \
		aem_logf_ctx(AEM_LOG_BUG, "assertion failed: %s\n", #condition); \
		aem_abort(); \
	} \
} while (0)
#endif

#ifndef aem_assert_msg
#define aem_assert_msg(condition, msg) do { \
	if (!(condition)) { \
		aem_logf_ctx(AEM_LOG_BUG, "assertion failed: %s: %s\n", #condition, msg); \
		aem_abort(); \
	} \
} while (0)
#endif

#ifndef aem_assert_eq
#define aem_assert_eq(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	if (_a != _b) { \
		aem_logf_ctx(AEM_LOG_BUG, "equality failed: %s != %s\n", #a, #b); \
		aem_abort(); \
	} \
	_a; \
})
#endif

#define aem_logf_ctx_once(loglevel, fmt, ...) do { static int _hits = 0; if (!_hits++) aem_logf_ctx((loglevel), "once: " fmt, ##__VA_ARGS__); } while (0)

#endif /* AEM_LOG_H */
