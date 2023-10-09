#define AEM_DEBUG_UTF8 0

#define AEM_INTERNAL
#include <aem/stringslice.h>
#include <aem/stringbuf.h>

#if AEM_DEBUG_UTF8
# include <aem/log.h>
#endif

#include "utf8.h"

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

// gcc-11.2.0 -O3 compiles away this table for both aem_stringbuf_put_rune
// and aem_stringslice_get_rune, but clang-13 -O3 doesn't.
const struct aem_utf8_info aem_utf8_info[AEM_UTF8_INFO_LEN] = {
	{.max = 0x0000007F, .top = 0x00, .mask = 0x7f},
	{.max = 0x000007FF, .top = 0xc0, .mask = 0x1f},
	{.max = 0x0000FFFF, .top = 0xe0, .mask = 0x0f},
	{.max = 0x001FFFFF, .top = 0xf0, .mask = 0x07},
	{.max = 0x03FFFFFF, .top = 0xf8, .mask = 0x03},
	{.max = 0xFFFFFFFF, .top = 0xfc, .mask = 0x03},
};

int aem_stringbuf_put_rune(struct aem_stringbuf *str, uint32_t c)
{
	// Find relevant table row
	size_t len;
	for (len = 0; len < AEM_UTF8_INFO_LEN-1; len++)
		if (c <= aem_utf8_info[len].max)
			break;

	aem_assert(len < AEM_UTF8_INFO_LEN);

	// Do first byte according to table
	const struct aem_utf8_info *info = &aem_utf8_info[len];
	size_t shift = len*6;
	aem_stringbuf_putc(str, info->top | ((c >> shift) & info->mask));

	// Do continuation bytes
	while (len--) {
		shift -= 6;
		aem_stringbuf_putc(str, 0x80 | ((c >> shift) & 0x3f));
	};

	return 0;
}

int aem_stringslice_get_rune(struct aem_stringslice *slice, uint32_t *out_p)
{
	aem_assert(slice);
	struct aem_stringslice out = *slice;

	// Get first byte
	int c0 = aem_stringslice_getc(slice);
	if (c0 < 0) // EOF
		return 0;

	uint32_t c = (unsigned int)c0;

	aem_assert(c0 < 0x100);

	size_t len;
	for (len = 0; len < AEM_UTF8_INFO_LEN; len++) {
		const struct aem_utf8_info *info = &aem_utf8_info[len];
		if ((c & ~info->mask) == info->top)
			break;
	}

	if (len >= AEM_UTF8_INFO_LEN) {
#if AEM_DEBUG_UTF8
		aem_logf_ctx(AEM_LOG_DEBUG, "Unexpected byte 0x%02x", c);
#endif
		slice->start = out.start;
		return 0;
	}

	aem_assert(len < AEM_UTF8_INFO_LEN);

	c &= aem_utf8_info[len].mask;

	// Get continuation bytes
	for (size_t i = 0; i < len; i++) {
		int c2 = aem_stringslice_getc(slice);
		if (c2 < 0 || (c2 & 0xc0) != 0x80) {
			// EOF or not a continuation byte
			slice->start = out.start;
			return 0;
		}

		c = (c << 6) | (c2 & 0x3f);
	}

	// Store result
	if (out_p)
		*out_p = c;

	return 1;
}

// BUG: 0xFFFFFFFF and <invalid> both return -1 and are therefore indistinguishable
int aem_stringslice_get(struct aem_stringslice *slice)
{
	uint32_t c;
	if (!aem_stringslice_get_rune(slice, &c))
		return -1;

	return c;
}
