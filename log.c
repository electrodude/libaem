#define _POSIX_C_SOURCE 1
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#define AEM_INTERNAL
#include <aem/ansi-term.h>
#include <aem/memory.h>
#include <aem/stringbuf.h>
#include <aem/translate.h>

#include "log.h"

static int aem_log_autoclose_curr = 0;
int aem_log_color = 0;

/// Log destinations

// Null destination: goes nowhere
static void aem_log_dest_null_log(struct aem_log_dest *dst, struct aem_log_module *mod, struct aem_stringslice msg)
{
	(void)dst;
	(void)mod;
	(void)msg;
}
struct aem_log_dest aem_log_dest_null = {
	.log = aem_log_dest_null_log,
};

// Log to FILE
void aem_log_dest_fp_log(struct aem_log_dest *dst, struct aem_log_module *mod, struct aem_stringslice msg)
{
	aem_assert(dst);
	aem_assert(mod);
	struct aem_log_dest_fp *dst_fp = aem_container_of(dst, struct aem_log_dest_fp, dst);

	aem_stringslice_file_write(msg, dst_fp->fp);
}
struct aem_log_dest_fp aem_log_fp = {
	.dst = {.log = aem_log_dest_fp_log},
	.fp = NULL,
};

FILE *aem_log_fset(FILE *fp_new, int autoclose_new)
{
	FILE *fp_old = aem_log_fp.fp;
	aem_log_fp.fp = fp_new;

	if (aem_log_autoclose_curr && fp_old) {
		fclose(fp_old);
		fp_old = NULL;
	}

	aem_log_autoclose_curr = autoclose_new;

	setvbuf(aem_log_fp.fp, NULL, _IOLBF, 0);

	int fd = fileno(aem_log_fp.fp);
	if (fd >= 0 && isatty(fd)) {
		aem_log_color = 1;
	} else {
		aem_log_color = 0;
	}

	aem_logf_ctx(AEM_LOG_DEBUG, "Switched to new log source: %s", aem_log_color ? "color" : "no color");

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
	return aem_log_fp.fp;
}


/// log level

struct aem_log_module aem_log_module_default = {.name = "default", .loglevel = AEM_LOG_NOTICE};
struct aem_log_module aem_log_module_default_internal = {.name = "aem", .loglevel = AEM_LOG_NOTICE};

const char *aem_log_level_color(enum aem_log_level loglevel)
{
	switch (loglevel) {
		case AEM_LOG_FATAL   : return AEM_SGR("101"     );
		case AEM_LOG_SECURITY: return AEM_SGR("101;30"  );
		case AEM_LOG_BUG     : return AEM_SGR("103;30"  );
		case AEM_LOG_NYI     : return AEM_SGR("101;30"  );
		case AEM_LOG_ERROR   : return AEM_SGR("31;1"    );
		case AEM_LOG_WARN    : return AEM_SGR("33;1"    );
		case AEM_LOG_GOOD    : return AEM_SGR("92;1"    );
		case AEM_LOG_NOTICE  : return AEM_SGR("94;1"    );
		case AEM_LOG_INFO    : return AEM_SGR("0"       );
		case AEM_LOG_DEBUG   : return AEM_SGR("37;2"    );
		case AEM_LOG_DEBUG2  : return AEM_SGR("90"      );
		case AEM_LOG_DEBUG3  : return AEM_SGR("90;2"    );
		default              : return AEM_SGR("101;30"  );
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
		case AEM_LOG_GOOD    : return 'g';
		case AEM_LOG_NOTICE  : return 'n';
		case AEM_LOG_INFO    : return 'i';
		case AEM_LOG_DEBUG   : return 'd';
		case AEM_LOG_DEBUG2  : return '2';
		case AEM_LOG_DEBUG3  : return '3';
		default              : return '?';
	}
}

enum aem_log_level aem_log_level_parse_letter(char c)
{
	switch (tolower(c)) {
		case 'f': return AEM_LOG_FATAL   ;
		case 'S': return AEM_LOG_SECURITY;
		case 'b': return AEM_LOG_BUG     ;
		case 'u': return AEM_LOG_NYI     ;
		case 'e': return AEM_LOG_ERROR   ;
		case 'w': return AEM_LOG_WARN    ;
		case 'g': return AEM_LOG_GOOD    ;
		case 'n': return AEM_LOG_NOTICE  ;
		case 'i': return AEM_LOG_INFO    ;
		case 'd': return AEM_LOG_DEBUG   ;
		case '2': return AEM_LOG_DEBUG2  ; // These two have no
		case '3': return AEM_LOG_DEBUG3  ; // valid long forms
	}
	return AEM_LOG_INVALID;
}
enum aem_log_level aem_log_level_parse(struct aem_stringslice word)
{
	// Empty string -> debug
	if (!aem_stringslice_ok(word))
		return AEM_LOG_DEBUG;

	enum aem_log_level level = aem_stringslice_ok(word) ? aem_log_level_parse_letter(*word.start) : AEM_LOG_INVALID;

	if (level == AEM_LOG_INVALID) {
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to parse log level; default to debug");
		level = AEM_LOG_DEBUG;
	}

	// If only one letter was given, return now
	if (!aem_stringslice_ok(word))
		return level;

	const char *expect = aem_log_level_describe(level);

