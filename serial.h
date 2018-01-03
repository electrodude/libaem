#ifndef AEM_SERIAL_H
#define AEM_SERIAL_H

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "stringbuf.h"
#include "stringslice.h"

struct aem_serial
{
#ifndef _WIN32
	int fd;
#else
	HANDLE fd;
#endif
};

int aem_serial_open(struct aem_serial *s, const char *device, int baud);
int aem_serial_close(struct aem_serial *s);

int aem_serial_ok(struct aem_serial *s);

size_t aem_serial_write(struct aem_serial *s, struct aem_stringslice out);
size_t aem_serial_read(struct aem_serial *s, struct aem_stringbuf *in);
int aem_serial_getc(struct aem_serial *s);

#endif /* AEM_SERIAL_H */
