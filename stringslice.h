#ifndef AEM_STRINGSLICE_H
#define AEM_STRINGSLICE_H

#include <stdio.h>
#include <string.h>

struct stringslice
{
	const char *start;
	const char *end;
};

#define stringslice_new(p, pe) ((struct stringslice){.start = p, .end = pe})
#define STRINGSLICE_EMPTY stringslice_new(NULL, NULL)

static inline struct stringslice stringslice_new_len(const char *p, size_t n)
{
	return stringslice_new(p, &p[n]);
}

static inline struct stringslice stringslice_new_cstr(const char *p)
{
	return stringslice_new_len(p, strlen(p));
}

#define stringslice_new_sizeof(char_arr) stringslice_new_len(char_arr, sizeof(char_arr))

int stringslice_file_write(const struct stringslice *slice, FILE *fp);

static inline int stringslice_ok(const struct stringslice *slice)
{
	return slice->start != slice->end;
}

static inline size_t stringslice_len(const struct stringslice *slice)
{
	return slice->end - slice->start;
}

static inline int stringslice_getc(struct stringslice *slice)
{
	if (!stringslice_ok(slice)) return -1;

	return (unsigned char)*slice->start++;
}

// Get a UTF-8 codepoint
// Implementation in utf8.c
int stringslice_get_utf8(struct stringslice *slice);

void stringslice_match_ws(struct stringslice *slice);

struct stringslice stringslice_match_word(struct stringslice *slice);
struct stringslice stringslice_match_line(struct stringslice *slice);

int stringslice_match(struct stringslice *slice, const char *s);

int stringslice_eq(struct stringslice slice, const char *s);

int stringslice_match_hexbyte(struct stringslice *slice);

#endif /* AEM_STRINGSLICE_H */
