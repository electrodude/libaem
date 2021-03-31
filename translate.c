#define AEM_INTERNAL
#include "translate.h"

void aem_string_escape(struct aem_stringbuf *restrict str, struct aem_stringslice slice)
{
	aem_assert(str);

	if (str->bad)
		return;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "\"%s\" ..= escape(<slice>)", aem_stringbuf_get(str));
#endif

	while (aem_stringslice_ok(slice)) {
		int c = aem_stringslice_getc(&slice);
		aem_stringbuf_putq(str, c);
	}
}

void aem_string_unescape(struct aem_stringbuf *restrict str, struct aem_stringslice *restrict slice)
{
	aem_assert(str);
	aem_assert(slice);

	while (aem_stringslice_ok(*slice)) {
		struct aem_stringslice checkpoint = *slice;

		int c = aem_stringslice_getc(slice);

		if (c == '\\') {
			int c2 = aem_stringslice_getc(slice);

			// if no character after the backslash, just output the backslash
			if (c2 < 0)
				c2 = '\\';

			switch (c2) {
#define UNQUOTE_CASE(find, replace) \
	case find: aem_stringbuf_putc(str, replace); break;
				UNQUOTE_CASE('n' , '\n');
				UNQUOTE_CASE('r' , '\r');
				UNQUOTE_CASE('t' , '\t');
				UNQUOTE_CASE('0' , '\0');
				case 'x':;
					int b = aem_stringslice_match_hexbyte(slice);
					if (b >= 0) { // if valid hex byte, unescape it
						c2 = b;
						aem_stringbuf_putc(str, b);
					} else {
						// else just put the unescaped 'x' without the backslash
						aem_stringbuf_putc(str, c2);
					}
					break;

				default:
					aem_stringbuf_putc(str, c2);
					break;
#undef UNQUOTE_CASE
			}
		} else if (c > 32 && c < 127) {
			aem_stringbuf_putc(str, c);
		} else {
			*slice = checkpoint;
			break;
		}
	}
}

void aem_string_urlencode(struct aem_stringbuf *restrict out, struct aem_stringslice in)
{
	aem_assert(out);

	while (aem_stringslice_ok(in)) {
		int c = aem_stringslice_getc(&in);

		if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ('0' <= c && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
			// Don't encode unreserved characters
			aem_stringbuf_putc(out, c);
		} else {
			// Do encode everything else
			aem_stringbuf_putc(out, '%');
			aem_stringbuf_puthex(out, c);
		}
	}
}
void aem_string_urldecode(struct aem_stringbuf *restrict out, struct aem_stringslice *restrict in)
{
	aem_assert(in);
	aem_assert(out);

	while (aem_stringslice_ok(*in)) {
		struct aem_stringslice checkpoint = *in;
		int c = aem_stringslice_get(in);

		if (c < 0) {
			break;
		} if (c == '%') {
			int c2 = aem_stringslice_match_hexbyte(in);
			if (c2 < 0) {
				*in = checkpoint;
				return;
			}
			aem_stringbuf_putc(out, c2);
		} else {
			aem_stringbuf_putc(out, c);
		}
	}
}
