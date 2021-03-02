#include <windows.h>

#define AEM_INTERNAL
#include <aem/log.h>

#include "serial.h"

int aem_serial_open(struct aem_serial *s, const char *device, int baud)
{
	aem_logf_ctx(AEM_LOG_WARN, "untested on Windows: open serial port");
	ser->fd = CreateFile(device,
			     GENERIC_READ | GENERIC_WRITE,
			     0,
			     0,
			     OPEN_EXISTING,
			     0,
			     0);

	if (ser->fd == INVALID_HANDLE_VALUE) {
		aem_logf_ctx(AEM_LOG_ERROR, "failed to open tty %s", device);

		return -1;
	}

	if (baud >= 0) {
		DCB dcb;

		FillMemory(&dcb, sizeof(dcb), 0);
		dcb.DCBlength = sizeof(dcb);
		// fix the baud later
		if (!BuildCommDCB("115200,n,8,1", &dcb)) {
			aem_logf_ctx(AEM_LOG_ERROR, "BuildCommDCB(def, &dcb) failed");
			fprintf(stderr, "BuildCommDCB failed\n");
			CloseHandle(ser->fd);
			return 1;
		}

		dcb.BaudRate = baud;

		if (!SetCommState(ser->fd, &dcb)) {
			fprintf(stderr, "SetCommState failed\n");
			CloseHandle(ser->fd);
			return 1;
		}
	}

	if (baud >= 0) {
		aem_logf_ctx(AEM_LOG_NOTICE, "opened tty %s baud %d", device, baud);
	} else {
		aem_logf_ctx(AEM_LOG_NOTICE, "opened tty %s", device);
	}

	return 0;
}

int aem_serial_close(struct aem_serial *s)
{
	aem_logf_ctx(AEM_LOG_WARN, "untested on Windows");
	if (ser->fd != INVALID_HANDLE_VALUE) {
		aem_logf_ctx(AEM_LOG_NOTICE, "close fd");

		CloseHandle(ser->fd);
		ser->fd = INVALID_HANDLE_VALUE;
	} else {
		aem_logf_ctx(AEM_LOG_WARN, "not open");

		return 0;
	}
}

int aem_serial_ok(struct aem_serial *s)
{
	return ser->fd != INVALID_HANDLE_VALUE;
}

size_t aem_serial_write(struct aem_serial *s, struct aem_stringslice out)
{
	aem_logf_ctx(AEM_LOG_WARN, "untested on Windows");

	DWORD n;
	if (!WriteFile(s->fd, out.start, aem_stringslice_len(&out), &n, NULL)) {
		return 0;
	}

	return n;
}

size_t aem_serial_read(struct aem_serial *s, struct aem_stringbuf *in)
{
	aem_logf_ctx(AEM_LOG_WARN, "untested on Windows");

	DWORD n;
	if (!ReadFile(s->fd, aem_stringbuf_end(in), aem_stringbuf_available(in), &n, NULL))
	{
		return 0;
	}

	in->n += n;

	return n;
}

int aem_serial_getc(struct aem_serial *s)
{
	aem_logf_ctx(AEM_LOG_WARN, "untested on Windows");

	unsigned char c;
	DWORD n;
	if (!ReadFile(s->fd, &c, 1, &n, NULL)) {
		return -1;
	}

	return c;
}
