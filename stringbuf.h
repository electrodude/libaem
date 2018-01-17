#ifndef AEM_STRINGBUF_H
#define AEM_STRINGBUF_H

#define AEM_STRINGBUF_DEBUG 0

#if AEM_STRINGBUF_DEBUG
#include "log.h"
#endif

#include "stringslice.h"

// String builder class
// N.B.: The null terminator is not present on str->s except after calls to aem_stringbuf_get
//  and functions that call it, such as aem_stringbuf_shrinkwrap and aem_stringbuf_release

enum aem_stringbuf_storage
{
	AEM_STRINGBUF_STORAGE_HEAP = 0,
	AEM_STRINGBUF_STORAGE_UNOWNED,
};

struct aem_stringbuf
{
	char *s;          // pointer to buffer
	size_t n;         // current length of string
	                  //  (not counting null terminator)
	size_t maxn;      // allocated length of buffer

	enum aem_stringbuf_storage storage; // whether we own the storage
	int bad     : 1;  // error flag: memory allocation error or .fixed = 1 but size exceeded
	int fixed   : 1;  // can't be realloced

};

#ifndef AEM_STRINGBUF_PREALLOC_LEN
// This may have different values in different files.
#define AEM_STRINGBUF_PREALLOC_LEN 32
#endif

// you can use this without calling aem_stringbuf_init() on it first because realloc(NULL, size) === malloc(size)
#define AEM_STRINGBUF_EMPTY ((struct aem_stringbuf){0})

