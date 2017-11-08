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
