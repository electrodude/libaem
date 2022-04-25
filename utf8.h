#ifndef AEM_UTF8_H
#define AEM_UTF8_H

#include <stdint.h>

#define AEM_UTF8_INFO_LEN 6
extern const struct aem_utf8_info {
	uint32_t max;
	unsigned char top;
	unsigned char mask;
} aem_utf8_info[AEM_UTF8_INFO_LEN];

// Duplicated in stringbuf.h
int aem_stringbuf_put_rune(struct aem_stringbuf *str, uint32_t c);
// Duplicated in stringslice.h
int aem_stringslice_get_rune(struct aem_stringslice *slice, uint32_t *out_p);

#endif /* AEM_UTF8_H */
