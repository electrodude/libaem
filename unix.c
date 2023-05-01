#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>

#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/pathutil.h>

#include "unix.h"

void aem_exe_dir(struct aem_stringbuf *out)
{
	struct aem_stringbuf exe = AEM_STRINGBUF_ALLOCA(256);
again:
	// TODO: This won't work on every *nix OS
	exe.n = readlink("/proc/self/exe", exe.s, exe.maxn);
	if (exe.n == exe.maxn) {
		aem_stringbuf_reserve(&exe, exe.n);
		goto again;
	}

	aem_stringbuf_putss(out, aem_dirname(aem_stringslice_new_str(&exe)));
	aem_stringbuf_dtor(&exe);
}

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
