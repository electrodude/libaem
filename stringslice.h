#ifndef AEM_STRINGSLICE_H
#define AEM_STRINGSLICE_H

#include <stdio.h>
#include <string.h>
#ifdef __unix__
#include <unistd.h>
#endif

#include <aem/aem.h>

struct aem_stringslice {
	const char *start;
	const char *end;
};

#define aem_stringslice_new(p, pe) ((struct aem_stringslice){.start = (p), .end = (pe)})
#define AEM_STRINGSLICE_EMPTY aem_stringslice_new(NULL, NULL)

static inline struct aem_stringslice aem_stringslice_new_len(const char *p, size_t n)
{
	return aem_stringslice_new(p, &p[n]);
}

// The optimizer should be smart enough to elide the call to strlen if you pass
// in a string literal.
static inline struct aem_stringslice aem_stringslice_new_cstr(const char *p)
{
	if (!p)
		return AEM_STRINGSLICE_EMPTY;

	return aem_stringslice_new_len(p, strlen(p));
}

#define aem_stringslice_new_sizeof(char_arr) aem_stringslice_new_len((char_arr), sizeof(char_arr)-1)


int aem_stringslice_file_write(struct aem_stringslice slice, FILE *fp);

#ifdef __unix__
ssize_t aem_stringslice_fd_write(struct aem_stringslice slice, int fd);
#endif


// Return true if stringslice length is non-zero
static inline int aem_stringslice_ok(struct aem_stringslice slice)
{
	return slice.start != slice.end;
}

// Return length of stringslice
static inline size_t aem_stringslice_len(const struct aem_stringslice slice)
{
	return slice.end - slice.start;
}

// Get next byte from stringslice (-1 if end), advance to next byte
static inline int aem_stringslice_getc(struct aem_stringslice *slice)
{
	if (!aem_stringslice_ok(*slice))
		return -1;

	return (unsigned char)*slice->start++;
}

// Get a UTF-8 rune
// Implementation in utf8.c
int aem_stringslice_get(struct aem_stringslice *slice);

// Get raw data
// Reads `count` bytes into `buf`.
// If fewer than `count` bytes are available, does nothing and returns -1.
static inline int aem_stringslice_read_data(struct aem_stringslice *slice, void *buf, size_t count)
{
	if (aem_stringslice_len(*slice) < count)
		return -1;

	memcpy(buf, slice->start, count);
	slice->start += count;

	return 0;
}
#define AEM_STRINGSLICE_RD_TYPE(slice, T) ({ \
	T out; \
	aem_stringslice_read_data(slice, &out, sizeof(out)); \
	out; \
})

int aem_stringslice_match_ws(struct aem_stringslice *slice);
struct aem_stringslice aem_stringslice_trim(struct aem_stringslice slice);

// Consume a CR, CRLF, or LF at the current position.
// Returns 1 for LF, 2 for CR, 3 for CRLF, or 0 on failure
int aem_stringslice_match_newline(struct aem_stringslice *slice);

struct aem_stringslice aem_stringslice_match_alnum(struct aem_stringslice *slice);
struct aem_stringslice aem_stringslice_match_word(struct aem_stringslice *slice);

// Match a line, even if it's missing its line terminator.
struct aem_stringslice aem_stringslice_match_line(struct aem_stringslice *slice);

// Match a line, but fail if it has no line terminator unless finish is non-zero.
//
// This function is intended to be used when the input stringslice might not
// contain all data yet - it can be called again whenever more data becomes
// available, and it will only return a line and modify the input stringslice
// when a complete line is available (unless finish is set).
//
// The `state` input should be initialized to zero and preserved across calls.
// It is currently only used to ensure CRLF line terminators are treated as
// only a single newline even when they're split across multiple partial
// buffers fed to consecutive calls to this function.
struct aem_stringslice aem_stringslice_match_line_multi(struct aem_stringslice *slice, int *state, int finish);

int aem_stringslice_match_prefix(struct aem_stringslice *slice, struct aem_stringslice s);
int aem_stringslice_match_suffix(struct aem_stringslice *slice, struct aem_stringslice s);

static inline int aem_stringslice_match(struct aem_stringslice *slice, const char *s)
{
	return aem_stringslice_match_prefix(slice, aem_stringslice_new_cstr(s));
}
static inline int aem_stringslice_match_end(struct aem_stringslice *slice, const char *s)
{
	return aem_stringslice_match_suffix(slice, aem_stringslice_new_cstr(s));
}

static inline int aem_stringslice_match_bom(struct aem_stringslice *slice)
{
	return aem_stringslice_match(slice, "\xEF\xBB\xBF");
}

// Test whether a stringslice exactly matches the given C-string
int aem_stringslice_eq(struct aem_stringslice slice, const char *s);
// Case-insensitive version of the above
int aem_stringslice_eq_case(struct aem_stringslice slice, const char *s);

int aem_stringslice_cmp(struct aem_stringslice s0, struct aem_stringslice s1);

int aem_stringslice_match_hexbyte(struct aem_stringslice *slice);

int aem_stringslice_match_ulong_base(struct aem_stringslice *slice, int base, unsigned long int *out);
int aem_stringslice_match_long_base(struct aem_stringslice *slice, int base, long int *out);
int aem_stringslice_match_uint_base(struct aem_stringslice *slice, int base, unsigned int *out);
int aem_stringslice_match_int_base(struct aem_stringslice *slice, int base, int *out);
int aem_stringslice_match_long_auto(struct aem_stringslice *slice, long int *out);

// TODO: inconsistency: this function returns -1 on failure and 0 on success,
// while most other functions in this file that only use their return value to
// indicate status return 0 on failure and 1 on success.
aem_deprecated_msg("Use aem_stringslice_match_int_base instead") static inline int aem_stringslice_match_int(struct aem_stringslice *slice, int base, int *out)
{
	return !aem_stringslice_match_int_base(slice, base, out);
}

#endif /* AEM_STRINGSLICE_H */
