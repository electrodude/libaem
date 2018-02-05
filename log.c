#include <ctype.h>

#include "log.h"

FILE *aem_log_fp = NULL;

enum aem_log_level aem_log_level_curr = AEM_LOG_NOTICE;

const char *aem_log_level_describe(enum aem_log_level loglevel)
{
	switch (loglevel)
	{
		case AEM_LOG_FATAL   : return "FATAL";
		case AEM_LOG_BUG     : return "BUG";
		case AEM_LOG_SECURITY: return "SECURITY";
		case AEM_LOG_ERROR   : return "ERROR";
		case AEM_LOG_WARN    : return "WARN";
		case AEM_LOG_NOTICE  : return "NOTICE";
		case AEM_LOG_INFO    : return "INFO";
		case AEM_LOG_DEBUG   : return "DEBUG";
		default              : return "(unknown)";
	}
}

char aem_log_level_letter(enum aem_log_level loglevel)
{
	switch (loglevel)
	{
		case AEM_LOG_FATAL   : return 'F';
		case AEM_LOG_BUG     : return 'B';
		case AEM_LOG_SECURITY: return 'S';
		case AEM_LOG_ERROR   : return 'E';
		case AEM_LOG_WARN    : return 'W';
		case AEM_LOG_NOTICE  : return 'n';
		case AEM_LOG_INFO    : return 'i';
		case AEM_LOG_DEBUG   : return 'd';
		default              : return '?';
	}
}

enum aem_log_level aem_log_level_parse(const char *p)
{
	const char *ps = p;

	switch (tolower(*p))
	{
		case 'f': goto f_atal;
		case 'b': goto b_ug;
		case 's': goto s_ecurity;
		case 'e': goto e_rror;
		case 'w': goto w_arn;
		case 'n': goto n_otice;
		case 'i': goto i_nfo;
		case 'd': goto d_ebug;
		default : break;
	}

	aem_logf(AEM_LOG_ERROR, "unknown log level %s, default to debug\n", ps);

	return AEM_LOG_DEBUG;

f_atal:
	// TODO: for each label, ensure p is prefix of remaining part, else warn
	return AEM_LOG_FATAL;
b_ug:
	return AEM_LOG_BUG;
s_ecurity:
	return AEM_LOG_SECURITY;
e_rror:
	return AEM_LOG_ERROR;
w_arn:
	return AEM_LOG_WARN;
n_otice:
	return AEM_LOG_NOTICE;
i_nfo:
	return AEM_LOG_INFO;
d_ebug:
	return AEM_LOG_DEBUG;
}

enum aem_log_level aem_log_level_parse_set(const char *p)
{
	enum aem_log_level loglevel = aem_log_level_parse(p);

	aem_log_level_curr = loglevel;

	return loglevel;
}

int aem_logf(enum aem_log_level loglevel, const char *fmt, ...)
{
	if (loglevel > aem_log_level_curr) return 0;

	va_list ap;
	va_start(ap, fmt);

	int count = aem_vdprintf(fmt, ap);

	va_end(ap);

	return count;
}

int aem_dprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int count = aem_vdprintf(fmt, ap);

	va_end(ap);

	return count;
}

int aem_vdprintf(const char *fmt, va_list ap)
{
	if (aem_log_fp == NULL) return 0;

	int count = vfprintf(aem_log_fp, fmt, ap);

	return count;
}
