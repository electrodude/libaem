#ifndef AEM_REGEX_H
#define AEM_REGEX_H

#include <aem/nfa.h>

/// Regex parser
int aem_regex_parse(struct aem_nfa *nfa, struct aem_stringslice re, unsigned int match);

#endif /* AEM_REGEX_H */
