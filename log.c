#define _POSIX_C_SOURCE 1
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#define AEM_INTERNAL
#include <aem/stringbuf.h>

#include "log.h"


// log file

static FILE *aem_log_fp = NULL;
static int aem_log_autoclose_curr = 0;
int aem_log_color = 0;

FILE *aem_log_fset(FILE *fp_new, int autoclose_new)
{
	FILE *fp_old = aem_log_fp;
	aem_log_fp = fp_new;

	if (aem_log_autoclose_curr && fp_old) {
		fclose(fp_old);
		fp_old = NULL;
	}

	aem_log_autoclose_curr = autoclose_new;

	int fd = fileno(aem_log_fp);
	if (fd >= 0 && isatty(fd)) {
		aem_log_color = 1;
	} else {
		aem_log_color = 0;
	}

	aem_logf_ctx(AEM_LOG_DEBUG, "Switched to new log source: %s\n", aem_log_color ? "color" : "no color");

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
struct aem_log_module aem_log_module_default_internal = {.loglevel = AEM_LOG_NOTICE};

const char *aem_log_level_color(enum aem_log_level loglevel)
{
	switch (loglevel) {
		case AEM_LOG_FATAL   : return "\033[5;101m";
		case AEM_LOG_SECURITY: return "\033[101;30;5m";
		case AEM_LOG_BUG     : return "\033[101;30m";
		case AEM_LOG_NYI     : return "\033[101;30m";
		case AEM_LOG_ERROR   : return "\033[31m";
		case AEM_LOG_WARN    : return "\033[33m";
		case AEM_LOG_NOTICE  : return "\033[94;1m";
		case AEM_LOG_INFO    : return "\033[0m";
		case AEM_LOG_DEBUG   : return "\033[37;2m";
		case AEM_LOG_DEBUG2  : return "\033[90m";
		case AEM_LOG_DEBUG3  : return "\033[90;2m";
		default              : return "\033[101;30m";
	}
}

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

enum aem_log_level aem_log_level_parse(struct aem_stringslice word)
{
	// Empty string -> debug
	if (!aem_stringslice_ok(word))
		return AEM_LOG_DEBUG;

	int c = aem_stringslice_get(&word);

	enum aem_log_level level;

	switch (0 <= c && c < 256 ? tolower(c) : -1) {
		case 'f': level = AEM_LOG_FATAL   ; break;
		case 's': level = AEM_LOG_SECURITY; break;
		case 'b': level = AEM_LOG_BUG     ; break;
		case 'u': level = AEM_LOG_NYI     ; break;
		case 'e': level = AEM_LOG_ERROR   ; break;
		case 'w': level = AEM_LOG_WARN    ; break;
		case 'n': level = AEM_LOG_NOTICE  ; break;
		case 'i': level = AEM_LOG_INFO    ; break;
		case 'd': level = AEM_LOG_DEBUG   ; break;
		case '2': level = AEM_LOG_DEBUG2  ; break; // These two have no
		case '3': level = AEM_LOG_DEBUG3  ; break; // valid long forms
		default :
			aem_logf_ctx(AEM_LOG_ERROR, "Failed to parse log level; default to debug\n");
			return AEM_LOG_DEBUG;
	}

	// If only one letter was given, return now
	if (!aem_stringslice_ok(word))
		return level;

	const char *expect = aem_log_level_describe(level);

	// Complain if the rest of the word isn't correct
	if (!aem_stringslice_eq_case(word, expect+1))
		aem_logf_ctx(AEM_LOG_WARN, "Misspelled log level; assuming you meant \"%s\"\n", expect);

	return level;
}
void aem_log_level_parse_set(const char *p)
{
	struct aem_stringslice s = aem_stringslice_new_cstr(p);
	aem_stringslice_match_ws(&s);
	aem_log_module_default.loglevel = aem_log_level_parse(aem_stringslice_match_alnum(&s));
	aem_logmf_ctx(&aem_log_module_default, aem_log_module_default.loglevel, "Set default log level to %s\n", aem_log_level_describe(aem_log_module_default.loglevel));
	aem_stringslice_match_ws(&s);

	if (aem_stringslice_match(&s, ",")) {
		aem_stringslice_match_ws(&s);
		aem_log_module_default_internal.loglevel = aem_log_level_parse(aem_stringslice_match_alnum(&s));
		aem_logmf_ctx(&aem_log_module_default_internal, aem_log_module_default_internal.loglevel, "Set internal log level to %s\n", aem_log_level_describe(aem_log_module_default_internal.loglevel));
		aem_stringslice_match_ws(&s);
	}

	if (aem_stringslice_ok(s))
		aem_logf_ctx(AEM_LOG_WARN, "Garbage after log level!\n");
}


/// Logging

__thread struct aem_stringbuf aem_log_buf = {0};

void aem_log_header_impl(struct aem_stringbuf *str, enum aem_log_level loglevel, const char *file, int line, const char *func)
{
	if (aem_log_color)
		aem_stringbuf_puts(str, aem_log_level_color(loglevel));
	aem_stringbuf_putc(str, aem_log_level_letter(loglevel));
	aem_stringbuf_putc(str, ' ');
	aem_stringbuf_printf(str, "%s:%d(%s)", file, line, func);
	//aem_stringbuf_puts(str, ": ");
	//aem_stringbuf_putc(str, aem_log_level_describe(loglevel));
	aem_stringbuf_puts(str, ":");
	if (aem_log_color)
		aem_stringbuf_puts(str, "\033[0m"); // Reset text style
	aem_stringbuf_puts(str, " ");
}

int aem_logmf_ctx_impl(struct aem_log_module *mod, enum aem_log_level loglevel, const char *file, int line, const char *func, const char *fmt, ...)
{
	if (loglevel > mod->loglevel)
		return 0;

	aem_stringbuf_reset(&aem_log_buf);
	aem_log_header_impl(&aem_log_buf, loglevel, file, line, func);

	va_list ap;
	va_start(ap, fmt);
	aem_stringbuf_vprintf(&aem_log_buf, fmt, ap);
	va_end(ap);
	if (aem_log_color)
		aem_stringbuf_puts(&aem_log_buf, "\033[0m");

	// TODO: If message exceeds line width, wrap and insert continuation headers.

	int rc = aem_log_str(&aem_log_buf);

#ifdef AEM_BREAK_ON_BUG
	if (loglevel <= AEM_LOG_BUG)
		aem_break();
#endif

	return rc;
}

int aem_log_str(struct aem_stringbuf *str)
{
	return aem_stringslice_file_write(aem_stringslice_new_str(str), aem_log_fp);
}
