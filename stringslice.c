#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#ifdef __unix__
# include <errno.h>
#endif

#define AEM_INTERNAL
#include <aem/log.h>

#include "stringslice.h"

int aem_stringslice_file_write(struct aem_stringslice slice, FILE *fp)
{
	aem_assert(fp);

	while (aem_stringslice_ok(slice)) {
		size_t n_written = fwrite(slice.start, 1, aem_stringslice_len(slice), fp);

		if (!n_written) {
			if (ferror(fp)) {
				return -1;
			}
		}

		slice.start += n_written;
	}

	return 0;
}

#ifdef __unix__
ssize_t aem_stringslice_fd_write(struct aem_stringslice slice, int fd)
{
	if (fd < 0)
		return -1;

	ssize_t total = 0;

	while (aem_stringslice_ok(slice)) {
again:;
		ssize_t out = write(fd, slice.start, aem_stringslice_len(slice));

		if (out < 0) {
			if (errno == EINTR) {
				goto again;
			}
			return -1;
		}

		slice.start += out;
		total += out;
	}

	return total;
}
#endif

int aem_stringslice_match_ws(struct aem_stringslice *slice)
{
	if (!slice)
		return 0;

	int matched = 0;

	while (aem_stringslice_ok(*slice) && isspace(*slice->start)) {
		matched = 1;
		slice->start++;
	}

	return matched;
}

struct aem_stringslice aem_stringslice_trim(struct aem_stringslice slice)
{
	while (aem_stringslice_ok(slice) && isspace(slice.start[0]))
		slice.start++;
	while (aem_stringslice_ok(slice) && isspace(slice.end[-1]))
		slice.end--;

	return slice;
}

int aem_stringslice_match_newline(struct aem_stringslice *slice)
{
	if (!slice)
		return 0;

	int matched = 0;

	if (aem_stringslice_match(slice, "\r"))
		matched |= 2;

	if (aem_stringslice_match(slice, "\n"))
		matched |= 1;

	return matched;
}

struct aem_stringslice aem_stringslice_match_alnum(struct aem_stringslice *slice)
{
	if (!slice)
		return AEM_STRINGSLICE_EMPTY;

	struct aem_stringslice line;
	line.start = slice->start;

	while (aem_stringslice_ok(*slice) && isalnum(*slice->start))
		slice->start++;

	line.end = slice->start;

	return line;
}
struct aem_stringslice aem_stringslice_match_word(struct aem_stringslice *slice)
{
	if (!slice)
		return AEM_STRINGSLICE_EMPTY;

	// TODO: aem_stringslice_match_ws(slice);

	struct aem_stringslice line;
	line.start = slice->start;

	while (aem_stringslice_ok(*slice) && !isspace(*slice->start))
		slice->start++;

	line.end = slice->start;

	return line;
}

struct aem_stringslice aem_stringslice_match_line(struct aem_stringslice *slice)
{
	if (!slice)
		return AEM_STRINGSLICE_EMPTY;

	struct aem_stringslice line = *slice;

	while (aem_stringslice_ok(*slice) && !aem_stringslice_match_newline(slice))
		slice->start++;

	line.end = slice->start;

	return line;
}

struct aem_stringslice aem_stringslice_match_line_multi(struct aem_stringslice *slice, int *state, int finish)
{
	if (!slice)
		return AEM_STRINGSLICE_EMPTY;

	aem_assert(state);

	// If the last character given to the previous invocation of this
	// function was CR, and the first character of this one is LF, then
	// skip the LF because it was part of the last line of the previous
	// invocation, which was already processed.
	if (*state && aem_stringslice_match(slice, "\n")) {
		//aem_logf_ctx(AEM_LOG_WARN, "Dropping missed LF from a CRLF split across a packet boundary");
		*state = 0;
	}

	struct aem_stringslice p = *slice;

	// Initialize line to be an empty slice at the beginning of the input slice.
	struct aem_stringslice line = {.start = p.start, .end = p.start};

	// Don't return an empty line if finish flag is set but input is empty
	if (!aem_stringslice_ok(p)) {
		line = AEM_STRINGSLICE_EMPTY;
		goto out;
	}

