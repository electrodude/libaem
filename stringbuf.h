#ifndef AEM_STRINGBUF_H
#define AEM_STRINGBUF_H

#include <alloca.h>
#include <stdarg.h>
#include <stdint.h>
#ifdef __unix__
#include <unistd.h>
#endif

#define AEM_STRINGBUF_DEBUG 0

#include <aem/log.h>

#include <aem/stringslice.h>

// String builder class
// N.B.: The null terminator is not present on str->s except after calls to aem_stringbuf_get
//  and functions that call it, such as aem_stringbuf_shrinkwrap and aem_stringbuf_release

enum aem_stringbuf_storage {
	AEM_STRINGBUF_STORAGE_HEAP = 0,
	AEM_STRINGBUF_STORAGE_UNOWNED,
};

struct aem_stringbuf {
	char *s;          // Pointer to buffer
	// It's a little late now, but these two should have been named nr and alloc
	size_t n;         // Current length of string
	                  //  (not counting null terminator)
	size_t maxn;      // Allocated length of buffer

	enum aem_stringbuf_storage storage; // Whether we own the storage
	char bad    : 1;  // Error flag: memory allocation error or .fixed = 1 but size exceeded
	char fixed  : 1;  // Can't be realloc'ed
};

// Initialize new instances to this value
#define AEM_STRINGBUF_EMPTY ((struct aem_stringbuf){0})

// Initializes a stringbuf and allocates storage for it on the stack.
// If the storage on the stack proves to be too small, it is copied into a
//  malloc()'d buffer and is then treated like any other stringbuf.
// You must call aem_stringbuf_dtor on this unless you're absolutely certain that it
//  wasn't copied into a malloc()'d buffer.  You can ensure that it won't be
//  copied into a malloc()'d buffer (instead, characters will just stop getting
//  added once it's full) if you set .fixed = 1.
#define AEM_STRINGBUF_ALLOCA(_len) \
	((struct aem_stringbuf){.s=alloca(_len), .n=0, .maxn = (_len), .storage = AEM_STRINGBUF_STORAGE_UNOWNED})

// Deprecated in favor of AEM_STRINGBUF_ALLOCA, and is indeed now just a macro for it.
#define AEM_STRINGBUF_ON_STACK(_name, _len) \
	struct aem_stringbuf _name = AEM_STRINGBUF_ALLOCA(_len);

// malloc() and initialize a new aem_stringbuf
struct aem_stringbuf *aem_stringbuf_new(void);

// Initialize a stringbuf, preallocating the given number of bytes
struct aem_stringbuf *aem_stringbuf_init_prealloc(struct aem_stringbuf *str, size_t maxn);
#define aem_stringbuf_new_prealloc(maxn) aem_stringbuf_init_prealloc(aem_stringbuf_new(), maxn)

// Create a new string
static inline struct aem_stringbuf *aem_stringbuf_init(struct aem_stringbuf *str)
{
	if (!str)
		return str;

	*str = AEM_STRINGBUF_EMPTY;

	return str;
}

// Free a malloc'd stringbuf and its buffer.
void aem_stringbuf_free(struct aem_stringbuf *str);

// Free a stringbuf's buffer and reset it to its initial state.
void aem_stringbuf_dtor(struct aem_stringbuf *str);

// Free malloc'd stringbuf, returning its internal buffer and writing the
// number of elements to n
// The caller assumes responsibilty for free()ing the returned buffer.
// Just use aem_stringbuf_get if str is on the stack.  TODO: This doesn't work with AEM_STRINGBUF_ON_STACK.
// Appends null terminator
char *aem_stringbuf_release(struct aem_stringbuf *str, size_t *n_p);

// Get a pointer to the end of a string
static inline char *aem_stringbuf_end(struct aem_stringbuf *str)
{
	aem_assert(str);
	return &str->s[str->n];
}

// Reset string length to zero
// This does nothing but str->n = 0; it neither null-terminates it
// nor shrinks the allocated size of the internal buffer.
static inline void aem_stringbuf_reset(struct aem_stringbuf *str)
{
	if (!str)
		return;
	str->n = 0;
	str->bad = 0;
}

// Return the number of available allocated bytes
static inline int aem_stringbuf_available(struct aem_stringbuf *str)
{
	aem_assert(str);
	return str->maxn - str->n - 1;
}

// realloc() internal buffer to be as small as possible
// Returns pointer to internal buffer
// Null-terminates the string.
char *aem_stringbuf_shrinkwrap(struct aem_stringbuf *str);

// Ensure that at least len + 1 bytes are available
int aem_stringbuf_reserve(struct aem_stringbuf *str, size_t len);
// Ensure the total size of the buffer is at least len
int aem_stringbuf_reserve_total(struct aem_stringbuf *str, size_t len);

// Append a character
static inline void aem_stringbuf_putc(struct aem_stringbuf *str, char c);

// Append a UTF-8 rune
// Implementation in utf8.c
#define aem_stringbuf_put aem_stringbuf_put_rune
int aem_stringbuf_put_rune(struct aem_stringbuf *str, uint32_t c);

// Append a null-terminated string
static inline void aem_stringbuf_puts(struct aem_stringbuf *str, const char *s);

