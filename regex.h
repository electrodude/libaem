#ifndef AEM_REGEX_H
#define AEM_REGEX_H

#include <aem/nfa.h>

/// Regex parser
enum aem_regex_flags {
	// If enabled, input regex text memory must remain valid as long as NFA still has that debug info.
	// If disabled, optimizations are enabled that may confuse tracing.
	AEM_REGEX_FLAG_DEBUG = 0x1,
	// If true, stop parsing once ctx->final is found
	//AEM_REGEX_FLAG_PARSE_UNTIL = 0x4,

// Only for aem_nfa_add_regex
	// If true, only create captures for pairs of () that would otherwise be unnecessary.
	AEM_REGEX_FLAG_EXPLICIT_CAPTURES = 0x2,

	// Disable UTF-8 conversion.  Would be named AEM_REGEX_FLAG_DISABLE_UTF8 if I weren't so opinionated; avoid unless you actually aren't matching text
	// Patterns must still be properly escaped UTF-8 patterns even if this flag is set.
	AEM_REGEX_FLAG_BINARY = 0x4,
};

// If you pass <0 as the match parameter to any of these functions, it will use
// the lowest number greater than any registered match number.
// Returns match number on success, or <0 on failure.
int aem_nfa_add_regex (struct aem_nfa *nfa, struct aem_stringslice re , int match, enum aem_regex_flags flags);
int aem_nfa_add_string(struct aem_nfa *nfa, struct aem_stringslice str, int match, enum aem_regex_flags flags);

#endif /* AEM_REGEX_H */