	// Complain if the rest of the word isn't correct
	if (!aem_stringslice_eq_case(word, expect)) {
		AEM_LOG_MULTI(out, AEM_LOG_WARN) {
			aem_stringbuf_puts(out, "Misspelled log level \"");
			aem_string_escape(out, word);
			aem_stringbuf_printf(out, "\"; assuming you meant \"%s\"", expect);
		}
	}

	return level;
}
void aem_log_level_parse_set(const char *p)
{
	struct aem_stringslice s = aem_stringslice_new_cstr(p);
	aem_stringslice_match_ws(&s);
	aem_log_module_default.loglevel = aem_log_level_parse(aem_stringslice_match_alnum(&s));
	aem_logmf_ctx(&aem_log_module_default, aem_log_module_default.loglevel, "Set default log level to %s", aem_log_level_describe(aem_log_module_default.loglevel));
	aem_stringslice_match_ws(&s);

	if (aem_stringslice_match(&s, ",")) {
		aem_stringslice_match_ws(&s);
		aem_log_module_default_internal.loglevel = aem_log_level_parse(aem_stringslice_match_alnum(&s));
		aem_logmf_ctx(&aem_log_module_default_internal, aem_log_module_default_internal.loglevel, "Set internal log level to %s", aem_log_level_describe(aem_log_module_default_internal.loglevel));
		aem_stringslice_match_ws(&s);
	}

	if (aem_stringslice_ok(s))
		aem_logf_ctx(AEM_LOG_WARN, "Garbage after log level!");
}


/// Logging

__thread struct aem_stringbuf aem_log_buf = {0};

int aem_log_module = 1;

unsigned int mod_name_width_max = 1;

struct aem_stringbuf *aem_log_header_mod_impl(struct aem_stringbuf *str, struct aem_log_module *mod, enum aem_log_level loglevel, const char *file, int line, const char *func)
{
	aem_assert(mod);
	if (loglevel > mod->loglevel)
		return NULL;

	aem_stringbuf_reset(str);

	if (aem_log_color)
		aem_stringbuf_puts(str, aem_log_level_color(loglevel));
	aem_stringbuf_putc(str, aem_log_level_letter(loglevel));
	aem_stringbuf_putc(str, ' ');

	if (aem_log_module) {
		aem_stringbuf_puts(str, "[");
		size_t name_start = str->n;
		if (mod->name)
			aem_stringbuf_puts(str, mod->name);

		// Pad to longest ever seen
		// TODO: Precompute mod_name_width_max from a log_module registry?
		if (str->n - name_start > mod_name_width_max)
			mod_name_width_max = str->n - name_start;
		aem_ansi_pad(str, name_start, mod_name_width_max);
		aem_stringbuf_puts(str, "] ");
	}

	aem_stringbuf_printf(str, "%s:%d(%s)", file, line, func);
	//aem_stringbuf_puts(str, ": ");
	//aem_stringbuf_putc(str, aem_log_level_describe(loglevel));
	aem_stringbuf_putc(str, ':');
	if (aem_log_color)
		aem_stringbuf_puts(str, AEM_SGR("0")); // Reset text style
	aem_stringbuf_putc(str, ' ');

	return str;
}

static void aem_log_submit(struct aem_log_module *mod, struct aem_stringbuf *str)
{
	if (!str)
		return;
	aem_assert(mod);
	struct aem_log_dest *dst = mod->dst;
	if (!dst)
		dst = &aem_log_fp.dst;
	aem_assert(dst->log);
	dst->log(dst, mod, aem_stringslice_new_str(str));
}

int aem_logmf_ctx_impl(struct aem_log_module *mod, enum aem_log_level loglevel, const char *file, int line, const char *func, const char *fmt, ...)
{
	aem_assert(fmt);

	struct aem_stringbuf *str = aem_log_header_mod_impl(&aem_log_buf, mod, loglevel, file, line, func);
	if (!str)
		return 0;

	va_list ap;
	va_start(ap, fmt);
	aem_stringbuf_vprintf(str, fmt, ap);
	va_end(ap);

	// If present, remove the final newline the user was required to supply in previous versions.
	if (str->n && str->s[str->n-1] == '\n')
		str->n--;

	// Reset color
	if (aem_log_color)
		aem_stringbuf_puts(str, AEM_SGR("0"));
	// Add newline
	aem_stringbuf_puts(str, "\n");

	if (!aem_log_color)
		aem_ansi_strip_inplace(str);

	// TODO: If message exceeds line width, wrap and insert continuation headers.

	aem_log_submit(mod, str);

	// Warn if format string ends with newline
	if (fmt[0] && fmt[strlen(fmt)-1] == '\n')
		aem_logmf_ctx_impl(&aem_log_module_default_internal, AEM_LOG_BUG, file, line, func, "Deprecated: format string for the previous message ends with \\n");

#ifdef AEM_BREAK_ON_BUG
	if (loglevel <= AEM_LOG_BUG)
		aem_break();
#endif

	return 0;
}

void aem_log_multi_impl(struct aem_log_module *mod, struct aem_stringbuf *str)
{
	aem_assert(str);
	aem_stringbuf_putc(str, '\n');
	if (!aem_log_color)
		aem_ansi_strip_inplace(str);
	aem_log_submit(mod, str);
}
