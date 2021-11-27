#define AEM_DEBUG_UTF8 0

#define AEM_INTERNAL
#include <aem/stringslice.h>
#include <aem/stringbuf.h>

#if AEM_DEBUG_UTF8
#include <aem/log.h>
#endif

/*
Functions to read and write UTF-8 codepoints (runes)

Supports encoding any 32-bit unsigned value, via the extended scheme at the end
of https://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt , and copied here:

We define 7 byte types:
T0	0xxxxxxx	7 free bits
Tx	10xxxxxx	6 free bits
T1	110xxxxx	5 free bits
T2	1110xxxx	4 free bits
T3	11110xxx	3 free bits
T4	111110xx	2 free bits
T5	111111xx	2 free bits

Encoding is as follows.
>From hex	Thru hex	Sequence		Bits
00000000	0000007f	T0			7
00000080	000007FF	T1 Tx			11
00000800	0000FFFF	T2 Tx Tx		16
00010000	001FFFFF	T3 Tx Tx Tx		21
00200000	03FFFFFF	T4 Tx Tx Tx Tx		26
04000000	FFFFFFFF	T5 Tx Tx Tx Tx Tx	32

Some notes:

1. The 2 byte sequence has 2^11 codes, yet only 2^11-2^7
are allowed. The codes in the range 0-7f are illegal.
I think this is preferable to a pile of magic additive
constants for no real benefit. Similar comment applies
to all of the longer sequences.

2. The 4, 5, and 6 byte sequences are only there for
political reasons. I would prefer to delete these.

3. The 6 byte sequence covers 32 bits, the FSS-UTF
proposal only covers 31.

4. All of the sequences synchronize on any byte that is
not a Tx byte.
*/

int aem_stringbuf_put(struct aem_stringbuf *str, unsigned int c)
{
	if (c < 0x80) {
		aem_stringbuf_putc(str, c);
		goto put0;
	} else if (c < 0x800) {
		aem_stringbuf_putc(str, 0xc0 | ((c >>  6) & 0x1f));
		goto put1;
	} else if (c < 0x10000) {
		aem_stringbuf_putc(str, 0xe0 | ((c >> 12) & 0x0f));
		goto put2;
	} else if (c < 0x200000) {
		aem_stringbuf_putc(str, 0xf0 | ((c >> 18) & 0x07));
		goto put3;
	} else if (c < 0x4000000) {
		aem_stringbuf_putc(str, 0xf8 | ((c >> 24) & 0x03));
		goto put4;
	} else {
		aem_stringbuf_putc(str, 0xfc | ((c >> 30) & 0x03));
		goto put5;
	}

put5:
	aem_stringbuf_putc(str, 0x80 | ((c >> 24) & 0x3f));
put4:
	aem_stringbuf_putc(str, 0x80 | ((c >> 18) & 0x3f));
put3:
	aem_stringbuf_putc(str, 0x80 | ((c >> 12) & 0x3f));
put2:
	aem_stringbuf_putc(str, 0x80 | ((c >>  6) & 0x3f));
put1:
	aem_stringbuf_putc(str, 0x80 | ((c >>  0) & 0x3f));
put0:

	return 0;
}

int aem_stringslice_get(struct aem_stringslice *slice)
{
	const char *start = slice->start; // make backup of start

	int c = aem_stringslice_getc(slice);
	if (c < 0 || c >= 0x100)  // end of input or invalid
		return c;

#if AEM_DEBUG_UTF8
	unsigned char c0 = c;
#endif

	size_t n = 0;

	if (c < 0x80) {
		n = 0;
		c &= 0x7f;
	} else if (c < 0xc0) {
#if AEM_DEBUG_UTF8
		aem_logf_ctx(AEM_LOG_DEBUG, "Unexpected continuation 0x%02x", c);
#endif
		//c &= 0x3f;
		return -1;
	} else if (c < 0xe0) {
		n = 1;
		c &= 0x1f;
	} else if (c < 0xf0) {
		n = 2;
		c &= 0x0f;
	} else if (c < 0xf8) {
		n = 3;
		c &= 0x07;
	} else if (c < 0xfc) {
		n = 4;
		c &= 0x03;
	} else if (c < 0x100) {
		n = 5;
		c &= 0x03;
	} else {
#if AEM_DEBUG_UTF8
		aem_logf_ctx(AEM_LOG_DEBUG, "Illegal first byte");
#endif
		return -1;
	}

#if AEM_DEBUG_UTF8
	struct aem_stringbuf *log = aem_log_header(&aem_log_buf, AEM_LOG_DEBUG);
	if (log)
		aem_stringbuf_printf(log, "utf8 parse: %zd: 0x%02x", n, c0);
#endif

	for (size_t i = 0; i < n; i++) {
		if (!aem_stringslice_ok(*slice))
			return -1;

		int c2 = aem_stringslice_getc(slice);
#if AEM_DEBUG_UTF8
		if (log)
			aem_stringbuf_printf(log, ", 0x%02x", (unsigned char)c2);
#endif
		if (c2 < 0 || (c2 & 0xc0) != 0x80) {
			slice->start = start;
#if AEM_DEBUG_UTF8
			if (log)
				aem_stringbuf_puts(log, " => invalid");
#endif
			return -1;
		}

		c <<= 6;
		c |= c2 & 0x3f;
	}

#if AEM_DEBUG_UTF8
	if (log) {
		aem_stringbuf_printf(log, " => 0x%08x\n", c);
		aem_log_str(log);
	}
#endif

	return c;
}
