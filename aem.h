#ifndef AEM_H
#define AEM_H

#ifdef __GNUC__
#define AEM_CONFIG_HAVE_STMT_EXPR
#endif

#ifdef AEM_DEBUG
// Debug mode

#ifndef aem_break
#include "debugbreak.h"
#define aem_break() debug_break()
#endif

#ifndef aem_abort
#ifdef AEM_BREAK_ON_ABORT
#define aem_abort() aem_break()
#else
#define aem_abort() abort()
#endif
#endif

#else /* !defined(AEM_DEBUG) */

#ifndef aem_abort
#if !defined(AEM_RECKLESS)
#define aem_abort() abort()
#elif defined(__GNUC__)
#define aem_abort() __builtin_unreachable()
#else
// You're crazy!
#define aem_abort() do {} while (0)
#endif
#endif

#ifndef aem_break
#define aem_break() aem_abort()
#endif

#endif /* AEM_DEBUG */

#ifdef __GNUC__
#define aem_deprecated __attribute__((deprecated))
#define aem_deprecated_msg(msg) __attribute__((deprecated(msg)))
#else
#define aem_deprecated
#define aem_deprecated_msg(msg)
#endif

#define AEM_STRINGIFY(s) #s
#define AEM_STRINGIFY2(s) AEM_STRINGIFY(s)
#define AEM_STATIC_ASSERT(cond) ((void)sizeof(struct{int:-!(cond);}))

#ifdef _WIN32
#define __thread __declspec(thread)
#endif

#endif /* AEM_H */
