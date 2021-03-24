#ifndef AEM_UNIX_H
#define AEM_UNIX_H

#include <aem/stringbuf.h>

void aem_exe_dir(struct aem_stringbuf *out);

int aem_fd_add_flags(int fd, int newflags);

#endif /* AEM_UNIX_H */
