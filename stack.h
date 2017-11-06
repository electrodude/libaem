#ifndef AEM_STACK_H
#define AEM_STACK_H

// for size_t
#include <stddef.h>

// Stack class

struct aem_stack
{
	void **s;                                               // pointer to stack buffer
	size_t n;                                               // current size of stack
	size_t maxn;                                            // number of allocated slots
};

#define stack aem_stack

#define STACK_FOREACH(_i, _stk) for (size_t _i = 0; _i < (_stk)->n; _i++)

#define STACK_EMPTY ((struct aem_stack){0})

struct aem_stack *stack_alloc_raw(void);

// Create a new stack, given how many slots to preallocate
#define stack_new_prealloc(maxn) stack_init_prealloc(stack_alloc_raw(), maxn)
struct aem_stack *stack_init_prealloc(struct aem_stack *stk, size_t maxn);

// Create a new stack
#define stack_new() stack_init(stack_alloc_raw())
#define stack_init(stk) stack_init_prealloc(stk, 16)

// Create a new stack and fill it with the given data
#define stack_new_v(n, ...) stack_init_v(stack_alloc_raw(), n, __VA_ARGS__)
struct aem_stack *stack_init_v(struct aem_stack *stk, size_t n, ...);

// Create a new stack and fill it with the given data
#define stack_new_array(n, elements) stack_init_array(stack_alloc_raw(), n, elements)
struct aem_stack *stack_init_array(struct aem_stack *stk, size_t n, void **elements);

// Clone a stack
#define stack_dup(orig) stack_init_stack(stack_alloc_raw(), orig)
struct aem_stack *stack_init_stack(struct aem_stack *stk, struct aem_stack *orig);

// Destroy given stack and its internal buffer
// Calls stack_reset on itself first.
void stack_free(struct aem_stack *stk);

// Destroy given stack's internal buffer, but does not free the stack itself
// If you want to use the given stack again, you must call stack_new* on
// it first.
// Use this for non-dynamically-allocated stacks
void stack_dtor(struct aem_stack *stk);


// Reset stack size to n
// Does nothing if stack is already the same size or smaller
static inline void stack_trunc(struct aem_stack *stk, size_t n)
{
	if (stk == NULL) return;

	stk->n = n;
}

// Reset stack size to 0
static inline void stack_reset(struct aem_stack *stk)
{
	stack_trunc(stk, 0);
}

// Destroy given stack, returning its internal buffer and writing the number of
// elements to n
// The caller assumes responsibilty for free()ing the returned buffer.
void **stack_release(struct aem_stack *stk, size_t *n);

// realloc() internal buffer to be as small as possible
void *stack_shrink(struct aem_stack *stk);

// Ensure there is space allocated for at least maxn elements
void stack_reserve(struct aem_stack *stk, size_t maxn);

// Push an element onto the top of the stack
void stack_push(struct aem_stack *stk, void *s);

// Push n elements onto the top of the stack
void stack_pushn(struct aem_stack *stk, size_t n, void **elements);

// Append stk2 to the end of stk
void stack_append(struct aem_stack *stk, struct aem_stack *stk2);

// Transfer n elements from src to dest, preserving order
// Does nothing and returns 0 if exactly n elements couldn't be transfered
// Returns the number of transfered elements - either n or 0
size_t stack_transfer(struct aem_stack *dest, struct aem_stack *src, size_t n);

// Pop the top element off of the stack
void *stack_pop(struct aem_stack *stk);

// Peek at the top element of the stack
void *stack_peek(struct aem_stack *stk);

// Return the i-th element from the top of the stack, or NULL if out of range
// Is zero indexed, so stack_index_end(stk, 0) == stack_peek(stk)
void *stack_index_end(struct aem_stack *stk, size_t i);

// Return the i-th element from the bottom of the stack, or NULL if out of range
void *stack_index(struct aem_stack *stk, size_t i);

// Set the i-th element from the bottom of the stack
// Increase the size of the stack if necessary
void stack_assign(struct aem_stack *stk, size_t i, void *s);


// qsort a stack
void stack_qsort(struct aem_stack *stk, int (*compar)(const void *p1, const void *p2));

#endif /* AEM_STACK_H */