	while (aem_stringslice_ok(p)) {
		// Set potential line end, in case a newline is next.
		line.end = p.start;

		// Try to get a newline
		int newline = aem_stringslice_match_newline(&p);
		// If we found a CR newline at the end of the input, it's
		// possible it's really part of a CRLF split across multiple
		// invocations of this function, so note this situation so we
		// can deal with it at the beginning of the next call to this
		// function.
		*state = newline == 2 && !aem_stringslice_ok(p);
		// If we found a newline, save our progress and return the line.
		if (newline) {
			*slice = p;
			goto out;
		}

		// It wasn't a newline, so it must be something else.
		aem_stringslice_getc(&p);
	}

	if (finish) {
		// Return the incomplete line anyway if this will be the final call to
		// this function.
		line.end = p.start;
		*slice = p;
	} else {
		// We didn't get a whole line, so don't return anything
		line = AEM_STRINGSLICE_EMPTY;
	}

out:
	if (finish)
		*state = 0;

	return line;
}

int aem_stringslice_match_prefix(struct aem_stringslice *slice, struct aem_stringslice s)
{
	if (!slice)
		return 0;

	struct aem_stringslice p = *slice;

	while (aem_stringslice_ok(s)) {
		if (!aem_stringslice_ok(p))
			return 0;

		if (aem_stringslice_getc(&p) != aem_stringslice_getc(&s))
			return 0;
	}

	*slice = p;

	return 1;
}

int aem_stringslice_match_suffix(struct aem_stringslice *slice, struct aem_stringslice s)
{
	if (!slice)
		return 0;

	struct aem_stringslice p = *slice;

	while (aem_stringslice_ok(s)) {
		if (!aem_stringslice_ok(p))
			return 0;

		if (*--p.end != *--s.end)
			return 0;
	}

	*slice = p;

	return 1;
}

int aem_stringslice_eq(struct aem_stringslice slice, const char *s)
{
	if (!s)
		return 1;

	// TODO: What is strncmp?
	while (aem_stringslice_ok(slice) && *s != '\0') { // While neither are finished
		if (*slice.start++ != *s++)               // Unequal characters => no match
			return 0;
	}

	return !aem_stringslice_ok(slice) && *s == '\0'; // Ensure both are finished.
}

int aem_stringslice_eq_case(struct aem_stringslice slice, const char *s)
{
	if (!s)
		return 1;

	// TODO: What is strncasecmp?
	while (aem_stringslice_ok(slice) && *s != '\0') { // While neither are finished
		if (tolower(*slice.start++) != tolower(*s++)) // Unequal characters => no match
			return 0;
	}

	return !aem_stringslice_ok(slice) && *s == '\0'; // Ensure both are finished.
}

int aem_stringslice_cmp(struct aem_stringslice s0, struct aem_stringslice s1)
{
	size_t l0 = aem_stringslice_len(s0);
	size_t l1 = aem_stringslice_len(s1);

	int cmp = strncmp(s0.start, s1.start, l0 < l1 ? l0 : l1);
	if (cmp)
		return cmp;

	return l1 - l0;
}

// Could be a lookup table
static inline int char2digit(char c, int numeral, int lcase, int ucase)
{
	if (c >= '0' && c <= '9')
		return c - '0' + numeral;
	if (c >= 'a' && c <= 'z')
		return c - 'a' + lcase;
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + ucase;
	return -1;
}

int aem_stringslice_match_hexbyte(struct aem_stringslice *slice)
{
	if (!slice)
		return -1;

	// Ensure at least two characters remain.
	if (slice->start == slice->end || slice->start + 1 == slice->end)
		return -1;

	const char *p = slice->start;

	int c1 = char2digit(*p++, 0, 0xA, 0xA);
	if (c1 < 0 || c1 > 0xF)
		return -1;

	int c0 = char2digit(*p++, 0, 0xA, 0xA);
	if (c0 < 0 || c0 > 0xF)
		return -1;

	slice->start = p;

	return c1 << 4 | c0;
}