// Initializes a stringbuf and allocates storage for it on the stack.
// If the storage on the stack proves to be too small, it is copied into a
//  malloc()'d buffer and is then treated like any other stringbuf.
// You must call aem_stringbuf_dtor on this unless you're absolutely sure that it
//  wasn't copied into a malloc()'d buffer.  You can ensure that it won't be
//  copied into a malloc()'d buffer (instead, characters will just stop getting
//  added when it gets full) if you set .fixed = 1.
#define AEM_STRINGBUF_ON_STACK(_name, _len) \
	char _name##_stringbuf_data[_len]; \
	struct aem_stringbuf _name = {.s=_name##_stringbuf_data, .n=0, .maxn = _len, .storage = AEM_STRINGBUF_STORAGE_UNOWNED};

// Allocates a new stringbuf and initialize it to all zeros (sufficiently
//  initialized for immediate use, see AEM_STRINGBUF_EMPTY).
struct aem_stringbuf *aem_stringbuf_new_raw(void);

// Create a new string, given how many bytes to preallocate
struct aem_stringbuf *aem_stringbuf_init_prealloc(struct aem_stringbuf *str, size_t maxn);
#define aem_stringbuf_new_prealloc(maxn) aem_stringbuf_init_prealloc(aem_stringbuf_new_raw(), maxn)

// Create a new string
#define aem_stringbuf_init(str) aem_stringbuf_init_prealloc(str, (AEM_STRINGBUF_PREALLOC_LEN))
#define aem_stringbuf_new() aem_stringbuf_init(aem_stringbuf_new_raw())

// Create a new malloc'd string from a stringslice
static inline struct aem_stringbuf *aem_stringbuf_init_array(struct aem_stringbuf *restrict str, size_t n, const char *s);
#define aem_stringbuf_new_array(n, s) aem_stringbuf_init_array(aem_stringbuf_new_raw(), n, s)

// Create a new malloc'd string from a stringslice
static inline struct aem_stringbuf *aem_stringbuf_init_slice(struct aem_stringbuf *restrict str, const char *start, const char *end);
#define aem_stringbuf_new_slice(start, end) aem_stringbuf_init_slice(aem_stringbuf_new_raw(), start, end)

// Create a new malloc'd string and set it to null-terminated string s
static inline struct aem_stringbuf *aem_stringbuf_init_cstr(struct aem_stringbuf *restrict str, const char *s);
#define aem_stringbuf_new_cstr(s) aem_stringbuf_init_cstr(aem_stringbuf_new_raw(), s)

// Create a new malloc'd string and set it to orig
// Clone a string.
static inline struct aem_stringbuf *aem_stringbuf_init_str(struct aem_stringbuf *restrict str, const struct aem_stringbuf *orig);
#define aem_stringbuf_new_str(str) aem_stringbuf_init_str(aem_stringbuf_new_raw(), str)
#define aem_stringbuf_dup(orig) aem_stringbuf_new_str(orig)

// Belongs in stringslice.h, but can't because inline and dereferencing pointer to incomplete type
static inline struct aem_stringslice aem_stringslice_new_str(const struct aem_stringbuf *orig);

// Free a malloc'd stringbuf and its buffer.
void aem_stringbuf_free(struct aem_stringbuf *str);

// Free a non-malloc'd stringbuf's buffer.
void aem_stringbuf_dtor(struct aem_stringbuf *str);

// Free malloc'd stringbuf, returning its internal buffer.
// The caller assumes responsibilty for free()ing the returned buffer.
// Just use aem_stringbuf_get if str is on the stack.  TODO: This doesn't work with AEM_STRINGBUF_ON_STACK.
// Appends null terminator
char *aem_stringbuf_release(struct aem_stringbuf *str);

// Get a pointer to the end of a string
static inline char *aem_stringbuf_end(struct aem_stringbuf *str)
{
	return &str->s[str->n];
}

// Reset a string to zero length
// This does nothing but str->n = 0; it neither null-terminates it
// nor shrinks the allocated size of the internal buffer.
static inline void aem_stringbuf_reset(struct aem_stringbuf *str)
{
	str->n = 0;
	str->bad = 0;
}

// Ensure that at least len bytes are allocated
void aem_stringbuf_grow(struct aem_stringbuf *str, size_t maxn_new);
// Ensure that at least len + 1 bytes are available
static inline void aem_stringbuf_reserve(struct aem_stringbuf *str, size_t len);

// Return the number of available allocated bytes
static inline int aem_stringbuf_available(struct aem_stringbuf *str)
{
	return str->maxn - str->n - 1;
}

// realloc() internal buffer to be as small as possible
// Appends null terminator
// Returns pointer to internal buffer
// The pointer is only valid until the next call to aem_stringbuf_put*.
// The null terminator is only there until the next time the string is modified
char *aem_stringbuf_shrinkwrap(struct aem_stringbuf *str);

// deprecated old name
#define aem_stringbuf_shrink aem_stringbuf_shrinkwrap


// Append a character
static inline void aem_stringbuf_putc(struct aem_stringbuf *str, char c);

// Append a UTF-8 codepoint
// Implementation in utf8.c
int aem_stringbuf_put_utf8(struct aem_stringbuf *str, unsigned int c);

// Append a null-terminated string
static inline void aem_stringbuf_puts(struct aem_stringbuf *str, const char *s);

// Append a null-terminated string, but do not let the result exceed n characters
static inline void aem_stringbuf_puts_limit(struct aem_stringbuf *str, size_t len, const char *s);

// Append a string that is n characters long
static inline void aem_stringbuf_putn(struct aem_stringbuf *str, size_t n, const char *s);

// Append a hex byte
static inline void aem_stringbuf_puthex(struct aem_stringbuf *str, unsigned char byte);

// Append a number
static inline void aem_stringbuf_putnum(struct aem_stringbuf *str, int base, int num);

// Append printf-formatted text
#define aem_stringbuf_putf(str, fmt, ...) aem_stringbuf_printf(str, fmt, __VA_ARGS__)
#define aem_stringbuf_append_printf(str, fmt, ...) aem_stringbuf_printf(str, fmt, __VA_ARGS__)
void aem_stringbuf_printf(struct aem_stringbuf *str, const char *fmt, ...);

/*
// Ensure there are at least len available bytes,
// and return a pointer to the current end.
char *aem_stringbuf_append_manual(struct aem_stringbuf *str, size_t len);
*/

// Append a stringbuf
static inline void aem_stringbuf_append(struct aem_stringbuf *str, const struct aem_stringbuf *str2)
{
	if (str2 == NULL) return;
	aem_stringbuf_putn(str, str2->n, str2->s);
}

// Append a character, escaping it if necessary
void aem_stringbuf_putq(struct aem_stringbuf *str, char c);

// Append a struct stringslice, escaping characters as necessary
void aem_stringbuf_append_stringslice_quote(struct aem_stringbuf *str, const struct aem_stringslice *slice);

// Append a stringbuf, escaping characters as necessary
static inline void aem_stringbuf_append_quote(struct aem_stringbuf *str, const struct aem_stringbuf *str2)
{
	if (str2 == NULL) return;
	struct aem_stringslice slice = aem_stringslice_new_str(str2);
	aem_stringbuf_append_stringslice_quote(str, &slice);
}

// Append a stringslice
static inline void aem_stringbuf_append_stringslice(struct aem_stringbuf *str, struct aem_stringslice slice)
{
	aem_stringbuf_putn(str, aem_stringslice_len(&slice), slice.start);
}

// Append the data starting at start and ending at the byte before end
static inline void aem_stringbuf_append_slice(struct aem_stringbuf *str, const char *start, const char *end)
{
	aem_stringbuf_append_stringslice(str, aem_stringslice_new(start, end));
}

// Unquote quoted characters out of slice and append it to str until unquoted stuff is found
int aem_stringbuf_append_unquote(struct aem_stringbuf *restrict str, struct aem_stringslice *restrict slice);

void aem_stringbuf_pad(struct aem_stringbuf *str, size_t len, char c);

// Set a string to a character, clearing it first
// Is equivalent to aem_stringbuf_reset followed by aem_stringbuf_putc.
static inline void aem_stringbuf_setc(struct aem_stringbuf *str, char c)
{
	aem_stringbuf_reset(str);
	aem_stringbuf_putc(str, c);
}

// Set a string to a null-terminated string, clearing it first
// Is equivalent to aem_stringbuf_reset followed by aem_stringbuf_puts.
static inline void aem_stringbuf_sets(struct aem_stringbuf *str, const char *s)
{
	aem_stringbuf_reset(str);
	aem_stringbuf_puts(str, s);
}

// Set a string to a string that is n characters long, clearing it first
// Is equivalent to aem_stringbuf_reset followed by aem_stringbuf_putn.
static inline void aem_stringbuf_setn(struct aem_stringbuf *str, size_t n, const char *s)
{
	aem_stringbuf_reset(str);
	aem_stringbuf_putn(str, n, s);
}

// Set a string to a stringslice, clearing it first
// Is equivalent to aem_stringbuf_reset followed by aem_stringbuf_append_stringslice.
static inline void aem_stringbuf_setslice(struct aem_stringbuf *str, const struct aem_stringslice slice)
{
	aem_stringbuf_reset(str);
	aem_stringbuf_append_stringslice(str, slice);
}

// Set a string to another string, clearing it first
// Is equivalent to aem_stringbuf_reset followed by aem_stringbuf_append.
static inline void aem_stringbuf_setstr(struct aem_stringbuf *str, const struct aem_stringbuf *str2)
{
	aem_stringbuf_reset(str);
	aem_stringbuf_append(str, str2);
}

// Appends null terminator
// Returns pointer to internal buffer
// The pointer is only valid until the next call to aem_stringbuf_reserve or aem_stringbuf_shrinkwrap.
static inline char *aem_stringbuf_get(struct aem_stringbuf *str)
{
	if (str == NULL) return NULL;
	if (str->s == NULL) return NULL;
	if (str->bad) return NULL;

	str->s[str->n] = 0; // null-terminate the string
	                    //  (there is room already allocated)
	return str->s;
}

// Return the i-th character from the beginning of the struct stringbuf, or -1 if out of range
int aem_stringbuf_index(struct aem_stringbuf *str, size_t i);

// Set the i-th character from the beginning of the struct stringbuf
// Increase the size of the struct stringbuf if necessary
void aem_stringbuf_assign(struct aem_stringbuf *str, size_t i, char c);

size_t aem_stringbuf_fread(struct aem_stringbuf *str, FILE *fp);
int aem_stringbuf_file_read(struct aem_stringbuf *str, FILE *fp);

int aem_stringbuf_file_write(const struct aem_stringbuf *str, FILE *fp);


#ifdef __unix__
int aem_stringbuf_fd_read(struct aem_stringbuf *str, int fd);
int aem_stringbuf_fd_read_n(struct aem_stringbuf *str, size_t n, int fd);

int aem_stringbuf_fd_write(const struct aem_stringbuf *str, int fd);
#endif



// inline implementations of frequently called functions

static inline struct aem_stringbuf *aem_stringbuf_init_array(struct aem_stringbuf *restrict str, size_t n, const char *s)
{
	aem_stringbuf_init_prealloc(str, n+1);
	aem_stringbuf_putn(str, n, s);
	return str;
}

static inline struct aem_stringbuf *aem_stringbuf_init_slice(struct aem_stringbuf *restrict str, const char *start, const char *end)
{
	aem_stringbuf_init_prealloc(str, end - start + 1);
	aem_stringbuf_append_slice(str, start, end);
	return str;
}

static inline struct aem_stringbuf *aem_stringbuf_init_cstr(struct aem_stringbuf *restrict str, const char *s)
{
	aem_stringbuf_init(str);
	aem_stringbuf_puts(str, s);
	return str;
}

static inline struct aem_stringbuf *aem_stringbuf_init_str(struct aem_stringbuf *restrict str, const struct aem_stringbuf *orig)
{
	aem_stringbuf_init_prealloc(str, orig->maxn);
	aem_stringbuf_append(str, orig);
	return str;
}

// Belongs in stringslice.h, but can't because inline and dereferencing pointer to incomplete type
static inline struct aem_stringslice aem_stringslice_new_str(const struct aem_stringbuf *str)
{
	return aem_stringslice_new_len(str->s, str->n);
}

static inline void aem_stringbuf_reserve(struct aem_stringbuf *str, size_t len)
{
	size_t n = str->n + len;
	// make room for new stuff and null terminator
	if (str->maxn < n + 1)
	{
		aem_stringbuf_grow(str, (n + 1)*2);
	}
}

static inline void aem_stringbuf_putc(struct aem_stringbuf *str, char c)
{
	if (str == NULL) return;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "putc(\"%s\", '%c')\n", aem_stringbuf_get(str), c);
#endif

	aem_stringbuf_reserve(str, 1);
	if (str->bad) return;

	str->s[str->n++] = c;
}

