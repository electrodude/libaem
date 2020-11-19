#include <ctype.h>
#include <errno.h>

#include "log.h"


// log file

FILE *aem_log_fp = NULL;
int aem_log_autoclose_curr = 0;

FILE *aem_log_fset(FILE *fp_new, int autoclose_new)
{

	FILE *fp_old = aem_log_fp;
	aem_log_fp = fp_new;

	if (aem_log_autoclose_curr && fp_old) {
		fclose(fp_old);
		fp_old = NULL;
	}

	aem_log_autoclose_curr = autoclose_new;

	return fp_old;
}

FILE *aem_log_fopen(const char *path_new)
{
	if (!path_new)
		return NULL;
	FILE *fp_new = fopen(path_new, "a");
	if (!fp_new) {
		return NULL;
	}

	FILE *fp_old = aem_log_fset(fp_new, 1);

	if (!fp_old) {
		errno = 0; // TODO: find better way to signal success but no old logfile
	}

	return fp_old;
}

FILE *aem_log_fget(void)
{
	return aem_log_fp;
}


// log level

struct aem_log_module aem_log_module_default = {.loglevel = AEM_LOG_NOTICE};

const char *aem_log_level_describe(enum aem_log_level loglevel)
{
	switch (loglevel) {
		case AEM_LOG_FATAL   : return "fatal";
		case AEM_LOG_SECURITY: return "security";
		case AEM_LOG_BUG     : return "bug";
		case AEM_LOG_NYI     : return "unimplemented";
		case AEM_LOG_ERROR   : return "error";
		case AEM_LOG_WARN    : return "warn";
		case AEM_LOG_NOTICE  : return "notice";
		case AEM_LOG_INFO    : return "info";
		case AEM_LOG_DEBUG   : return "debug";
		case AEM_LOG_DEBUG2  : return "debug2";
		case AEM_LOG_DEBUG3  : return "debug3";
		default              : return "(unknown)";
	}
}

char aem_log_level_letter(enum aem_log_level loglevel)
{
	switch (loglevel) {
		case AEM_LOG_FATAL   : return 'F';
		case AEM_LOG_SECURITY: return 'S';
		case AEM_LOG_BUG     : return 'B';
		case AEM_LOG_NYI     : return 'U';
		case AEM_LOG_ERROR   : return 'E';
		case AEM_LOG_WARN    : return 'W';
		case AEM_LOG_NOTICE  : return 'n';
		case AEM_LOG_INFO    : return 'i';
		case AEM_LOG_DEBUG   : return 'd';
		case AEM_LOG_DEBUG2  : return '2';
		case AEM_LOG_DEBUG3  : return '3';
		default              : return '?';
	}
}

enum aem_log_level aem_log_level_check_prefix(enum aem_log_level level, const char *in)
{
	const char *expect = aem_log_level_describe(level);

	const char *p = in;
	const char *p2 = expect;

	// this should be a function that lives in stringslice or something
	while (*p && *p2) {
		if (tolower(*p) != tolower(*p2)) {
			aem_logf_ctx(AEM_LOG_WARN, "unknown log level %s, autocorrect to %s\n", in, expect);
			break;
		}
		p++; p2++;
	}

	return level;
}

enum aem_log_level aem_log_level_parse(const char *p)
{
	if (!p)
		return AEM_LOG_DEBUG; // default to debug

	switch (tolower(*p)) {
		case 'f': goto f_atal;
		case 's': goto s_ecurity;
		case 'b': goto b_ug;
		case 'u': goto u_nimplemented;
		case 'e': goto e_rror;
		case 'w': goto w_arn;
		case 'n': goto n_otice;
		case 'i': goto i_nfo;
		case 'd': goto d_ebug;
		case '2': goto d2;
		case '3': goto d3;
		default : break;
	}

	aem_logf_ctx(AEM_LOG_ERROR, "unknown log level %s, default to debug\n", p);

	return AEM_LOG_DEBUG;

f_atal:
	return aem_log_level_check_prefix(AEM_LOG_FATAL, p);
s_ecurity:
	return aem_log_level_check_prefix(AEM_LOG_SECURITY, p);
b_ug:
	return aem_log_level_check_prefix(AEM_LOG_BUG, p);
u_nimplemented:
	return aem_log_level_check_prefix(AEM_LOG_NYI, p);
e_rror:
	return aem_log_level_check_prefix(AEM_LOG_ERROR, p);
w_arn:
	return aem_log_level_check_prefix(AEM_LOG_WARN, p);
n_otice:
	return aem_log_level_check_prefix(AEM_LOG_NOTICE, p);
i_nfo:
	return aem_log_level_check_prefix(AEM_LOG_INFO, p);
d_ebug:
	return aem_log_level_check_prefix(AEM_LOG_DEBUG, p);
d2:
	return AEM_LOG_DEBUG2;
d3:
	return AEM_LOG_DEBUG3;
}


// logging

int aem_logmf(struct aem_log_module *module, enum aem_log_level loglevel, const char *fmt, ...)
{
	if (loglevel > module->loglevel)
		return 0;

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
	if (!aem_log_fp)
		return 0;

	int count = vfprintf(aem_log_fp, fmt, ap);

	return count;
}
