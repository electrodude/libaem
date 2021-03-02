#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define AEM_INTERNAL
#include <aem/log.h>

#include "unix.h"

int aem_fd_add_flags(int fd, int newflags)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "fcntl(%d, F_GETFL, 0): %s", fd, strerror(errno));
		return flags;
	}
	flags |= newflags;
	if (fcntl(fd, F_SETFL, flags)) {
		aem_logf_ctx(AEM_LOG_ERROR, "fcntl(%d, F_SETFL, %d): %s", fd, flags, strerror(errno));
		return -1;
	}
	return 0;
}
