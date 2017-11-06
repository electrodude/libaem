#include <stdarg.h>
#include <stdio.h>

#include "debug.h"

FILE *aem_debug_fp = NULL;

int aem_dprintf(const char *fmt, ...)
{
	int count = 0;

	if (aem_debug_fp != NULL)
	{
		va_list ap;
		va_start(ap, fmt);

		count = vfprintf(aem_debug_fp, fmt, ap);

		va_end(ap);
	}

	return count;
}
