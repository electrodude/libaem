#ifndef AEM_NFA_UTIL_H
#define AEM_NFA_UTIL_H

#include <stdint.h>

// Private header; do not include this yourself.

struct aem_stringbuf;

void aem_nfa_desc_char(struct aem_stringbuf *out, uint32_t c);
void aem_nfa_desc_range(struct aem_stringbuf *out, uint32_t lo, uint32_t hi);

#endif /* AEM_NFA_UTIL_H */
