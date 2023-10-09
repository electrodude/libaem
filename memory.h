#ifndef AEM_MEMORY_H
#define AEM_MEMORY_H

#include <stddef.h>  /* for offsetof */

int aem_array_realloc_impl(void **arr_p, size_t size, size_t alloc_new);
int aem_array_grow_impl(void **arr_p, size_t size, size_t *alloc_p, size_t nr);

// Resize given array to have alloc elements.
// arr must be an lvalue.
// Passing 0 for alloc (or sizeof(*arr)) is guaranteed to set the pointer to NULL
#define AEM_ARRAY_RESIZE(arr, alloc) \
	(aem_array_realloc_impl((void**)&(arr), sizeof *(arr), (alloc)))

// Resize given array, currently allocated to have alloc elements allocated, to
// have at least nr elements allocated.
// arr and alloc must be lvalues.
#define AEM_ARRAY_GROW(arr, nr, alloc) \
	(aem_array_grow_impl((void**)&(arr), sizeof *(arr), &(alloc), (nr)))

// Get the address of an object of `type` containing a field `member` located at `ptr`.
// ptr == ptr ? &(aem_container_of(ptr, type, member))->member : ptr
// (Returns NULL if ptr is NULL)
#ifdef AEM_CONFIG_HAVE_STMT_EXPR
# define aem_container_of(ptr, type, member) \
	__extension__({ \
		const __typeof__(((type *) NULL)->member) * __ptr = (ptr); \
		__ptr ? (type *)((char *)__ptr - offsetof(type, member)) : (type *)__ptr; \
	})
#else
// If we don't have statement expressions, fall back to this inferior version
// that doesn't enforce typeof(ptr) == typeof(type->member)
static inline void *aem_container_of_impl(void *ptr, size_t offset)
{
	if (!ptr)
		return NULL;

	return (void *)((char *)ptr - offset);
}
# define aem_container_of(ptr, type, member) \
	((type *)aem_container_of_impl((__typeof__(((type *) NULL)->member) *)(ptr), offsetof(type, member)))
#endif

#endif /* AEM_MEMORY_H */
