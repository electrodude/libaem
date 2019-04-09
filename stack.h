#ifndef AEM_STACK_H
#define AEM_STACK_H

// for size_t
#include <stddef.h>

// Stack class

struct aem_stack
{
	void **s;         // pointer to stack buffer
	size_t n;         // current size of stack
	size_t maxn;      // number of allocated slots
};

#ifndef AEM_STACK_PREALLOC_LEN
// This may have different values in different files.
#define AEM_STACK_PREALLOC_LEN 16
#endif

#define AEM_STACK_FOREACH(_i, _stk) for (size_t _i = 0; _i < (_stk)->n; _i++)

#define AEM_STACK_EMPTY ((struct aem_stack){0})

struct aem_stack *aem_stack_alloc_raw(void);

// Create a new stack, given how many slots to preallocate
struct aem_stack *aem_stack_init_prealloc(struct aem_stack *stk, size_t maxn);
#define aem_stack_new_prealloc(maxn) aem_stack_init_prealloc(aem_stack_alloc_raw(), maxn)

// Create a new stack
#define aem_stack_init(stk) aem_stack_init_prealloc(stk, (AEM_STACK_PREALLOC_LEN))
#define aem_stack_new() aem_stack_init(aem_stack_alloc_raw())

// Create a new stack and fill it with the given data
struct aem_stack *aem_stack_init_v(struct aem_stack *stk, size_t n, ...);
#define aem_stack_new_v(n, ...) aem_stack_init_v(aem_stack_alloc_raw(), n, __VA_ARGS__)

// Create a new stack and fill it with the given data
static inline struct aem_stack *aem_stack_init_array(struct aem_stack *stk, size_t n, void **elements);
#define aem_stack_new_array(n, elements) aem_stack_init_array(aem_stack_alloc_raw(), n, elements)

// Clone a stack
static inline struct aem_stack *aem_stack_init_stack(struct aem_stack *stk, struct aem_stack *orig);
#define aem_stack_dup(orig) aem_stack_init_stack(aem_stack_alloc_raw(), orig)

// Destroy given stack and its internal buffer
// calls aem_stack_dtor first
void aem_stack_free(struct aem_stack *stk);

// Destroy given stack's internal buffer, but does not free the stack itself
// If you want to use the given stack again, you must call aem_stack_new* on
// it first.
// Use this for non-dynamically-allocated stacks
void aem_stack_dtor(struct aem_stack *stk);


// Reset stack size to n
// Does nothing if stack is already the same size or smaller
static inline void aem_stack_trunc(struct aem_stack *stk, size_t n)
{
	if (!stk) return;

	if (stk->n < n) return;

	stk->n = n;
}

// Reset stack size to 0
static inline void aem_stack_reset(struct aem_stack *stk)
{
	if (!stk) return;

	stk->n = 0;
}

// Destroy given stack, returning its internal buffer and writing the number of
// elements to n
// The caller assumes responsibilty for free()ing the returned buffer.
void **aem_stack_release(struct aem_stack *stk, size_t *n);

// realloc() internal buffer to be as small as possible
void *aem_stack_shrinkwrap(struct aem_stack *stk);

// deprecated old name
#define aem_stack_shrink aem_stack_shrinkwrap

// Ensure there is space allocated for at least maxn elements
void aem_stack_reserve(struct aem_stack *stk, size_t maxn);

// Push an element onto the top of the stack
void aem_stack_push(struct aem_stack *stk, void *s);

// Push n elements onto the top of the stack
void aem_stack_pushn(struct aem_stack *stk, size_t n, void **elements);

// Append stk2 to the end of stk
void aem_stack_append(struct aem_stack *stk, struct aem_stack *stk2);

// Transfer n elements from src to dest, preserving order
// Does nothing and returns 0 if exactly n elements couldn't be transfered.
// Returns the number of transfered elements - either n or 0.
// aem_stack_transfer(abcdef, 012345, 3) -> abcdef345, 012
size_t aem_stack_transfer(struct aem_stack *dest, struct aem_stack *src, size_t n);

// Pop the top element off of the stack
void *aem_stack_pop(struct aem_stack *stk);

// Peek at the top element of the stack
void *aem_stack_peek(struct aem_stack *stk);

// Return the i-th element from the top of the stack, or NULL if out of range
// Is zero indexed, so aem_stack_index_end(stk, 0) == aem_stack_peek(stk)
void *aem_stack_index_end(struct aem_stack *stk, size_t i);

// Return the i-th element from the bottom of the stack, or NULL if out of range
void *aem_stack_index(struct aem_stack *stk, size_t i);

// Set the i-th element from the bottom of the stack
// Increase the size of the stack if necessary
void aem_stack_assign(struct aem_stack *stk, size_t i, void *s);


// qsort a stack
void aem_stack_qsort(struct aem_stack *stk, int (*compar)(const void *p1, const void *p2));


// implementations of inline functions

static inline struct aem_stack *aem_stack_init_array(struct aem_stack *stk, size_t n, void **restrict elements)
{
	aem_stack_init_prealloc(stk, n);
	aem_stack_pushn(stk, n, elements);
	return stk;
}

static inline struct aem_stack *aem_stack_init_stack(struct aem_stack *stk, struct aem_stack *restrict orig)
{
	aem_stack_init_prealloc(stk, orig->maxn);
	aem_stack_append(stk, orig);
	return stk;
}

#endif /* AEM_STACK_H */
