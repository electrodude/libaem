#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// for vsnprintf
#include <stdio.h>

#ifdef __unix__
#include <errno.h>
#endif

#define AEM_INTERNAL
#include "stringbuf.h"

#define AEM_STRINGBUF_DEBUG_ALLOC 0

struct aem_stringbuf *aem_stringbuf_new_raw(void)
{
	struct aem_stringbuf *str = malloc(sizeof(*str));

	if (!str) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc() failed: %s", strerror(errno));
		return NULL;
	}

	*str = AEM_STRINGBUF_EMPTY;

	return str;
}

struct aem_stringbuf *aem_stringbuf_init_prealloc(struct aem_stringbuf *str, size_t maxn)
{
	if (!str)
		return NULL;

	str->n = 0;
	str->maxn = maxn;
	str->s = malloc(str->maxn);
	str->bad = 0;
	str->fixed = 0;
	str->storage = AEM_STRINGBUF_STORAGE_HEAP;

	return str;
}

void aem_stringbuf_free(struct aem_stringbuf *str)
{
	if (!str)
		return;
	if (str->bad)
		return;

	aem_stringbuf_dtor(str);

	free(str);
}

static inline void aem_stringbuf_storage_free(struct aem_stringbuf *str)
{
	aem_assert(str);
	switch (str->storage) {
		case AEM_STRINGBUF_STORAGE_HEAP:
			aem_assert(str->s);
			free(str->s);
			break;

		case AEM_STRINGBUF_STORAGE_UNOWNED:
			// not our responsibility; don't worry about it
			break;

		default:
			aem_logf_ctx(AEM_LOG_BUG, "%p: unknown storage type %d, leaking %p!", str, str->storage, str->s);
			break;
	}
}

void aem_stringbuf_dtor(struct aem_stringbuf *str)
{
	if (!str)
		return;

	str->n = 0;

	if (str->s)
		aem_stringbuf_storage_free(str);

	str->s = NULL;
	str->maxn = 0;
}

char *aem_stringbuf_release(struct aem_stringbuf *str)
{
	if (!str)
		return NULL;

	aem_stringbuf_shrinkwrap(str);

	char *s = str->s;

	free(str);

	return s;
}


void aem_stringbuf_grow(struct aem_stringbuf *str, size_t maxn_new)
{
	aem_assert(str);

	if (str->bad)
		return;

	aem_assert(maxn_new >= str->n+1);

	if (str->fixed) {
		str->bad = 1;
		return;
	}

#if 0
	size_t maxn_old = str->maxn;
#endif
	if (str->storage == AEM_STRINGBUF_STORAGE_HEAP) {
#if AEM_STRINGBUF_DEBUG_ALLOC
		aem_logf_ctx(AEM_LOG_DEBUG, "realloc: n %zd, maxn %zd -> %zd", str->n, str->maxn, maxn_new);
#endif
		char *s_new = realloc(str->s, maxn_new);

		if (!s_new) {
			str->bad = 1;
			return;
		}

		str->s = s_new;
		str->maxn = maxn_new;
	} else {
#if AEM_STRINGBUF_DEBUG_ALLOC
		aem_logf_ctx(AEM_LOG_DEBUG, "to heap: n %zd, maxn %zd -> %zd", str->n, str->maxn, maxn_new);
#endif

		char *s_new = malloc(maxn_new);

		if (!s_new) {
			str->bad = 1;
			return;
		}

		memcpy(s_new, str->s, str->n);

		aem_stringbuf_storage_free(str); // free old storage

		str->storage = AEM_STRINGBUF_STORAGE_HEAP;
		str->s = s_new;
		str->maxn = maxn_new;
	}

#if 0
	if (maxn_old) {
		str->bad = 1;
	}
#endif
}


char *aem_stringbuf_shrinkwrap(struct aem_stringbuf *str)
{
	if (!str)
		return NULL;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "%p", aem_stringbuf_get(str));
#endif

	if (!str->fixed && str->storage == AEM_STRINGBUF_STORAGE_HEAP) {
		size_t maxn_new = str->n + 1;
		char *s_new = realloc(str->s, maxn_new);

		if (s_new) {
			str->s = s_new;
			str->maxn = maxn_new;
		}
	}

	return aem_stringbuf_get(str);
}

void aem_stringbuf_pop_front(struct aem_stringbuf *str, size_t n)
{
	aem_assert(str);

	if (n >= str->n) {
		aem_stringbuf_reset(str);
		return;
	}

	memmove(str->s, &str->s[n], str->n - n);
	str->n -= n;
}

