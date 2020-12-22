#include <ctype.h>
#include <stdlib.h>

#ifdef __unix__
#include <errno.h>
#endif

#include <aem/log.h>

#include "stringslice.h"

int aem_stringslice_file_write(struct aem_stringslice *slice, FILE *fp)
{
	if (!slice)
		return 1;

	while (aem_stringslice_ok(*slice)) {
		size_t n_written = fwrite(slice->start, 1, aem_stringslice_len(*slice), fp);

		if (!n_written) {
			if (ferror(fp)) {
				return 1;
			}
		}

		slice->start += n_written;
	}

	return 0;
}

#ifdef __unix__
ssize_t aem_stringslice_fd_write(struct aem_stringslice *slice, int fd)
{
	if (!slice)
		return -1;

	ssize_t total = 0;

	while (aem_stringslice_ok(*slice)) {
again:;
		ssize_t out = write(fd, slice->start, aem_stringslice_len(*slice));

		if (out < 0) {
			if (errno == EINTR) {
				goto again;
			}
			return -1;
		}

		slice->start += out;
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

int aem_stringslice_match_newline(struct aem_stringslice *slice)
{
	if (!slice)
		return 0;

	int matched = 0;

	if (aem_stringslice_ok(*slice) && *slice->start == '\r') {
		slice->start++;
		matched |= 2;
	}

	if (aem_stringslice_ok(*slice) && *slice->start == '\n') {
		slice->start++;
		matched |= 1;
	}

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

	struct aem_stringslice p = *slice;

	struct aem_stringslice line = p;

	// If the last character given to the previous invocation of this
	// function was CR, and the first character of this one is LF, then
	// skip the LF because it was part of the last line of the previous
	// invocation, which was already processed.
	if (*state && aem_stringslice_match(slice, "\n")) {
		aem_logf_ctx(AEM_LOG_WARN, "Dropping missed LF from a CRLF split across a packet boundary\n");
		*state = 0;
	}

	while (aem_stringslice_ok(p)) {
		// Set potential line end, in case a newline is next.
		line.end = p.start;

		// Try to get a newline
		int newline = aem_stringslice_match_newline(&p);
		// If the last character of p was a CR, set the flag so we know
		// to skip an LF at the beginning of the next call to this
		// function.
		*state = newline == 1 && !aem_stringslice_ok(p);
		// If it was a newline, save our progress and return the line.
		if (newline) {
			*slice = p;

			return line;
		}

		// It wasn't a newline, so it must be something else.
		aem_stringslice_getc(&p);
	}

	if (finish) {
		*slice = p;

		return line;
	}

	return AEM_STRINGSLICE_EMPTY;
}

int aem_stringslice_match(struct aem_stringslice *slice, const char *s)
{
	if (!slice)
		return 0;
	if (!s)
		return 1;

	const char *p2 = slice->start;

	while (*s) {
		if (p2 == slice->end) {
			return 0;
		}

		if (*p2++ != *s++) {
			return 0;
		}
	}

	slice->start = p2;

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

int aem_stringslice_cmp(struct aem_stringslice s0, struct aem_stringslice s1) {
	size_t l0 = aem_stringslice_len(s0);
	size_t l1 = aem_stringslice_len(s1);

	int cmp = strncmp(s0.start, s1.start, l0 < l1 ? l0 : l1);
	if (cmp)
		return cmp;

	return l1 - l0;
}

static inline int hex2nib(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 0xA;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 0xA;
	} else {
		return -1;
	}
}

int aem_stringslice_match_hexbyte(struct aem_stringslice *slice)
{
	if (!slice)
		return -1;

	if (slice->start == slice->end || slice->start + 1 == slice->end)
		return -1;

	const char *p = slice->start;

	int c1 = hex2nib(*p++);
	if (c1 < 0)
		return -1;

	int c0 = hex2nib(*p++);
	if (c0 < 0)
		return -1;

	slice->start = p;

	return c1 << 4 | c0;
}

int aem_stringslice_match_int(struct aem_stringslice *slice, int base, int *out) {
	if (!slice)
		return -1;

	int acc = 0;

	struct aem_stringslice best = *slice;
	struct aem_stringslice curr = *slice;

	int neg = 0;
	if (aem_stringslice_match(&curr, "-")) {
		neg = 1;
	}

	while (aem_stringslice_ok(curr)) {
		int c = aem_stringslice_get(&curr);
		int digit;
		if ('0' <= c && c <= '9') {
			digit = c - '0';
		} else if ('a' <= c && c <= 'z') {
			digit = c - 'a' + 10;
		} else if ('A' <= c && c <= 'Z') {
			digit = c - 'A' + (base > 36 ? 36 : 10); // Be case-insensitive if base is low enough to not need both cases.
		} else {
			break;
		}

		acc = acc*base + digit;
		best = curr;
	}

	// Return failure if we found no digits
	if (best.start == slice->start)
		return -1;

	if (neg)
		acc = -acc;

	if (out)
		*out = acc;
	*slice = best;

	return 0;
}
