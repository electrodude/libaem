#ifndef AEM_STACK_H
#define AEM_STACK_H

// for size_t
#include <stddef.h>

// Stack class

struct stack
{
	void **s;                                               // pointer to stack buffer
	size_t n;                                               // current size of stack
	size_t maxn;                                            // number of allocated slots
};

#define STACK_FOREACH(_i, _stk) for (size_t _i = 0; _i < (_stk)->n; _i++)

#define STACK_EMPTY ((struct stack){0})

struct stack *stack_alloc_raw(void);

// Create a new stack, given how many slots to preallocate
#define stack_new_prealloc(maxn) stack_init_prealloc(stack_alloc_raw(), maxn)
struct stack *stack_init_prealloc(struct stack *stk, size_t maxn);

// Create a new stack
#define stack_new() stack_init(stack_alloc_raw())
#define stack_init(stk) stack_init_prealloc(stk, 16)

// Create a new stack and fill it with the given data
#define stack_new_v(n, ...) stack_init_v(stack_alloc_raw(), n, __VA_ARGS__)
struct stack *stack_init_v(struct stack *stk, size_t n, ...);

// Create a new stack and fill it with the given data
#define stack_new_array(n, elements) stack_init_array(stack_alloc_raw(), n, elements)
struct stack *stack_init_array(struct stack *stk, size_t n, void **elements);

// Clone a stack
#define stack_dup(orig) stack_init_stack(stack_alloc_raw(), orig)
struct stack *stack_init_stack(struct stack *stk, struct stack *orig);

// Destroy given stack and its internal buffer
// Calls stack_reset on itself first.
void stack_free(struct stack *stk);

// Destroy given stack's internal buffer, but does not free the stack itself
// If you want to use the given stack again, you must call stack_new* on
// it first.
// Use this for non-dynamically-allocated stacks
void stack_dtor(struct stack *stk);


// Reset stack size to n
// Does nothing if stack is already the same size or smaller
static inline void stack_trunc(struct stack *stk, size_t n)
{
	if (stk == NULL) return;

	stk->n = n;
}

// Reset stack size to 0
static inline void stack_reset(struct stack *stk)
{
	stack_trunc(stk, 0);
}

// Destroy given stack, returning its internal buffer and writing the number of
// elements to n
// The caller assumes responsibilty for free()ing the returned buffer.
void **stack_release(struct stack *stk, size_t *n);

// realloc() internal buffer to be as small as possible
void *stack_shrink(struct stack *stk);

// Ensure there is space allocated for at least maxn elements
void stack_reserve(struct stack *stk, size_t maxn);

// Push an element onto the top of the stack
void stack_push(struct stack *stk, void *s);

// Push n elements onto the top of the stack
void stack_pushn(struct stack *stk, size_t n, void **elements);

// Append stk2 to the end of stk
void stack_append(struct stack *stk, struct stack *stk2);

// Transfer n elements from src to dest
// Transfers nothing and returns -1 if src->n < n
// Returns the number of transfered elements, or -1 if they could not all be
//  transfered.
// TODO: Should return an ssize_t, but they apparently don't exist on non-POSIX
//  systems.
int stack_transfer(struct stack *dest, struct stack *src, size_t n);

// Pop the top element off of the stack
void *stack_pop(struct stack *stk);

// Peek at the top element of the stack
void *stack_peek(struct stack *stk);

// Return the i-th element from the top of the stack, or NULL if out of range
// Is zero indexed, so stack_index_end(stk, 0) == stack_peek(stk)
void *stack_index_end(struct stack *stk, size_t i);

// Return the i-th element from the bottom of the stack, or NULL if out of range
void *stack_index(struct stack *stk, size_t i);

// Set the i-th element from the bottom of the stack
// Increase the size of the stack if necessary
void stack_assign(struct stack *stk, size_t i, void *s);


#endif /* AEM_STACK_H */
