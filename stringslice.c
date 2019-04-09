#include <stdlib.h>
#include <ctype.h>

#ifdef __unix__
#include <errno.h>
#endif

#include "stringslice.h"

int aem_stringslice_file_write(struct aem_stringslice *slice, FILE *fp)
{
	if (!slice) return 1;

	while (aem_stringslice_ok(slice))
	{
		size_t n_written = fwrite(slice->start, 1, aem_stringslice_len(slice), fp);

		if (!n_written)
		{
			if (ferror(fp))
			{
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
	if (!slice) return -1;

	ssize_t total = 0;

	while (aem_stringslice_ok(slice))
	{
again:;
		ssize_t out = write(fd, slice->start, aem_stringslice_len(slice));

		if (out < 0)
		{
			if (errno == EINTR)
			{
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
	if (!slice) return 0;

	int matched = 0;

	while (aem_stringslice_ok(slice) && isspace(*slice->start))
	{
		matched = 1;
		slice->start++;
	}

	return matched;
}

struct aem_stringslice aem_stringslice_match_word(struct aem_stringslice *slice)
{
	if (!slice) return AEM_STRINGSLICE_EMPTY;

	struct aem_stringslice line;
	line.start = slice->start;

	while (aem_stringslice_ok(slice) && !isspace(*slice->start)) slice->start++;

	line.end = slice->start;

	return line;
}

struct aem_stringslice aem_stringslice_match_line(struct aem_stringslice *slice)
{
	if (!slice) return AEM_STRINGSLICE_EMPTY;

	struct aem_stringslice line;
	line.start = slice->start;

	while (aem_stringslice_ok(slice) && !(*slice->start == '\n' || *slice->start == '\n')) slice->start++;

	line.end = slice->start;

	return line;
}

int aem_stringslice_match(struct aem_stringslice *slice, const char *s)
{
	if (!slice) return 0;
	if (!s) return 1;

	const char *p2 = slice->start;

	while (*s)
	{
		if (p2 == slice->end)
		{
			return 0;
		}

		if (*p2++ != *s++)
		{
			return 0;
		}
	}

	slice->start = p2;

	return 1;
}

int aem_stringslice_eq(struct aem_stringslice slice, const char *s)
{
	if (!s) return 1;

	while (aem_stringslice_ok(&slice) && *s != '\0') // while neither are finished
	{
		if (*slice.start++ != *s++) return 0; // unequal characters => no match
	}

	return !aem_stringslice_ok(&slice) && *s == '\0'; // ensure both are finished
}

static inline int hex2nib(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 0xA;
	}
	else if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 0xA;
	}
	else
	{
		return -1;
	}
}

int aem_stringslice_match_hexbyte(struct aem_stringslice *slice)
{
	if (!slice) return -1;

	if (slice->start == slice->end || slice->start + 1 == slice->end) return -1;

	const char *p = slice->start;

	int c1 = hex2nib(*p++);
	if (c1 < 0) return -1;

	int c0 = hex2nib(*p++);
	if (c0 < 0) return -1;

	slice->start = p;

	return c1 << 4 | c0;
}
