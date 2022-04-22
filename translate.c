#define AEM_INTERNAL
#include "translate.h"

void aem_string_escape_rune(struct aem_stringbuf *str, uint32_t c)
{
	if (!str)
		return;
	if (str->bad)
		return;

	switch (c) {
#define ESCAPE_CASE(find, replace) \
	case find: aem_stringbuf_puts(str, replace); break;
		ESCAPE_CASE('\n', "\\n")
		ESCAPE_CASE('\r', "\\r")
		ESCAPE_CASE('\t', "\\t")
		ESCAPE_CASE('\0', "\\0")
		ESCAPE_CASE('\"', "\\\"")
		ESCAPE_CASE('\\', "\\\\")
		ESCAPE_CASE(' ' , "\\ ")
#undef ESCAPE_CASE
		default:
			if (c >= 32 && c < 127) {
				aem_stringbuf_putc(str, c);
			} else if (c < 0x100) {
				aem_stringbuf_printf(str, "\\x%02x", c);
			} else if (c < 0x10000) {
				aem_stringbuf_printf(str, "\\u%04x", c);
			} else {
				aem_stringbuf_printf(str, "\\U%08x", c);
			}
			break;
	}
}
void aem_string_escape(struct aem_stringbuf *restrict str, struct aem_stringslice slice)
{
	aem_assert(str);

	if (str->bad)
		return;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "\"%s\" ..= escape(<slice>)", aem_stringbuf_get(str));
#endif

	while (aem_stringslice_ok(slice)) {
		uint32_t c;
		if (!aem_stringslice_get_rune(&slice, &c)) {
			// TODO BUG: Don't differentiate between valid and invalid UTF-8?
			c = aem_stringslice_getc(&slice);
		}
		aem_string_escape_rune(str, c);
	}
}

int aem_string_unescape_rune(struct aem_stringslice *in, uint32_t *c_p, int *esc_p)
{
	aem_assert(in);

	struct aem_stringslice orig = *in;

	int esc = aem_stringslice_match(in, "\\");
	uint32_t c;
	if (!aem_stringslice_get_rune(in, &c)) {
		if (esc) {
			// Unescaped backslash at end of string
			// TODO: Or backslash followed by invalid codepoint
			esc = 0;
			c = '\\';
			goto done;
		}
		goto fail;
	}

	if (esc) {
		esc = 1; // Substituted escape
		switch (c) {
		case '0': c = '\0'  ; break;
		case 'e': c = '\x1b'; break;
		case 'f': c = '\f'  ; break;
		case 't': c = '\t'  ; break;
		case 'n': c = '\n'  ; break;
		case 'r': c = '\r'  ; break;
		case 'v': c = '\v'  ; break;
		case 'x':
		case 'u':
		case 'U': {
			int len = 0;
			if (aem_stringslice_match(in, "{"))
				len = -1;
			else if (c == 'x')
				len = 2;
			else if (c == 'u')
				len = 4;
			else if (c == 'U')
				len = 8;
			else
				goto fail;
			c = 0;
			while (len) {
				int d = aem_stringslice_get(in);
				if ('0' <= d && d <= '9')
					c = (c << 4) + (d - '0' + 0x0);
				else if ('A' <= d && d <= 'F')
					c = (c << 4) + (d - 'A' + 0xA);
				else if ('a' <= d && d <= 'f')
					c = (c << 4) + (d - 'a' + 0xA);
				else if (d < 0)
					goto fail;
				else if (d == '}' && len < 0)
					break;
				len--;
			}
			break;
		}
		default:
			esc = 2; // Unrecognized escape
			break;
		}
	}

done:
	if (esc_p)
		*esc_p = esc;

	if (c_p)
		*c_p = c;

	return 1;

fail:
	*in = orig;
	return 0;
}

void aem_string_unescape(struct aem_stringbuf *restrict str, struct aem_stringslice *restrict slice)
{
	aem_assert(str);
	aem_assert(slice);

	while (aem_stringslice_ok(*slice)) {
		uint32_t c;
		int esc;
		if (aem_string_unescape_rune(slice, &c, &esc)) {
			aem_stringbuf_put_rune(str, c);
		} else {
			// TODO: Just copy invalid UTF-8 verbatim?
			int c = aem_stringslice_getc(slice);
			aem_stringbuf_putc(str, c);
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
