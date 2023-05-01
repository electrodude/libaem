#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/translate.h>

#include "pathutil.h"

struct aem_stringslice aem_stringslice_match_pathcomponent(struct aem_stringslice *in, int *trailing_slash_p)
{
	aem_assert(in);

	struct aem_stringslice out = *in;

	while (aem_stringslice_ok(*in) && *in->start != '/') {
		in->start++;
	}
	out.end = in->start;

	// Eat all trailing slashes
	int trailing_slash = 0;
	while (aem_stringslice_match(in, "/"))
		trailing_slash = 1;

	if (trailing_slash_p)
		*trailing_slash_p = trailing_slash;

	return out;
}
int aem_sandbox_path(struct aem_stringbuf *out, struct aem_stringslice base, struct aem_stringslice subpath, const char *ext)
{
	aem_assert(out);
	if (aem_stringslice_ok(base))
		aem_stringbuf_putss(out, base);
	else
		aem_stringbuf_puts(out, ".");

	// Ensure a slash separates the base from the subpath
	if (aem_stringbuf_index(out, out->n-1) != '/')
		aem_stringbuf_putc(out, '/');

	// TODO: What about going into paths that don't exist or that are files, not directories, and then ..'ing out of them again?

	size_t base_i = out->n;

	int rc = 0;

	int trailing_slash;
	while (aem_stringslice_ok(subpath)) {
		struct aem_stringslice component = aem_stringslice_match_pathcomponent(&subpath, &trailing_slash);

		AEM_LOG_MULTI(out, AEM_LOG_DEBUG3) {
			aem_stringbuf_puts(out, "Component: ");
			aem_string_escape(out, component);
		}

		if (!aem_stringslice_ok(component)) {
			// Leading /
		} else if (aem_stringslice_eq(component, ".")) {
			// Ignore "."
			continue;
		} else if (aem_stringslice_eq(component, "..")) {
			// Back up until the previous slash
			aem_logf_ctx(AEM_LOG_DEBUG3, "Before ..: \"%s\"", aem_stringbuf_get(out));
			if (out->n > base_i) {
				aem_assert(aem_stringbuf_index(out, out->n-1) == '/');
				out->n--;
			}
			size_t n = out->n;
			while (out->n > base_i && aem_stringbuf_index(out, out->n-1) != '/')
				out->n--;
			aem_logf_ctx(AEM_LOG_DEBUG3, "After  ..: \"%s\"", aem_stringbuf_get(out));
			if (n == out->n)
				rc |= 1;
		} else {
			aem_stringbuf_putss(out, component);

			if (trailing_slash)
				aem_stringbuf_putc(out, '/');
		}
	}

	if (ext) {
		// A trailing slash doesn't make sense with an extension.
		if (trailing_slash)
			rc |= 2;

		// You can't have an extension without a filename.
		if (out->n == base_i)
			rc |= 2;

		// Only add the extension if it isn't already present.
		struct aem_stringslice ss = aem_stringslice_new_str(out);
		if (!aem_stringslice_match_end(&ss, ext))
			aem_stringbuf_puts(out, ext);
	}

	return rc;

}


struct aem_stringslice aem_dirname(struct aem_stringslice path)
{
	// Remove trailing slashes
	while (aem_stringslice_ok(path) && path.end[-1] == '/')
		path.end--;

	// Remove trailing non-slashes
	while (aem_stringslice_ok(path) && path.end[-1] != '/')
		path.end--;

	// If empty, return .
	if (!aem_stringslice_ok(path))
		return aem_stringslice_new_cstr(".");

	// Remove trailing slashes
	while (aem_stringslice_ok(path) && path.end[-1] == '/')
		path.end--;

	// If empty, return /
	if (!aem_stringslice_ok(path))
		return aem_stringslice_new_cstr("/");

	return path;
}
