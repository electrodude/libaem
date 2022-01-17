#ifndef AEM_ANSI_TERM_H
#define AEM_ANSI_TERM_H

#include <aem/stringslice.h>

struct aem_stringbuf;

struct aem_stringslice aem_ansi_match_csi(struct aem_stringslice *in);

size_t aem_ansi_len(struct aem_stringslice in);

void aem_ansi_strip(struct aem_stringbuf *out, struct aem_stringslice in);
void aem_ansi_strip_inplace(struct aem_stringbuf *str);

#endif /* AEM_ANSI_TERM_H */
