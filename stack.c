#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "stack.h"

struct stack *stack_alloc_raw(void)
{
	struct stack *stk = malloc(sizeof(*stk));

	if (stk == NULL) return NULL;

	*stk = STACK_EMPTY;

	return stk;
}

struct stack *stack_init_prealloc(struct stack *stk, size_t maxn)
{
	if (stk == NULL) return NULL;

	stk->n = 0;
	stk->maxn = maxn;
	stk->s = malloc(stk->maxn * sizeof(void*));

	return stk;
}

struct stack *stack_init_v(struct stack *stk, size_t n, ...)
{
	va_list ap;
	va_start(ap, n);

	stack_init_prealloc(stk, n);

	for (size_t i = 0; i < n; i++)
	{
		stack_push(stk, va_arg(ap, void*));
	}

	va_end(ap);

	return stk;
}

struct stack *stack_init_array(struct stack *stk, size_t n, void **restrict elements)
{
	stack_init_prealloc(stk, n);

	stack_pushn(stk, n, elements);

	return stk;
}

struct stack *stack_init_stack(struct stack *stk, struct stack *restrict orig)
{
	if (stk == NULL) return NULL;

	stack_init_prealloc(stk, orig->maxn);

	stk->n = orig->n;

	STACK_FOREACH(i, orig)
	{
		stk->s[i] = orig->s[i];
	}

	return stk;
}

void stack_free(struct stack *stk)
{
	if (stk == NULL) return;

	stack_dtor(stk);

	free(stk);
}

void stack_dtor(struct stack *stk)
{
	if (stk == NULL) return;

	stack_reset(stk);

	stk->n = stk->maxn = 0;

	if (stk->s == NULL) return;

	free(stk->s);

	stk->s = NULL;
}


void **stack_release(struct stack *stk, size_t *n)
{
	if (stk == NULL)
	{
		*n = 0;
		return NULL;
	}

	stack_shrink(stk);

	void **s = stk->s;
	*n = stk->n;

	free(stk);

	return s;
}


static inline void stack_grow(struct stack *stk, size_t maxn_new)
{
#if DEBUG
	fprintf(stderr, "realloc: %zd, %zd -> %zd\n", stk->n, stk->maxn, maxn_new);
#endif
	stk->maxn = maxn_new;
	stk->s = realloc(stk->s, stk->maxn * sizeof(void*));
}

void *stack_shrink(struct stack *stk)
{
	if (stk == NULL) return NULL;

	stack_grow(stk, stk->n);

	return stk->s;
}

void stack_reserve(struct stack *stk, size_t maxn)
{
	if (stk == NULL) return;

	if (stk->maxn < maxn)
	{
		stack_grow(stk, maxn);
	}
}


void stack_push(struct stack *stk, void *s)
{
	if (stk == NULL) return;

#if DEBUG
	fprintf(stderr, "Push %p\n", s);
#endif

	if (stk->maxn < stk->n + 1)
	{
		stack_grow(stk, (stk->n + 1)*2);
	}

	stk->s[stk->n++] = s;
}

void stack_pushn(struct stack *restrict stk, size_t n, void **restrict elements)
{
	if (stk == NULL) return;

	if (elements == NULL) return;

	for (size_t i = 0; i < n; i++)
	{
		stack_push(stk, elements[i]);
	}
}

void stack_append(struct stack *restrict stk, struct stack *restrict stk2)
{
	if (stk == NULL) return;

	if (stk2 == NULL) return;

	size_t n = stk->n + stk2->n;
	// make room for stk2
	if (stk->maxn < n)
	{
		stack_grow(stk, n*2);
	}

	memcpy(&stk->s[stk->n], stk2->s, stk2->n*sizeof(void*));

	stk->n = n;
}

size_t stack_transfer(struct stack *restrict dest, struct stack *restrict src, size_t n)
{
	if (dest == NULL) return 0;
	if (src  == NULL) return 0;

	if (src->n < n) return 0;

	size_t new_top = src->n - n;

	for (size_t i = 0; i < n; i++)
	{
		stack_push(dest, src->s[new_top + i]);
	}

	stack_trunc(src, new_top);

	return n;
}

void *stack_pop(struct stack *stk)
{
	if (stk == NULL) return NULL;

	if (stk->n <= 0)
	{
#if DEBUG
		fprintf(stderr, "Pop underflow!\n");
#endif

		return NULL;
	}

	void *p = stk->s[--stk->n];

#if DEBUG
	fprintf(stderr, "Pop %p\n", p);
#endif

	return p;
}

void *stack_peek(struct stack *stk)
{
	if (stk == NULL) return NULL;

	if (stk->n <= 0)
	{
#if DEBUG
		fprintf(stderr, "Peek underflow!\n");
#endif

		return NULL;
	}

#if DEBUG
	fprintf(stderr, "Peek %p\n", stk->s[stk->n-1]);
#endif

	return stk->s[stk->n-1];
}

void *stack_index_end(struct stack *stk, size_t i)
{
	if (stk == NULL) return NULL;

	size_t i2 = stk->n - 1 - i;

	if (i >= stk->n)
	{
#if DEBUG
		fprintf(stderr, "Index rev [-%zd = %zd] underflow!\n", i, i2);
#endif

		return NULL;
	}

#if DEBUG
	fprintf(stderr, "Index rev [-%zd = %zd]\n", i, i2, stk->s[i2]);
#endif

	return stk->s[i2];
}

void *stack_index(struct stack *stk, size_t i)
{
	if (stk == NULL) return NULL;

	if (i >= stk->n)
	{
		return NULL;
	}
#if DEBUG
	fprintf(stderr, "Index [%zd] = %p\n", i, stk->s[i]);
#endif

	return stk->s[i];
}

void stack_assign(struct stack *stk, size_t i, void *s)
{
	if (stk == NULL) return;


	if (i + 1 > stk->n)
	{
		stk->n = i + 1;

		if (stk->n > stk->maxn)
		{
			stack_grow(stk, stk->n);
		}
	}

	stk->s[i] = s;
}


void stack_qsort(struct stack *stk, int (*compar)(const void *p1, const void *p2))
{
	qsort(stk->s, stk->n, sizeof(stk->s[0]), compar);
}
