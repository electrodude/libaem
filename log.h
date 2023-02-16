#ifndef AEM_LOG_H
#define AEM_LOG_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <aem/aem.h>

// You must set a logfile (i.e. call aem_log_fset, aem_log_fopen, or aem_log_stderr) before calling any logging functions.

enum aem_log_level {
	AEM_LOG_INVALID = -1,
	AEM_LOG_FATAL,    // Fatal error: program execution must cease as a result.
	AEM_LOG_SECURITY, // Security error: Occurrence of security error
	AEM_LOG_BUG,      // Non-fatal bug: Occurrence of something which should never happen, but program execution may continue.
	AEM_LOG_NYI,      // Use of unimplemented feature
	AEM_LOG_ERROR,    // Non-fatal error
	AEM_LOG_WARN,     // Warning: Probable misconfiguration, past or future errors likely, performance is bad, etc.
	AEM_LOG_GOOD,     // Success/Good: Test passed, initialization completed, etc.
	AEM_LOG_NOTICE,   // Noteworthy event
	AEM_LOG_INFO,     // Misc. information
	AEM_LOG_DEBUG,    // Debug info: Useless or trivial during normal execution
	AEM_LOG_DEBUG2,   // Fine debug info: Painful levels of detail
	AEM_LOG_DEBUG3,   // Very fine debug info: Probably drowns out all normal messages during normal execution
};

struct aem_log_module {
	enum aem_log_level loglevel;
	const char *name;
};


/// Log to FILE

extern int aem_log_color;

FILE *aem_log_fset(FILE *fp_new, int autoclose_new);
FILE *aem_log_fopen(const char *path_new);
FILE *aem_log_fget(void);

static inline FILE *aem_log_stderr(void)
{
	return aem_log_fset(stderr, 0);
}


/// Log level

extern struct aem_log_module aem_log_module_default;
extern struct aem_log_module aem_log_module_default_internal;
#ifndef aem_log_module_current
#ifdef AEM_INTERNAL
#define aem_log_module_current (&aem_log_module_default_internal)
#else
#define aem_log_module_current (&aem_log_module_default)
#endif
#endif

const char *aem_log_level_color(enum aem_log_level loglevel);
const char *aem_log_level_describe(enum aem_log_level loglevel);
char aem_log_level_letter(enum aem_log_level loglevel);
enum aem_log_level aem_log_level_parse_letter(char c);

struct aem_stringslice;
enum aem_log_level aem_log_level_parse(struct aem_stringslice word);
void aem_log_level_parse_set(const char *p);


/// Logging functions
struct aem_stringbuf;
// TODO: This isn't destructed on thread exit
extern __thread struct aem_stringbuf aem_log_buf;

struct aem_stringbuf *aem_log_header_mod_impl(struct aem_stringbuf *str, struct aem_log_module *module, enum aem_log_level loglevel, const char *file, int line, const char *func);
#define aem_log_header_mod(str, module, loglevel) aem_log_header_mod_impl((str), (module), (loglevel), __FILE__, __LINE__, __func__)
#define aem_log_header(str, loglevel) aem_log_header_mod((str), (aem_log_module_current), (loglevel))

void aem_log_multi_impl(struct aem_stringbuf *str);
#define AEM_LOG_MULTI_BUF(str, buf, loglevel) for (struct aem_stringbuf *str = aem_log_header((buf), (loglevel)); str; aem_log_multi_impl(str), str = NULL)
#define AEM_LOG_MULTI(str, loglevel) AEM_LOG_MULTI_BUF(str, &aem_log_buf, loglevel)

int aem_log_str(struct aem_stringbuf *str);

int aem_logmf_ctx_impl(struct aem_log_module *module, enum aem_log_level loglevel, const char *file, int line, const char *func, const char *fmt, ...);
#define aem_logmf_ctx(module, loglevel, fmt, ...) aem_logmf_ctx_impl((module), (loglevel), __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define aem_logf_ctx(loglevel, fmt, ...) aem_logmf_ctx((aem_log_module_current), (loglevel), fmt, ##__VA_ARGS__)

#define aem_logf_ctx_once(loglevel, ...) do { static int _hits = 0; if (!_hits) aem_logf_ctx((loglevel), ##__VA_ARGS__); _hits = 1; } while (0)

/// Assertions
#ifndef aem_assert
#ifndef AEM_SKIP_ASSERTS
#define aem_assert(condition) do { \
	if (!(condition)) { \
		aem_logf_ctx(AEM_LOG_BUG, "Assertion failed: %s", #condition); \
		aem_abort(); \
	} \
} while (0)
#else
#define aem_assert(condition) do { (void)(condition); } while (0)
#endif
#endif /* aem_assert */

#ifndef aem_assert_msg
#ifndef AEM_SKIP_ASSERTS
#define aem_assert_msg(condition, msg) do { \
	if (!(condition)) { \
		aem_logf_ctx(AEM_LOG_BUG, "Assertion failed: %s: %s", #condition, msg); \
		aem_abort(); \
	} \
} while (0)
#else
#define aem_assert_msg(condition, msg) do { (void)(condition); (void)(msg); } while (0)
#endif
#endif /* aem_assert_msg */

#ifndef aem_assert_eq
#ifndef AEM_SKIP_ASSERTS
#define aem_assert_eq(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	if (_a != _b) { \
		aem_logf_ctx(AEM_LOG_BUG, "Equality failed: %s != %s", #a, #b); \
		aem_abort(); \
	} \
	_a; \
})
#else
#define aem_assert_eq(a, b) ({ (void)(b); (a); })
#endif
#endif /* aem_assert_eq */

#ifdef AEM_DEBUG
#ifndef aem_unreachable
#define aem_unreachable() do { aem_logf_ctx(AEM_LOG_BUG, "Can't get here!"); aem_abort(); } while (0)
#endif
#endif

#endif /* AEM_LOG_H */
