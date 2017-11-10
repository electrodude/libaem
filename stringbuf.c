#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// for vsnprintf
#include <stdio.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include "log.h"

#include "stringbuf.h"

#define AEM_STRINGBUF_DEBUG_ALLOC 0

struct aem_stringbuf *aem_stringbuf_new_raw(void)
{
	struct aem_stringbuf *str = malloc(sizeof(*str));

	if (str == NULL) return NULL;

	*str = AEM_STRINGBUF_EMPTY;

	return str;
}

struct aem_stringbuf *aem_stringbuf_init_prealloc(struct aem_stringbuf *str, size_t maxn)
{
	if (str == NULL) return NULL;

	str->n = 0;
	str->maxn = maxn;
	str->s = malloc(str->maxn);
	str->bad = 0;
	str->fixed = 0;
	str->storage = AEM_STRINGBUF_STORAGE_HEAP;

	return str;
}

struct aem_stringbuf *aem_stringbuf_init_array(struct aem_stringbuf *restrict str, size_t n, const char *restrict s)
{
	aem_stringbuf_init_prealloc(str, n);

	aem_stringbuf_putn(str, n, s);

	return str;
}

struct aem_stringbuf *aem_stringbuf_init_cstr(struct aem_stringbuf *restrict str, const char *restrict s)
{
	aem_stringbuf_init(str);

	aem_stringbuf_puts(str, s);

	return str;
}

struct aem_stringbuf *aem_stringbuf_init_slice(struct aem_stringbuf *restrict str, const char *start, const char *end)
{
	aem_stringbuf_init(str);

	aem_stringbuf_append_slice(str, start, end);

	return str;
}

struct aem_stringbuf *aem_stringbuf_init_str(struct aem_stringbuf *restrict str, const struct aem_stringbuf *restrict orig)
{
	aem_stringbuf_init_prealloc(str, orig->maxn);

	memcpy(str->s, orig->s, orig->n);

	str->n = orig->n;

	return str;
}

void aem_stringbuf_free(struct aem_stringbuf *str)
{
	if (str == NULL) return;
	if (str->bad) return;

	aem_stringbuf_dtor(str);

	free(str);
}

static inline void aem_stringbuf_storage_free(struct aem_stringbuf *str)
{
	switch (str->storage)
	{
		case AEM_STRINGBUF_STORAGE_HEAP:
			free(str->s);
			break;

		case AEM_STRINGBUF_STORAGE_UNOWNED:
			// not our responsibility; don't worry about it
			break;

		default:
			aem_logf_ctx(AEM_LOG_BUG, "%p: unknown storage type %d, leaking %p!\n", str, str->storage, str->s);
			break;
	}
}

void aem_stringbuf_dtor(struct aem_stringbuf *str)
{
	if (str == NULL) return;

	str->n = 0;

	if (str->s == NULL) return;

	aem_stringbuf_storage_free(str);

	str->s = NULL;
	str->maxn = 0;
}

char *aem_stringbuf_release(struct aem_stringbuf *str)
{
	if (str == NULL) return NULL;

	aem_stringbuf_shrink(str);

	char *s = str->s;

	free(str);

	return s;
}