static const char aem_stringbuf_putint_digits[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
void aem_stringbuf_putint(struct aem_stringbuf *str, int base, int num)
{
	if (num < 0) {
		aem_stringbuf_putc(str, '-');
		num = -num;
	}

	int top = num / base;
	if (top > 0) {
		aem_stringbuf_putint(str, top, base);
	}
	aem_stringbuf_putc(str, aem_stringbuf_putint_digits[num % base]);
}
void aem_stringbuf_puthex(struct aem_stringbuf *str, unsigned char byte)
{
	aem_stringbuf_putc(str, aem_stringbuf_putint_digits[(byte >> 4) & 0xF]);
	aem_stringbuf_putc(str, aem_stringbuf_putint_digits[(byte     ) & 0xF]);
}

// TODO: can these actually safely be restrict?
static inline int aem_stringbuf_vprintf_try(struct aem_stringbuf *restrict str, const char *restrict fmt, va_list argp)
{
	return vsnprintf(aem_stringbuf_end(str), aem_stringbuf_available(str)+1, fmt, argp);
}
void aem_stringbuf_vprintf(struct aem_stringbuf *restrict str, const char *restrict fmt, va_list argp)
{
	if (!str)
		return;
	if (str->bad)
		return;

	if (!fmt)
		return;

	va_list argp2;
	va_copy(argp2, argp);
	int len = aem_stringbuf_vprintf_try(str, fmt, argp2);
	va_end(argp2);

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "%p: %zd out of %zd, %s", str, len, aem_stringbuf_available(str)+1, fmt);
#endif

	if (len > aem_stringbuf_available(str)) {
		// If the first time didn't have enough space, get more and try again.
		// This might reserve one more byte than we actually need.
		aem_stringbuf_reserve(str, len+2);
		len = aem_stringbuf_vprintf_try(str, fmt, argp);
		// We can't safely assert here because aem_log* isn't
		// re-entrant, is called by aem_assert, and calls this
		// function.  So we have to settle for just truncating the
		// message if it somehow doesn't fit.  But this case should be
		// impossible, anyway.
		if (len > aem_stringbuf_available(str))
			len = aem_stringbuf_available(str);
	}

	str->n += len;
	return;
}

void aem_stringbuf_printf(struct aem_stringbuf *restrict str, const char *restrict fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	aem_stringbuf_vprintf(str, fmt, argp);
	va_end(argp);
}

void aem_stringbuf_pad(struct aem_stringbuf *str, size_t len, char c)
{
	aem_assert(str);

	aem_stringbuf_reserve(str, len);

	while (str->n < len && !str->bad) {
		aem_stringbuf_putc(str, c);
	}
}

int aem_stringbuf_index(struct aem_stringbuf *str, size_t i)
{
	if (!str)
		return -1;

	if (i >= str->n) {
		return -1;
	}
#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "[%zd] = %c", i, str->s[i]);
#endif

	return str->s[i];
}

void aem_stringbuf_assign(struct aem_stringbuf *str, size_t i, char pad, char c)
{
	aem_assert(str);

	size_t n_new = i + 1;
	if (n_new > str->n) {
		aem_stringbuf_reserve_total(str, n_new);
		for (size_t j = str->n; j < n_new && j < str->maxn; j++)
			str->s[j] = pad;
		str->n = n_new;
	}

	str->s[i] = c;
}


void aem_stringbuf_rtrim(struct aem_stringbuf *str)
{
	aem_assert(str);

	while (str->n && isspace(str->s[str->n-1]))
		str->n--;
}


size_t aem_stringbuf_file_read(struct aem_stringbuf *str, size_t n, FILE *fp)
{
	aem_assert(str);

	if (!fp) {
		aem_logf_ctx(AEM_LOG_ERROR, "fp == NULL!");
		return -1;
	}

	aem_stringbuf_reserve(str, n);

	size_t in = fread(aem_stringbuf_end(str), 1, n, fp);

	//aem_logf_ctx(AEM_LOG_DEBUG, "read %zd: n = %zd, maxn = %zd", in, str->n, str->maxn);

	str->n += in;

	return in;
}

int aem_stringbuf_file_read_all(struct aem_stringbuf *str, FILE *fp)
{
	aem_assert(str);

	if (!fp) {
		aem_logf_ctx(AEM_LOG_ERROR, "fp == NULL!");
		return -1;
	}

	ssize_t in;
	do {
		in = aem_stringbuf_file_read(str, 4096, fp);
	} while (in > 0 || (!feof(fp) && !ferror(fp)));

	if (feof(fp)) {
		return 1;
	} else if (ferror(fp)) {
		return -1;
	}

	aem_stringbuf_shrinkwrap(str);

	return 0;
}

int aem_stringbuf_file_write(const struct aem_stringbuf *restrict str, FILE *fp)
{
	if (!str) {
		aem_logf_ctx(AEM_LOG_ERROR, "str == NULL!");
		return 1;
	}

	if (!fp) {
		aem_logf_ctx(AEM_LOG_ERROR, "fp == NULL!");
		return -1;
	}

	struct aem_stringslice slice = aem_stringslice_new_str(str);

	return aem_stringslice_file_write(slice, fp);
}


#ifdef __unix__
ssize_t aem_stringbuf_fd_read(struct aem_stringbuf *str, size_t n, int fd)
{
	aem_assert(str);
	aem_assert(fd >= 0);

	aem_stringbuf_reserve(str, n);

	ssize_t in = read(fd, aem_stringbuf_end(str), n);

	//aem_logf_ctx(AEM_LOG_DEBUG, "read %zd: n = %zd, maxn = %zd", in, str->n, str->maxn);

	if (in > 0) {
		str->n += in;
	}

	return in;
}

int aem_stringbuf_fd_read_all(struct aem_stringbuf *str, int fd)
{
	aem_assert(str);
	aem_assert(fd >= 0);

	ssize_t in;
	do {
		in = aem_stringbuf_fd_read(str, 4096, fd);
	} while (in > 0 || (in < 0 && errno == EINTR));

	if (in == 0) {
		return 1;
	} else if (in < 0) {
		return -1;
	}

	aem_stringbuf_shrinkwrap(str);

	return 0;
}

ssize_t aem_stringbuf_fd_write(const struct aem_stringbuf *str, int fd)
{
	aem_assert(str);
	aem_assert(fd >= 0);

	struct aem_stringslice slice = aem_stringslice_new_str(str);

	return aem_stringslice_fd_write(slice, fd);
}
#endif