// Append a null-terminated string, but do not let the result exceed n characters
static inline void aem_stringbuf_puts_limit(struct aem_stringbuf *str, size_t len, const char *s);

// Append a string that is n characters long
static inline void aem_stringbuf_putn(struct aem_stringbuf *str, size_t n, const char *s);

// Append an integer.
#define aem_stringbuf_putnum aem_stringbuf_putint
void aem_stringbuf_putint(struct aem_stringbuf *str, int base, int num);

// Append a hex byte
void aem_stringbuf_puthex(struct aem_stringbuf *str, unsigned char byte);

// Append printf-formatted text.
void aem_stringbuf_vprintf(struct aem_stringbuf *str, const char *fmt, va_list argp);
void aem_stringbuf_printf(struct aem_stringbuf *str, const char *fmt, ...);

// Append another stringbuf.
static inline void aem_stringbuf_append(struct aem_stringbuf *str, const struct aem_stringbuf *str2);
#define aem_stringbuf_concat aem_stringbuf_append

// Append a character, escaping it if necessary (deprecated and replaced).
#define aem_stringbuf_putq aem_string_escape_rune

// Append a stringslice.
static inline void aem_stringbuf_putss(struct aem_stringbuf *str, struct aem_stringslice slice)
{
	aem_stringbuf_putn(str, aem_stringslice_len(slice), slice.start);
}

// Pad str with character c until it is len bytes long
void aem_stringbuf_pad(struct aem_stringbuf *str, size_t len, char c);

// Return pointer to internal buffer, after ensuring string is null terminated.
// The pointer is only valid until the next call to aem_stringbuf_reserve or aem_stringbuf_shrinkwrap.
static inline char *aem_stringbuf_get(struct aem_stringbuf *str)
{
	if (!str)
		return NULL;
	if (!str->s)
		return NULL;
	if (str->bad) // ???
		return NULL;

	str->s[str->n] = 0; // Null-terminate the string
	                    //  (there is room already allocated)
	return str->s;
}

// Return the i-th character from the beginning of the stringbuf, or -1 if out of range.
int aem_stringbuf_index(struct aem_stringbuf *str, size_t i);

// Set the i-th character from the beginning of the stringbuf.
// Increases the size of the stringbuf if necessary.
void aem_stringbuf_assign(struct aem_stringbuf *str, size_t i, char pad, char c);

// Removes trailing whitespace from end of string
void aem_stringbuf_rtrim(struct aem_stringbuf *str);

// Removes characters from the beginning of a stringbuf.
void aem_stringbuf_pop_front(struct aem_stringbuf *str, size_t n);

size_t aem_stringbuf_file_read(struct aem_stringbuf *str, size_t n, FILE *fp);
int aem_stringbuf_file_read_all(struct aem_stringbuf *str, FILE *fp);
int aem_stringbuf_file_write(const struct aem_stringbuf *str, FILE *fp);


#ifdef __unix__
ssize_t aem_stringbuf_fd_read(struct aem_stringbuf *str, size_t n, int fd);
int aem_stringbuf_fd_read_all(struct aem_stringbuf *str, int fd);
#define aem_stringbuf_fd_write(_str, _fd) (aem_stringslice_fd_write(aem_stringslice_new_str(_str), (_fd)))
#endif



/// Inline implementations of frequently called functions

// Really belongs in stringslice.h, but can't because inline and dereferencing pointer to incomplete type.
static inline struct aem_stringslice aem_stringslice_new_str(const struct aem_stringbuf *str)
{
	aem_assert(str);
	return aem_stringslice_new_len(str->s, str->n);
}

static inline void aem_stringbuf_putc(struct aem_stringbuf *str, char c)
{
	aem_assert(str);

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "putc(\"%s\", '%c')", aem_stringbuf_get(str), c);
#endif

	aem_stringbuf_reserve(str, 1);
	if (str->bad)
		return;

	str->s[str->n++] = c;
}

static inline void aem_stringbuf_puts(struct aem_stringbuf *restrict str, const char *restrict s)
{
	aem_assert(str);

	if (str->bad)
		return;

	if (!s)
		return;

#if 0
	while (*s)
		aem_stringbuf_putc(str, *s++);
#else
	aem_stringbuf_putn(str, strlen(s), s);
#endif
}

static inline void aem_stringbuf_puts_limit(struct aem_stringbuf *restrict str, size_t len, const char *restrict s)
{
	aem_assert(str);

	if (!s)
		return;

	//aem_stringbuf_reserve(len);
	if (str->bad)
		return;

	while (*s && str->n < len)
		aem_stringbuf_putc(str, *s++);
}

static inline void aem_stringbuf_putn(struct aem_stringbuf *restrict str, size_t n, const char *restrict s)
{
	if (!n)
		return;

	aem_assert(str);
	aem_assert(s);

	aem_stringbuf_reserve(str, n);
	if (str->bad)
		return;

	memcpy(aem_stringbuf_end(str), s, n);

	str->n += n;
}

static inline void aem_stringbuf_append(struct aem_stringbuf *str, const struct aem_stringbuf *str2)
{
	aem_assert(str2);

	aem_stringbuf_putn(str, str2->n, str2->s);
}

#endif /* AEM_STRINGBUF_H */
