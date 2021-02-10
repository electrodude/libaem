#ifndef AEM_TRANSLATE_H
#define AEM_TRANSLATE_H

#include <aem/stringbuf.h>
#include <aem/stringslice.h>

void aem_string_escape(struct aem_stringbuf *restrict str, struct aem_stringslice slice);
void aem_string_unescape(struct aem_stringbuf *restrict str, struct aem_stringslice *restrict slice);

void aem_string_urlencode(struct aem_stringbuf *restrict out, struct aem_stringslice in);
void aem_string_urldecode(struct aem_stringbuf *restrict out, struct aem_stringslice *restrict in);


#endif /* AEM_TRANSLATE_H */
