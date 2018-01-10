#ifndef AEM_LINKED_LIST_H
#define AEM_LINKED_LIST_H

#ifndef aem_typeof
#define aem_typeof __typeof__
#endif

#ifndef aem_assert
#include "log.h"
#endif

#define AEM_LL_INIT(chain, prev, next) do { \
	(chain)->prev = (chain); \
	(chain)->next = (chain); \
} while (0)

#define AEM_LL_INSERT_BEFORE(chain, node, prev, next) do { \
	aem_typeof(chain) _chain = (chain); \
	aem_typeof(node ) _node  = (node ); \
	(_node )->next       = (_chain); \
	(_node )->prev       = (_chain)->prev; \
	(_chain)->prev->next = (_node ); \
	(_chain)->prev       = (_node ); \
} while (0)

#define AEM_LL_INSERT_AFTER(chain, node, prev, next) AEM_LL_INSERT_BEFORE((chain), (node), next, prev)

#define AEM_LL_REMOVE(node, prev, next) do { \
	aem_typeof(node) _node = (node); \
	(_node)->next->prev = (_node)->prev; \
	(_node)->prev->next = (_node)->next; \
	(_node)->next = (_node); \
	(_node)->prev = (_node); \
} while (0)

#define AEM_LL_EMPTY(chain, next) ((chain)->next == (chain))

#define AEM_LL_FOR_RANGE_TP(T, curr, start, end, prev, next) \
	for (T curr = (start); curr != (end); curr = curr->next)

#define AEM_LL_FOR_RANGE(curr, start, end, prev, next) \
	AEM_LL_FOR_RANGE_TP(aem_typeof(start), curr, (start), (end), prev, next)

#define AEM_LL_FOR_ALL_TP(T, curr, chain, prev, next) \
	AEM_LL_FOR_RANGE_TP(T, curr, (chain)->next, (chain), prev, next)

#define AEM_LL_FOR_ALL(curr, chain, prev, next) \
	AEM_LL_FOR_ALL_TP(aem_typeof((chain)->next), curr, (chain), prev, next)

#define AEM_LL_VERIFY(chain, prev, next, assert) do { \
	AEM_LL_FOR_ALL(_curr, chain, prev, next) \
	{ \
		aem_assert((_curr)->prev->next == (_curr)); \
		aem_assert((_curr)->next->prev == (_curr)); \
	} \
} while (0)

#endif /* AEM_LINKED_LIST_H */
