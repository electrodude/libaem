#ifndef AEM_REGEX_H
#define AEM_REGEX_H

#include <aem/nfa.h>

/// Regex parser
int aem_regex_compile(struct aem_nfa *nfa, struct aem_stringslice re, unsigned int match);

// These functions both allocate a new match number for you.
// Returns match number on success, or <0 on failure.
int aem_nfa_add_regex(struct aem_nfa *nfa, struct aem_stringslice re);
int aem_nfa_add_string(struct aem_nfa *nfa, struct aem_stringslice str);

#endif /* AEM_REGEX_H */
