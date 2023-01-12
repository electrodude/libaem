#ifndef AEM_STACK_H
#define AEM_STACK_H

#include <stdint.h>

#include <aem/log.h>

// Stack class

struct aem_stack {
	void **s;         // Pointer to stack buffer
	size_t n;         // Current size of stack
	size_t maxn;      // Number of allocated slots
};

#define AEM_STACK_FOREACH(_i, _stk) for (size_t _i = 0; _i < (_stk)->n; _i++)

// Initialize new instances to this value
#define AEM_STACK_EMPTY ((struct aem_stack){0})

// malloc() and initialize a new aem_stack
struct aem_stack *aem_stack_new(void);

// Initialize a stack, preallocating the given number of elements
struct aem_stack *aem_stack_init_prealloc(struct aem_stack *stk, size_t maxn);
#define aem_stack_new_prealloc(maxn) aem_stack_init_prealloc(aem_stack_new(), maxn)

// Create a new stack.
static inline struct aem_stack *aem_stack_init(struct aem_stack *stk);

// Free a malloc'd stack and its buffer.
void aem_stack_free(struct aem_stack *stk);

// Free a stack's buffer and reset it to its initial state.
void aem_stack_dtor(struct aem_stack *stk);

// Free malloc'd stack, returning its internal buffer and writing the
// number of elements to n
// The caller assumes responsibilty for free()ing the returned buffer.
void **aem_stack_release(struct aem_stack *stk, size_t *n_p);


// Reset stack size to n
// Does nothing if stack is already the same size or smaller
static inline void aem_stack_trunc(struct aem_stack *stk, size_t n);

// Reset stack size to 0
static inline void aem_stack_reset(struct aem_stack *stk);

// Return the number of available allocated elements
static inline int aem_stack_available(const struct aem_stack *stk);

// realloc() internal buffer to be as small as possible
// Returns pointer to internal buffer
void *aem_stack_shrinkwrap(struct aem_stack *stk);

// Ensure there is space allocated for at least len more elements
int aem_stack_reserve(struct aem_stack *stk, size_t len);
// Ensure there is space allocated for a total of at least maxn elements
int aem_stack_reserve_total(struct aem_stack *stk, size_t maxn);

// Push an element onto the top of the stack
void aem_stack_push(struct aem_stack *stk, void *s);

// Push n elements onto the top of the stack.
void aem_stack_pushn(struct aem_stack *stk, size_t n, void *const *s);

// Push n elements specified as varargs onto the top of the stack.
void aem_stack_pushv(struct aem_stack *stk, size_t n, ...);

// Append stk2 to the end of stk.
static inline void aem_stack_append(struct aem_stack *stk, const struct aem_stack *stk2);

// Transfer n elements from src to dest, preserving order.
// Does nothing and returns 0 if exactly n elements couldn't be transfered.
// Returns the number of transfered elements - either n or 0.
// aem_stack_transfer(abcdef, 012345, 3) -> abcdef345, 012
size_t aem_stack_transfer(struct aem_stack *dest, struct aem_stack *src, size_t n);

// Pop the top element off of the stack.
void *aem_stack_pop(struct aem_stack *stk);

// Peek at the top element of the stack.
void *aem_stack_peek(struct aem_stack *stk);

// Return the i-th element from the top of the stack, or NULL if out of range.
// Is zero indexed, so aem_stack_index_end(stk, 0) == aem_stack_peek(stk)
void *aem_stack_index_end(struct aem_stack *stk, size_t i);

// Return the i-th element from the bottom of the stack, or NULL if out of range.
static inline void *aem_stack_index(struct aem_stack *stk, size_t i);
static inline const void *aem_stack_index_const(const struct aem_stack *stk, size_t i);

// Return a pointer to the i-th element from the bottom of the stack.
// If the specified index is invalid, push NULL until it is.
void **aem_stack_index_p(struct aem_stack *stk, size_t i);

// Set the i-th element from the bottom of the stack.
// Increase the size of the stack if necessary
void aem_stack_assign(struct aem_stack *stk, size_t i, void *s);

// Set the first NULL element from the bottom of the stack, and return the index.
// Increase the size of the stack if necessary
size_t aem_stack_assign_empty(struct aem_stack *stk, void *s);

// Set the i-th element to NULL, and then remove any NULL elements at the end
// of the stack.
// Returns a pointer to the removed element.
void *aem_stack_remove(struct aem_stack *stk, size_t i);

// Inserts an element at position i.
// Returns zero on success or non-zero if the position was invalid.
int aem_stack_insert(struct aem_stack *stk, size_t i, void *p);

// Inserts an element at the i-th position from the end.
// Returns zero on success or non-zero if the position was invalid.
int aem_stack_insert_end(struct aem_stack *stk, size_t i, void *p);

// qsort a stack
void aem_stack_qsort(struct aem_stack *stk, int (*compar)(const void *p1, const void *p2));


/// Implementations of inline functions
static inline struct aem_stack *aem_stack_init(struct aem_stack *stk)
{
	if (!stk)
		return stk;

	*stk = AEM_STACK_EMPTY;

	return stk;
}

static inline void aem_stack_append(struct aem_stack *stk, const struct aem_stack *stk2)
{
	aem_assert(stk2);

	aem_stack_pushn(stk, stk2->n, stk2->s);
}

static inline void aem_stack_trunc(struct aem_stack *stk, size_t n)
{
	if (!stk)
		return;

	if (stk->n < n)
		return;

	stk->n = n;
}

static inline void aem_stack_reset(struct aem_stack *stk)
{
	if (!stk)
		return;

	stk->n = 0;
}

static inline int aem_stack_available(const struct aem_stack *stk)
{
	aem_assert(stk);
	return stk->maxn - stk->n;
}

static inline void *aem_stack_index(struct aem_stack *stk, size_t i)
{
	if (!stk)
		return NULL;

	if (i >= stk->n)
		return NULL;

	return stk->s[i];
}
static inline const void *aem_stack_index_const(const struct aem_stack *stk, size_t i)
{
	if (!stk)
		return NULL;

	if (i >= stk->n)
		return NULL;

	return stk->s[i];
}

#endif /* AEM_STACK_H */
