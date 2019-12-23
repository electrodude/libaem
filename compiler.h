#ifndef AEM_COMPILER_H
#define AEM_COMPILER_H

#include <stddef.h>  /* for offsetof */

/*
 * aem_container_of - Get the address of an object containing a field.
 *
 * @ptr: pointer to the field.
 * @type: type of the object.
 * @member: name of the field within the object.
 */
#define aem_container_of(ptr, type, member)                                \
	__extension__                                                      \
	({                                                                 \
		const __typeof__(((type *) NULL)->member) * __ptr = (ptr); \
		(type *)((char *)__ptr - offsetof(type, member));          \
	})

#endif /* AEM_COMPILER_H */
