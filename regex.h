#ifndef AEM_REGEX_H
#define AEM_REGEX_H

#include <aem/nfa.h>

/// Regex parser

/* Flags:
 * d: Generate debug information for regex tracing, disable some optimizations.
 *    Stores pointers into your pattern stringslice in the `struct aem_nfa`!
 * c: Only create captures for pairs of () that would otherwise be unnecessary
 * b: Binary mode: match single bytes, rather than UTF-8 codepoints, and make
 *    /./ match any character, even newline.  Patterns must still be valid
 *    UTF-8 regardless of the status of this flag.
 */

// If you pass <0 as the match parameter to any of these functions, it will use
// the lowest number greater than any registered match number.
// Returns match number on success, or <0 on failure.
int aem_nfa_add_regex (struct aem_nfa *nfa, struct aem_stringslice re , int match, struct aem_stringslice flags);
int aem_nfa_add_string(struct aem_nfa *nfa, struct aem_stringslice str, int match, struct aem_stringslice flags);

#endif /* AEM_REGEX_H */
