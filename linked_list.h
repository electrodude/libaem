#ifndef AEM_LINKED_LIST_H
#define AEM_LINKED_LIST_H

#ifndef aem_assert
#include <aem/log.h>
#endif

// Linked lists with sentinel nodes


// Initialize a linked list

#define AEM_LL1_INIT(chain, next) \
	(chain)->next = (chain) \

#define AEM_LL2_INIT(chain, name) do { \
	AEM_LL1_INIT((chain), name##_prev); \
	AEM_LL1_INIT((chain), name##_next); \
} while (0)


// Insert an item into a linked list

#define AEM_LL1_INSERT_AFTER(chain, node, next) do { \
	__typeof__(chain) _chain = (chain); \
	__typeof__(node ) _node  = (node ); \
	\
	(_node)->next = (_chain)->next; \
	(_chain)->next = (_node); \
} while (0)

#define AEM_LL2_INSERT_BEFORE_4(chain, node, prev, next) do { \
	__typeof__(chain) _chain = (chain); \
	__typeof__(node ) _node  = (node ); \
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


// Remove an item from a linked list

#define AEM_LL2_REMOVE_EXPLICIT(node, prev, next) do { \
	__typeof__(node) _node = (node); \
	\
	(_node)->next->prev = (_node)->prev; \
	(_node)->prev->next = (_node)->next; \
	(_node)->next = (_node); \
	(_node)->prev = (_node); \
} while (0)

#define AEM_LL2_REMOVE(node, name) \
	AEM_LL2_REMOVE_EXPLICIT(node, name##_prev, name##_next)

#define AEM_LL_EMPTY(chain, next) ((chain)->next == (chain))
#define AEM_LL2_EMPTY(chain, name) AEM_LL_EMPTY((chain), name##_next)


// Iterate over a linked list

#define AEM_LL_FOR_RANGE_TP(T, curr, start, end, next) \
	for (T curr = (start); curr != (end); curr = curr->next)

#define AEM_LL_FOR_RANGE(curr, start, end, next) \
	AEM_LL_FOR_RANGE_TP(__typeof__(start), curr, (start), (end), next)

#define AEM_LL_FOR_ALL_TP(T, curr, chain, next) \
	AEM_LL_FOR_RANGE_TP(T, curr, (chain)->next, (chain), next)

#define AEM_LL_FOR_ALL(curr, chain, next) \
	AEM_LL_FOR_ALL_TP(__typeof__((chain)->next), curr, (chain), next)

#define AEM_LL2_FOR_RANGE_TP(T, curr, start, end, name) \
	AEM_LL_FOR_RANGE_TP(T, curr, start, end, name##_next)
#define AEM_LL2_FOR_RANGE(curr, start, end, name) \
	AEM_LL_FOR_RANGE(curr, start, end, name##_next)
#define AEM_LL2_FOR_ALL_TP(T, curr, chain, name) \
	AEM_LL_FOR_ALL_TP(T, curr, chain, name##_next)
#define AEM_LL2_FOR_ALL(curr, chain, name) \
	AEM_LL_FOR_ALL(curr, chain, name##_next)


// Iterate over a linked list, deleting an element if curr is set to NULL by the loop body

#define AEM_LL_FILTER_RANGE_TP(T, curr, start, end, next) \
	for (T *aem_ll_prev = (start), *curr, *aem_ll_next; \
	\
		(aem_ll_next = aem_ll_prev->next->next), \
		(curr = aem_ll_prev->next) != (end); \
	\
		curr ? (aem_ll_prev = aem_ll_prev->next /* advance */) \
		     : (aem_ll_prev->next = aem_ll_next /* remove curr */))

#define AEM_LL_FILTER_RANGE(curr, start, end, next) \
	AEM_LL_FILTER_RANGE_TP(__typeof__(*(start)), curr, (start), (end), next)

#define AEM_LL_FILTER_ALL_TP(T, curr, chain, next) \
	AEM_LL_FILTER_RANGE_TP(T, curr, (chain), (chain), next)

#define AEM_LL_FILTER_ALL(curr, chain, next) \
	AEM_LL_FILTER_ALL_TP(__typeof__(*(chain)->next), curr, (chain), next)


// Repeatedly look at the first element of a linked list until the list is empty.
#define AEM_LL_WHILE_FIRST_TP(T, curr, chain, next) \
	for (T *curr; (curr = (chain)->next) != (chain);)

#define AEM_LL_WHILE_FIRST(curr, chain, next) \
	AEM_LL_WHILE_FIRST_TP(__typeof__(*(chain)->next), curr, (chain), next)

// Empty a linked list
// Calls provided destructor on first element until no elements remain.
#define AEM_LL_DTOR(chain, next, dtor) \
	AEM_LL_WHILE_FIRST(aem_ll_curr, (chain), next) { \
		(dtor)(aem_ll_curr); \
	}

// Verify a doubly linked list

#define AEM_LL2_VERIFY(chain, prev, next, assert) \
	AEM_LL_FOR_ALL(_curr, chain, next) { \
		assert((_curr)->prev->next == (_curr)); \
		assert((_curr)->next->prev == (_curr)); \
	}

#endif /* AEM_LINKED_LIST_H */
