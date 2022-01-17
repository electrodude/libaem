#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/stringbuf.h>

#include "ansi-term.h"

struct aem_stringslice aem_ansi_match_csi(struct aem_stringslice *in)
{
	aem_assert(in);

	struct aem_stringslice curr = *in;

	if (!aem_stringslice_match(&curr, "\x1b["))
		return AEM_STRINGSLICE_EMPTY;

	int c = aem_stringslice_getc(&curr);
	while (0x30 <= c && c < 0x40)
		c = aem_stringslice_getc(&curr);
	// Match intermediate bytes
	while (0x20 <= c && c < 0x30)
		c = aem_stringslice_getc(&curr);
	// Match final byte
	if (!(0x40 <= c && c <= 0x7F)) {
		// TODO: Correctly count invalid CSI sequence
		return AEM_STRINGSLICE_EMPTY;
	}
	// TODO: Do real terminals just eat bytes until they find [\x40-\x7F]?

	struct aem_stringslice out = {.start = in->start, .end = curr.start};
	*in = curr;
	return out;
}

size_t aem_ansi_len(struct aem_stringslice in)
{
	size_t n = 0;

	while (aem_stringslice_ok(in)) {
		// TODO: Unicode double-width characters
		// TODO: Ignore other control codes
		if (aem_stringslice_ok(aem_ansi_match_csi(&in))) {
			// skip
		} else {
			// UTF-8 codepoint
			uint32_t c;
			if (aem_stringslice_get_rune(&in, &c)) {
				n++;
			}
		}
	}

	return n;
}

void aem_ansi_strip(struct aem_stringbuf *out, struct aem_stringslice in)
{
	aem_assert(out);

	while (aem_stringslice_ok(in)) {
		if (aem_stringslice_ok(aem_ansi_match_csi(&in))) {
			// skip
		} else {
			// other byte
			int c = aem_stringslice_getc(&in);
			if (c < 0)
				break; // can't happen?
			aem_stringbuf_putc(out, c);
		}
	}
}

void aem_ansi_strip_inplace(struct aem_stringbuf *str)
{
	aem_assert(str);
	// Yes, this is correct.  `aem_ansi_strip` never writes more characters than it reads.
	struct aem_stringslice src = aem_stringslice_new_str(str);
	str->n = 0;
	aem_ansi_strip(str, src);
}
