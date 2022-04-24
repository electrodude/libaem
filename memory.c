#include <stdlib.h>

#define AEM_INTERNAL
#include <aem/log.h>

#include "memory.h"

int aem_array_realloc_impl(void **arr_p, size_t size, size_t alloc_new)
{
	aem_assert(arr_p);

#ifdef AEM_DEBUG
	// TODO: Add out-of-memory fault injection machinery
#endif

	if (alloc_new && size) {
		size_t bytes = alloc_new * size;
		// Detect integer multiplication overflow.
		// GCC -O2 and clang -O1 are smart enough to just check the
		// flags from the multiplication instead of doing all this.
		if (bytes / size != alloc_new)
			return -1;

		void *arr_new = realloc(*arr_p, bytes);
		if (!arr_new)
			return -1;

		*arr_p = arr_new;
	} else {
		// !alloc_new => free
		free(*arr_p);
		*arr_p = NULL;
	}

	return 0;
}

int aem_array_grow_impl(void **arr_p, size_t size, size_t *alloc_p, size_t nr)
{
	aem_assert(alloc_p);
	size_t alloc_old = *alloc_p;
	if (nr > alloc_old) {
		size_t alloc_new = alloc_old*2;
		if (alloc_new < nr)
			alloc_new = nr + 8;

		// TODO: Resize down if nr*4 < alloc_old

		int rc = aem_array_realloc_impl(arr_p, size, alloc_new);
		if (rc < 0)
			return rc;

		*alloc_p = alloc_new;

		return 1;
	}
	return 0;
}