void aem_stringbuf_grow(struct aem_stringbuf *str, size_t maxn_new)
{
	if (str->bad) return;

	// if it's already big enough, don't do anything
	if (str->maxn >= maxn_new) return;

	if (str->fixed)
	{
		str->bad = 1;
		return;
	}

#if 0
	size_t maxn_old = str->maxn;
#endif
	if (str->storage == AEM_STRINGBUF_STORAGE_HEAP)
	{
#if AEM_STRINGBUF_DEBUG_ALLOC
		aem_logf_ctx(AEM_LOG_DEBUG, "realloc: n %zd, maxn %zd -> %zd\n", str->n, str->maxn, maxn_new);
#endif
		char *s_new = realloc(str->s, maxn_new);

		if (s_new == NULL)
		{
			str->bad = 1;
			return;
		}

		str->s = s_new;
		str->maxn = maxn_new;
	}
	else
	{
#if AEM_STRINGBUF_DEBUG_ALLOC
		aem_logf_ctx(AEM_LOG_DEBUG, "to heap: n %zd, maxn %zd -> %zd\n", str->n, str->maxn, maxn_new);
#endif

		char *s_new = malloc(maxn_new);

		if (s_new == NULL)
		{
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
	if (maxn_old)
	{
		str->bad = 1;
	}
#endif
}


char *aem_stringbuf_shrink(struct aem_stringbuf *str)
{
	if (str == NULL) return NULL;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "%p\n", aem_stringbuf_get(str));
#endif

	if (!str->fixed && str->storage == AEM_STRINGBUF_STORAGE_HEAP)
	{
		size_t maxn_new = str->n + 1;
		char *s_new = realloc(str->s, maxn_new);

		if (s_new != NULL)
		{
			str->s = s_new;
			str->maxn = maxn_new;
		}
	}

	return aem_stringbuf_get(str);
}

void aem_stringbuf_printf(struct aem_stringbuf *restrict str, const char *restrict fmt, ...)
{
	if (str == NULL) return;
	if (str->bad) return;

	if (fmt == NULL) return;

	va_list argp;

	va_start(argp, fmt);
	size_t len = vsnprintf(NULL, 0, fmt, argp) + 2;
	va_end(argp);

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "%p: %s\n", str, fmt);
#endif

	aem_stringbuf_reserve(str, len);

	va_start(argp, fmt);
	str->n += vsnprintf(aem_stringbuf_end(str), aem_stringbuf_available(str), fmt, argp);
	va_end(argp);
}

/*
char *aem_stringbuf_append_manual(struct aem_stringbuf *str, size_t len)
{
	if (str == NULL) return;

	aem_stringbuf_reserve(str, len);
	if (str->bad) return;

	char *p = aem_stringbuf_end(str);

	str->n += len;

	return p;
}
*/

void aem_stringbuf_append(struct aem_stringbuf *restrict str, const struct aem_stringbuf *restrict str2)
{
	if (str == NULL) return;

	if (str2 == NULL) return;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "\"%s\" ..= \"%s\"\n", aem_stringbuf_get(str), aem_stringbuf_get(str2));
#endif


	aem_stringbuf_reserve(str, str2->n);
	if (str->bad) return;

	memcpy(aem_stringbuf_end(str), str2->s, str2->n);

	str->n += str2->n;
}

void aem_stringbuf_putq(struct aem_stringbuf *str, char c)
{
	if (str == NULL) return;
	if (str->bad) return;

	switch (c)
	{
		case '\n':
			aem_stringbuf_puts(str, "\\n");
			break;
		case '\r':
			aem_stringbuf_puts(str, "\\r");
			break;
		case '\t':
			aem_stringbuf_puts(str, "\\t");
			break;
		case '\0':
			aem_stringbuf_puts(str, "\\0");
			break;
		case '\\':
			aem_stringbuf_puts(str, "\\\\");
			break;
		case '"':
			aem_stringbuf_puts(str, "\\\"");
			break;
		default:
			if (c >= 32 && c < 127)
			{
				aem_stringbuf_putc(str, c);
			}
			else
			{
				aem_stringbuf_puts(str, "\\x");
				aem_stringbuf_puthex(str, c);
			}
			break;
	}
}

void aem_stringbuf_append_quote(struct aem_stringbuf *restrict str, const struct aem_stringbuf *restrict str2)
{
	if (str == NULL) return;
	if (str->bad) return;

	if (str2 == NULL) return;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "\"%s\" ..= quote(\"%s\")\n", aem_stringbuf_get(str), aem_stringbuf_get(str2));
#endif

	for (size_t i = 0; i < str2->n; i++)
	{
		aem_stringbuf_putq(str, str2->s[i]);
	}
}

void aem_stringbuf_append_stringslice_quote(struct aem_stringbuf *restrict str, const struct aem_stringslice *restrict slice)
{
	if (str == NULL) return;
	if (str->bad) return;

	if (slice == NULL) return;

#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "\"%s\" ..= <slice>\n", aem_stringbuf_get(str));
#endif

	for (const char *p = slice->start; p != slice->end; p++)
	{
		aem_stringbuf_putq(str, *p);
	}
}