int aem_stringslice_match_ulong_base(struct aem_stringslice *slice, int base, unsigned long int *out)
{
	aem_assert(slice);
	aem_assert(2 <= base && base <= 36);
	aem_assert(out);

	if (!aem_stringslice_ok(*slice))
		return 0;

	struct aem_stringslice curr = *slice;
	struct aem_stringslice best = curr;

	unsigned long int n = 0;

	int any_digits = 0;
	for (int c; (c = aem_stringslice_getc(&curr)) >= 0; ) {
		// TODO: + and / for Base64; but Base64 order is [A-Za-z0-9+/].
		int digit = char2digit(c, 0, 10, 10);
		if (digit < 0 || digit >= base) // Invalid digit; end of number
			break;
		if (n > ULONG_MAX/base) // Overflow; say we didn't find any number at all.
			return 0;
		n = n*base + digit;
		best = curr;
		any_digits = 1;
	}

	// Return failure if we found no digits
	if (!any_digits)
		return 0;

	*slice = best;
	*out = n;
	return 1;
}
int aem_stringslice_match_long_base(struct aem_stringslice *slice, int base, long int *out)
{
	aem_assert(slice);
	aem_assert(2 <= base && base <= 36);
	aem_assert(out);

	if (!aem_stringslice_ok(*slice))
		return 0;

	struct aem_stringslice curr = *slice;

	int neg = aem_stringslice_match(&curr, "-");

	unsigned long un;
	int ok = aem_stringslice_match_ulong_base(&curr, base, &un);

	if (!ok)
		return ok;

	long n;
	if (!neg) {
		n = un;
		// If it's negative, an overflow occurred.
		if (n < 0)
			return 0;
	} else {
		n = -un;
		// If it's positive, an overflow occurred.
		if (n >= 0)
			return 0;
	}

	*slice = curr;
	*out = n;
	return ok;
}

int aem_stringslice_match_uint_base(struct aem_stringslice *slice, int base, unsigned int *out)
{
	aem_assert(slice);
	aem_assert(out);

	struct aem_stringslice curr = *slice;

	unsigned long int out_l;
	int ok = aem_stringslice_match_ulong_base(&curr, base, &out_l);

	if (!ok)
		return ok;

	// Check for overflow
	if (out_l > UINT_MAX)
		return 0;

	*slice = curr;
	*out = out_l;
	return ok;
}
int aem_stringslice_match_int_base(struct aem_stringslice *slice, int base, int *out)
{
	aem_assert(slice);
	aem_assert(out);

	struct aem_stringslice curr = *slice;

	long int out_l;
	int ok = aem_stringslice_match_long_base(&curr, base, &out_l);

	if (!ok)
		return ok;

	// Check for overflow
	if ((out_l > 0 && out_l > INT_MAX) || (out_l < 0 && out_l < INT_MIN))
		return 0;

	*slice = curr;
	*out = out_l;
	return ok;
}

int aem_stringslice_match_long_auto(struct aem_stringslice *slice, long int *out)
{
	aem_assert(slice);
	aem_assert(out);

	if (!aem_stringslice_ok(*slice))
		return 0;

	struct aem_stringslice curr = *slice;

	int neg = aem_stringslice_match(&curr, "-");

	int base = 10;
	// TODO: Custom radix: e.g. 5r14320
	if (aem_stringslice_match(&curr, "0x")) {
		base = 16;
	} else if (aem_stringslice_match(&curr, "0b") || aem_stringslice_match(&curr, "0y")) {
		base = 2;
	//} else if (aem_stringslice_match(&curr, "0")) { // What's octal?
		//base = 8;
	// TODO: Pascal/Spin syntax: $ (hex), % (bin), %% (quaternary)
	} else {
		base = 10;
	}

	// TODO: baseN: syntax

	unsigned long int un;
	int ok = aem_stringslice_match_ulong_base(&curr, base, &un);

	if (!ok)
		return ok;

	long n;
	if (!neg) {
		n = un;
		// If it's negative, an overflow occurred.
		if (n < 0)
			return 0;
	} else {
		n = -un;
		// If it's positive, an overflow occurred.
		if (n >= 0)
			return 0;
	}

	*slice = curr;
	*out = n;
	return ok;
}
