#ifndef AEM_PATHUTIL_H
#define AEM_PATHUTIL_H

#include <aem/stringbuf.h>
#include <aem/stringslice.h>

struct aem_stringslice aem_stringslice_match_pathcomponent(struct aem_stringslice *in, int *trailing_slash_p);
int aem_sandbox_path(struct aem_stringbuf *out, struct aem_stringslice base, struct aem_stringslice subpath, const char *ext);

struct aem_stringslice aem_dirname(struct aem_stringslice path);

#endif /* AEM_PATHUTIL_H */
