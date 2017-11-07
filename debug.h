#ifndef AEM_DEBUG_H
#define AEM_DEBUG_H

#include <stdio.h>

extern FILE *aem_debug_fp;

int aem_dprintf(const char *fmt, ...);

#define aem_dprintf_ctx(fmt, ...) aem_dprintf("%s:%d(%s): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define aem_assert(condition) if (condition) { aem_dprintf_ctx("assertion failed: ", #condition); abort(); }

#endif /* AEM_DEBUG_H */
