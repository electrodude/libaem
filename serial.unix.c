#define _DEFAULT_SOURCE
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define AEM_INTERNAL
#include <aem/log.h>

#include "serial.h"

int aem_serial_open(struct aem_serial *ser, const char *device, int baud)
{
again:;
	ser->fd = open(device, O_RDWR | O_NOCTTY);

	if (ser->fd < 0) {
		switch (errno) {
			case EINTR: goto again;
		}

		aem_logf_ctx(AEM_LOG_ERROR, "failed to open tty %s: %s\n", device, strerror(errno));

		return ser->fd;
	}

	if (baud >= 0) {
		struct termios tio;
		tcgetattr(ser->fd, &tio);
		cfmakeraw(&tio);
		cfsetspeed(&tio, baud);
		tcsetattr(ser->fd, TCSADRAIN, &tio);
	}

	if (baud >= 0) {
		aem_logf_ctx(AEM_LOG_NOTICE, "opened tty %s baud %d: fd %d\n", device, baud, ser->fd);
	} else {
		aem_logf_ctx(AEM_LOG_NOTICE, "opened tty %s: fd %d\n", device, ser->fd);
	}

	return 0;
}

int aem_serial_close(struct aem_serial *ser)
{
	if (ser->fd >= 0) {
		aem_logf_ctx(AEM_LOG_NOTICE, "close fd %d\n", ser->fd);

again:;
		int rc = close(ser->fd);
		if (rc < 0) {
			switch (errno) {
				case EINTR: goto again;
			}

			aem_logf_ctx(AEM_LOG_ERROR, "failed to close fd %d, leaking: %s\n", ser->fd, strerror(errno));
		}
		ser->fd = -1;

		return rc;
	}
	else
	{
		aem_logf_ctx(AEM_LOG_WARN, "not open\n");

		return 0;
	}
}

int aem_serial_ok(struct aem_serial *ser)
{
	return ser->fd >= 0;
}

size_t aem_serial_write(struct aem_serial *ser, struct aem_stringslice out)
{
again:;
	ssize_t n = write(ser->fd, out.start, aem_stringslice_len(out));

	if (n < 0) {
		switch (errno) {
			case EINTR: goto again;
		}

		return 0;
	}

	return n;
}

size_t aem_serial_read(struct aem_serial *ser, struct aem_stringbuf *in)
{
again:;
	ssize_t n = read(ser->fd, aem_stringbuf_end(in), aem_stringbuf_available(in));

	if (n < 0) {
		switch (errno) {
			case EINTR: goto again;
		}

		return 0;
	}

	in->n += n;

	return n;
}

int aem_serial_getc(struct aem_serial *ser)
{
	unsigned char c;
again:;
	ssize_t n = read(ser->fd, &c, 1);

	if (n < 0) {
		switch (errno) {
			case EINTR: goto again;
		}

		return -1;
	}

	return c;
}