static inline void aem_stringbuf_puts(struct aem_stringbuf *restrict str, const char *restrict s)
{
	if (str == NULL) return;
	if (str->bad) return;

	if (s == NULL) return;

#if 0
	for (;*s;s++)
	{
		aem_stringbuf_putc(str, *s);
	}
#else
	aem_stringbuf_putn(str, strlen(s), s);
#endif
}

static inline void aem_stringbuf_puts_limit(struct aem_stringbuf *restrict str, size_t len, const char *restrict s)
{
	if (str == NULL) return;

	if (s == NULL) return;

	//aem_stringbuf_reserve(len);
	if (str->bad) return;

	for (; *s && str->n < len; s++)
	{
		aem_stringbuf_putc(str, *s);
	}
}

static inline void aem_stringbuf_putn(struct aem_stringbuf *restrict str, size_t n, const char *restrict s)
{
	if (str == NULL) return;

	if (s == NULL) return;

	aem_stringbuf_reserve(str, n);
	if (str->bad) return;

	memcpy(aem_stringbuf_end(str), s, n);

	str->n += n;
}

static const char aem_stringbuf_putnum_digits[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static inline void aem_stringbuf_putnum(struct aem_stringbuf *str, int base, int num)
{
	if (num < 0)
	{
		aem_stringbuf_putc(str, '-');
		num = -num;
	}

	int top = num / base;
	if (top > 0)
	{
		aem_stringbuf_putnum(str, top, base);
	}
	aem_stringbuf_putc(str, aem_stringbuf_putnum_digits[num % base]);
}

static inline void aem_stringbuf_puthex(struct aem_stringbuf *str, unsigned char byte)
{
	aem_stringbuf_putc(str, aem_stringbuf_putnum_digits[(byte >> 4) & 0xF]);
	aem_stringbuf_putc(str, aem_stringbuf_putnum_digits[(byte     ) & 0xF]);
}

#endif /* AEM_STRINGBUF_H */
