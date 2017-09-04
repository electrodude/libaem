#ifndef AEM_LINKED_LIST_H
#define AEM_LINKED_LIST_H

#define LL_INIT(chain, prev, next) do { \
	(chain)->prev = (chain); \
	(chain)->next = (chain); \
} while (0)

#define LL_INSERT_BEFORE(chain, node, prev, next) do { \
	__typeof__(chain) _chain = (chain); \
	__typeof__(node ) _node  = (node ); \
	(_node )->next       = (_chain); \
	(_node )->prev       = (_chain)->prev; \
	(_chain)->prev->next = (_node ); \
	(_chain)->prev       = (_node ); \
} while (0)

#define LL_INSERT_AFTER(chain, node, prev, next) LL_INSERT_BEFORE(chain, node, next, prev)

#define LL_REMOVE(node, prev, next) do { \
	__typeof__(node) _node = (node); \
	(_node)->next->prev = (_node)->prev; \
	(_node)->prev->next = (_node)->next; \
} while (0)

#define LL_FOR_RANGE(curr, start, end, prev, next) \
	for (__typeof__(start) curr = start; curr != (end); curr = curr->next)

#define LL_FOR_ALL(curr, chain, prev, next) \
	LL_FOR_RANGE(curr, (chain)->next, (chain), prev, next)

#define LL_VERIFY(chain, prev, next, assert) do { \
	LL_FOR_ALL(_curr, chain, prev, next) \
	{ \
		assert(_curr->prev->next == _curr); \
		assert(_curr->next->prev == _curr); \
	} \
} while (0)

#endif /* AEM_LINKED_LIST_H */
