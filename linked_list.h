#ifndef AEM_LINKED_LIST_H
#define AEM_LINKED_LIST_H

#ifndef aem_typeof
#define aem_typeof __typeof__
#endif

#ifndef aem_assert
#include "log.h"
#endif


// Initialize a linked list

#define AEM_LL1_INIT(chain, next) \
	(chain)->next = (chain) \

// deprecated
#define AEM_LL_INIT(chain, prev, next) do { \
	AEM_LL1_INIT((chain), prev); \
	AEM_LL1_INIT((chain), next); \
} while (0)

#define AEM_LL2_INIT(chain, name) \
	AEM_LL_INIT((chain), name##_prev, name##_next)


// Insert an item into a linked list

#define AEM_LL1_INSERT_AFTER(chain, node, next) do { \
	aem_typeof(chain) _chain = (chain); \
	aem_typeof(node ) _node  = (node ); \
	\
	(_node)->next = (_chain)->next; \
	(_chain)->next = (_node); \
} while (0)

#define AEM_LL2_INSERT_BEFORE_4(chain, node, prev, next) do { \
	aem_typeof(chain) _chain = (chain); \
	aem_typeof(node ) _node  = (node ); \
	\
	(_node )->next       = (_chain); \
	(_node )->prev       = (_chain)->prev; \
	(_chain)->prev->next = (_node ); \
	(_chain)->prev       = (_node ); \
} while (0)

#define AEM_LL2_INSERT_AFTER_4(chain, node, prev, next) \
	AEM_LL2_INSERT_BEFORE_4((chain), (node), next, prev)

#define AEM_LL2_INSERT_BEFORE(chain, node, name) \
	AEM_LL2_INSERT_BEFORE_4((chain), (node), name##_prev, name##_next)
#define AEM_LL2_INSERT_AFTER(chain, node, name) \
	AEM_LL2_INSERT_AFTER_4((chain), (node), name##_prev, name##_next)

// deprecated
#define AEM_LL_INSERT_BEFORE(chain, node, prev, next) \
	AEM_LL2_INSERT_BEFORE_4((chain), (node), prev, next)
#define AEM_LL_INSERT_AFTER(chain, node, prev, next) \
	AEM_LL2_INSERT_AFTER_4((chain), (node), prev, next)


// Remove an item from a linked list

// deprecated
#define AEM_LL_REMOVE(node, prev, next) do { \
	aem_typeof(node) _node = (node); \
	\
	(_node)->next->prev = (_node)->prev; \
	(_node)->prev->next = (_node)->next; \
	(_node)->next = (_node); \
	(_node)->prev = (_node); \
} while (0)

#define AEM_LL2_REMOVE(node, name) \
	AEM_LL_REMOVE((node), name##_prev, name##_next)

#define AEM_LL_EMPTY(chain, next) ((chain)->next == (chain))


// Iterate over a linked list

// AEM_LL_FOR_* deprecated until prev parameter is removed
#define AEM_LL_FOR_RANGE_TP(T, curr, start, end, prev, next) \
	for (T curr = (start); curr != (end); curr = curr->next)

#define AEM_LL_FOR_RANGE(curr, start, end, prev, next) \
	AEM_LL_FOR_RANGE_TP(aem_typeof(start), curr, (start), (end), prev, next)

#define AEM_LL_FOR_ALL_TP(T, curr, chain, prev, next) \
	AEM_LL_FOR_RANGE_TP(T, curr, (chain)->next, (chain), prev, next)

#define AEM_LL_FOR_ALL(curr, chain, prev, next) \
	AEM_LL_FOR_ALL_TP(aem_typeof((chain)->next), curr, (chain), prev, next)

// Use this temporarily instead of AEM_LL_FOR_* until prev parameter is removed
// Will become deprecated in favor of AEM_LL_FOR_* when prev parameter is removed
#define AEM_LL2_FOR_RANGE_TP(T, curr, start, end, _, next) \
        AEM_LL_FOR_RANGE_TP(T, curr, start, end, , next)
#define AEM_LL2_FOR_RANGE(curr, start, end, _, next) \
        AEM_LL_FOR_RANGE(curr, start, end, , next)
#define AEM_LL2_FOR_ALL_TP(T, curr, chain, _, next) \
        AEM_LL_FOR_ALL_TP(T, curr, chain, , next)
#define AEM_LL2_FOR_ALL(curr, chain, _, next) \
        AEM_LL_FOR_ALL(curr, chain, , next)


// Iterate over a linked list, deleting an element if curr is set to NULL by the loop body

#define AEM_LL_FILTER_RANGE_TP(T, curr, start, end, next) \
	for (T *_prev = (start), *curr, *_next; \
	\
	     (_next = _prev->next->next), \
	     (curr = _prev->next) != (end); \
	\
	     curr ? (_prev = _prev->next /* advance */) \
	          : (_prev->next = _next /* remove curr */))

#define AEM_LL_FILTER_RANGE(curr, start, end, next) \
	AEM_LL_FILTER_RANGE_TP(aem_typeof(*(start)), curr, (start), (end), next)

#define AEM_LL_FILTER_ALL_TP(T, curr, chain, next) \
	AEM_LL_FILTER_RANGE_TP(T, curr, (chain), (chain), next)

#define AEM_LL_FILTER_ALL(curr, chain, next) \
	AEM_LL_FILTER_ALL_TP(aem_typeof(*(chain)->next), curr, (chain), next)


// Empty a linked list
// Calls provided destructor on first element until no elements remain.
#define AEM_LL_DTOR(chain, next, dtor) \
	do { while (!AEM_LL_EMPTY((chain), next)) { dtor((chain)->next); } } while (0)


// Verify a doubly linked list

#define AEM_LL2_VERIFY(chain, prev, next, assert) do { \
	AEM_LL_FOR_ALL(_curr, chain, prev, next) \
	{ \
		assert((_curr)->prev->next == (_curr)); \
		assert((_curr)->next->prev == (_curr)); \
	} \
} while (0)

// deprecated
#define AEM_LL_VERIFY(chain, prev, next, assert) \
	AEM_LL2_VERIFY((chain, prev, next, assert)

#endif /* AEM_LINKED_LIST_H */
