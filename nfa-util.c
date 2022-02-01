#define AEM_INTERNAL
#include <aem/ansi-term.h>
#include <aem/stringbuf.h>

#include "nfa-util.h"

void aem_nfa_desc_char(struct aem_stringbuf *out, uint32_t c)
{
	switch (c) {
#define AEM_STRINGBUF_PUTQ_CASE(find, replace) \
	case find: aem_stringbuf_puts(out, replace); break;
		AEM_STRINGBUF_PUTQ_CASE('\n', "\\n")
		AEM_STRINGBUF_PUTQ_CASE('\r', "\\r")
		AEM_STRINGBUF_PUTQ_CASE('\t', "\\t")
		AEM_STRINGBUF_PUTQ_CASE('\0', "\\0")
		AEM_STRINGBUF_PUTQ_CASE('\'', "\\'")
		AEM_STRINGBUF_PUTQ_CASE('\\', "\\\\")
		AEM_STRINGBUF_PUTQ_CASE('[', "\\[")
		AEM_STRINGBUF_PUTQ_CASE(']', "\\]")
#undef AEM_STRINGBUF_PUTQ_CASE
	default:
		if (c >= 32 && c < 127) {
			aem_stringbuf_putc(out, c);
		} else {
			aem_stringbuf_printf(out, "\\u%x", c);
		}
		break;
	}
}
void aem_nfa_desc_range(struct aem_stringbuf *out, uint32_t lo, uint32_t hi)
{
	aem_assert(out);

	if (hi != lo) {
		aem_stringbuf_puts(out, AEM_SGR("95") "[" AEM_SGR("0"));
		aem_nfa_desc_char(out, lo);
		aem_stringbuf_puts(out, AEM_SGR("95") "-" AEM_SGR("0"));
		aem_nfa_desc_char(out, hi);
		aem_stringbuf_puts(out, AEM_SGR("95") "]" AEM_SGR("0"));
	} else {
		aem_stringbuf_puts(out, AEM_SGR("95") "'" AEM_SGR("0"));
		aem_nfa_desc_char(out, lo);
		aem_stringbuf_puts(out, AEM_SGR("95") "'" AEM_SGR("0"));
	}
}
