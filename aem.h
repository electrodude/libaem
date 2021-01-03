#ifndef AEM_H
#define AEM_H

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

#else /* !AEM_DEBUG */

#ifndef aem_abort
#define aem_abort() abort()
#endif

#ifndef aem_break
#define aem_break() aem_abort()
#endif

#endif /* AEM_DEBUG */

#endif /* AEM_H */