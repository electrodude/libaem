#ifndef AEM_STRINGSLICE_H
#define AEM_STRINGSLICE_H

#include <stdio.h>
#include <string.h>
#ifdef __unix__
#include <unistd.h>
#endif

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

static inline struct aem_stringslice aem_stringslice_new_cstr(const char *p)
{
	return aem_stringslice_new_len(p, strlen(p));
}

#define aem_stringslice_new_sizeof(char_arr) aem_stringslice_new_len((char_arr), sizeof(char_arr))


int aem_stringslice_file_write(struct aem_stringslice *slice, FILE *fp);

#ifdef __unix__
ssize_t aem_stringslice_fd_write(struct aem_stringslice *slice, int fd);
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
	if (!aem_stringslice_ok(*slice)) return -1;

	return (unsigned char)*slice->start++;
}

// Unget (move backwards) one byte
// Only use this if you know there is a byte to unget.
static inline int aem_stringslice_ungetc(struct aem_stringslice *slice)
{
	return (unsigned char)*--slice->start;
}

// Get a UTF-8 rune
// Implementation in utf8.c
int aem_stringslice_get(struct aem_stringslice *slice);
// Old deprecated name
#define aem_stringslice_get_utf8 aem_stringslice_get

// Get raw data
// Reads `count` bytes into `buf`.
// If fewer than `count` bytes are available, does nothing and returns -1.
static inline int aem_stringslice_read_data(struct aem_stringslice *slice, void *buf, size_t count) {
	if (aem_stringslice_len(*slice) < count) return -1;

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

// Consume a CR, CRLF, or LF at the current position.
// Returns 1 for LF, 2 for CR, 3 for CRLF, or 0 on failure
int aem_stringslice_match_newline(struct aem_stringslice *slice);

struct aem_stringslice aem_stringslice_match_alnum(struct aem_stringslice *slice);
struct aem_stringslice aem_stringslice_match_word(struct aem_stringslice *slice);
struct aem_stringslice aem_stringslice_match_line(struct aem_stringslice *slice);

int aem_stringslice_match(struct aem_stringslice *slice, const char *s);

int aem_stringslice_eq(struct aem_stringslice slice, const char *s);

int aem_stringslice_cmp(struct aem_stringslice s0, struct aem_stringslice s1);

int aem_stringslice_match_hexbyte(struct aem_stringslice *slice);
int aem_stringslice_match_int(struct aem_stringslice *slice, int base, int *out);

#endif /* AEM_STRINGSLICE_H */