void aem_stringbuf_pad(struct aem_stringbuf *str, size_t len, char c)
{
	if (str == NULL) return;

	while (str->n < len && !str->bad)
	{
		aem_stringbuf_putc(str, c);
	}
}

int aem_stringbuf_index(struct aem_stringbuf *str, size_t i)
{
	if (str == NULL) return -1;

	if (i >= str->n)
	{
		return -1;
	}
#if AEM_STRINGBUF_DEBUG
	aem_logf_ctx(AEM_LOG_DEBUG, "[%zd] = %c\n", i, str->s[i]);
#endif

	return str->s[i];
}

void aem_stringbuf_assign(struct aem_stringbuf *str, size_t i, char c)
{
	if (str == NULL) return;

	if (i + 1 > str->n)
	{
		str->n = i + 1;

		aem_stringbuf_reserve(str, str->n + i);
	}

	str->s[i] = c;
}


size_t aem_stringbuf_fread(struct aem_stringbuf *str, FILE *fp)
{
	if (str == NULL) return 1;

	size_t n_read = fread(aem_stringbuf_end(str), 1, aem_stringbuf_available(str), fp);

	//aem_logf_ctx(AEM_LOG_DEBUG, "read %zd: n = %zd, maxn = %zd\n", n_read, str->n, str->maxn);

	str->n += n_read;

	return n_read;
}

int aem_stringbuf_file_read(struct aem_stringbuf *str, FILE *fp)
{
	if (str == NULL) return 1;

	do
	{
		aem_stringbuf_reserve(str, str->n > 4096 ? str->n : 4096);

		size_t n_read = aem_stringbuf_fread(str, fp);

		if (n_read == 0)
		{
			if (feof(fp))
			{
				return 1;
			}
			else if (ferror(fp))
			{
				return -1;
			}
		}
	} while (!feof(fp));

	aem_stringbuf_shrink(str);

	return 0;
}

int aem_stringbuf_file_write(const struct aem_stringbuf *restrict str, FILE *fp)
{
	if (str == NULL) return 1;

	const char *p  =  str->s;
	const char *pe = &str->s[str->n];

	while (p < pe)
	{
		size_t n_written = fwrite(p, 1, pe - p, fp);

		p += n_written;

		if (n_written == 0)
		{
			if (ferror(fp))
			{
				return 1;
			}
		}
	}

	return 0;
}


#ifdef __unix__
int aem_stringbuf_fd_read(struct aem_stringbuf *str, int fd)
{
	if (str == NULL) return 1;

	ssize_t n_read;
	do
	{
		aem_stringbuf_reserve(str, str->n > 4096 ? str->n : 4096);

		n_read = read(fd, aem_stringbuf_end(str), aem_stringbuf_available(str));

		//aem_logf_ctx(AEM_LOG_DEBUG, "read %zd: n = %zd, maxn = %zd\n", n_read, str->n, str->maxn);

		if (n_read == 0)
		{
			return 1;
		}
		else if (n_read < 0)
		{
			return -1;
		}

		str->n += n_read;

	} while (n_read > 0);

	aem_stringbuf_shrink(str);

	return 0;
}

int aem_stringbuf_fd_read_n(struct aem_stringbuf *str, size_t n, int fd)
{
	if (str == NULL) return 1;

	aem_stringbuf_reserve(str, n);

	ssize_t n_read;
	do
	{
		n_read = read(fd, aem_stringbuf_end(str), n);

		//aem_logf_ctx(AEM_LOG_DEBUG, "read %zd: n = %zd, maxn = %zd\n", n_read, str->n, str->maxn);

		if (n_read == 0)
		{
			return 1;
		}
		else if (n_read < 0)
		{
			return -1;
		}

		str->n += n_read;

		n -= n_read;

	} while (n_read > 0 && n > 0);

	aem_stringbuf_shrink(str);

	return 0;
}

int aem_stringbuf_fd_write(const struct aem_stringbuf *restrict str, int fd)
{
	if (str == NULL) return 1;

	const char *p  =  str->s;
	const char *pe = &str->s[str->n];

	while (p < pe)
	{
		ssize_t n_written = write(fd, p, pe - p);

		p += n_written;

		if (n_written < 0)
		{
			return 1;
		}
	}

	return 0;
}
#endif
