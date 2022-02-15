#ifndef AEM_NFA_UTIL_H
#define AEM_NFA_UTIL_H

#include <stdint.h>

#ifndef AEM_INTERNAL
#warning This is a private header; do not include it yourself.
#endif

struct aem_stringbuf;

void aem_nfa_desc_char(struct aem_stringbuf *out, uint32_t c);
void aem_nfa_desc_range(struct aem_stringbuf *out, uint32_t lo, uint32_t hi);

#endif /* AEM_NFA_UTIL_H */
