#ifndef AEM_REGEX_H
#define AEM_REGEX_H

#include <limits.h>

#include <aem/nfa.h>

/// Regex parser
enum aem_regex_flags {
	// If enabled, input regex text memory must remain valid as long as NFA still has that debug info.
	// If disabled, optimizations are enabled that may confuse tracing.
	AEM_REGEX_FLAG_DEBUG = 0x1,

// Only for aem_nfa_add_regex
	// If true, only create captures for pairs of () that would otherwise be unnecessary.
	AEM_REGEX_FLAG_EXPLICIT_CAPTURES = 0x2,
};

#define AEM_NFA_MATCH_ALLOC UINT_MAX

// If you pass AEM_NFA_MATCH_ALLOC as the match parameter to any of these
// functions, it will use the lowest number greater than any registered match
// number.
// Returns match number on success, or <0 on failure.
int aem_nfa_add_regex (struct aem_nfa *nfa, struct aem_stringslice re , unsigned int match, enum aem_regex_flags flags);
int aem_nfa_add_string(struct aem_nfa *nfa, struct aem_stringslice str, unsigned int match, enum aem_regex_flags flags);

#endif /* AEM_REGEX_H */
