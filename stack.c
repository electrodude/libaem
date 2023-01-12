#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/memory.h>

#include "stack.h"

struct aem_stack *aem_stack_new(void)
{
	struct aem_stack *stk = malloc(sizeof(*stk));

	if (!stk) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc() failed: %s", strerror(errno));
		return NULL;
	}

	*stk = AEM_STACK_EMPTY;

	return stk;
}

struct aem_stack *aem_stack_init_prealloc(struct aem_stack *stk, size_t maxn)
{
	if (!stk)
		return NULL;

	*stk = AEM_STACK_EMPTY;
	if (maxn)
		aem_stack_reserve_total(stk, maxn);

	return stk;
}

void aem_stack_free(struct aem_stack *stk)
{
	if (!stk)
		return;

	aem_stack_dtor(stk);

	free(stk);
}

void aem_stack_dtor(struct aem_stack *stk)
{
	if (!stk)
		return;

	free(stk->s);
	*stk = AEM_STACK_EMPTY;
}

void **aem_stack_release(struct aem_stack *stk, size_t *n_p)
{
	if (!stk) {
		if (n_p)
			*n_p = 0;
		return NULL;
	}

	void **s = aem_stack_shrinkwrap(stk);

	if (n_p)
		*n_p = stk->n;

	free(stk);

	return s;
}

void *aem_stack_shrinkwrap(struct aem_stack *stk)
{
	if (!stk)
		return NULL;

	size_t maxn_new = stk->n;
	if (AEM_ARRAY_RESIZE(stk->s, maxn_new)) {
		aem_logf_ctx(AEM_LOG_ERROR, "realloc() failed: %s", strerror(errno));
	} else {
		stk->maxn = maxn_new;
	}

	return stk->s;
}

int aem_stack_reserve(struct aem_stack *stk, size_t len)
{
	aem_assert(stk);

	return aem_stack_reserve_total(stk, stk->n + len);
}

int aem_stack_reserve_total(struct aem_stack *stk, size_t maxn)
{
	aem_assert(stk);

	return AEM_ARRAY_GROW(stk->s, maxn, stk->maxn);
}


void aem_stack_push(struct aem_stack *stk, void *s)
{
	aem_assert(stk);

	aem_stack_reserve(stk, 1);

	stk->s[stk->n++] = s;
}

void aem_stack_pushn(struct aem_stack *restrict stk, size_t n, void *const *restrict s)
{
	if (!n)
		return;

	aem_assert(stk);
	aem_assert(s);

	aem_stack_reserve(stk, n);

	memcpy(&stk->s[stk->n], s, n*sizeof(void*));

	stk->n += n;
}

void aem_stack_pushv(struct aem_stack *stk, size_t n, ...)
{
	aem_assert(stk);

	va_list ap;
	va_start(ap, n);

	aem_stack_reserve(stk, n);

	for (size_t i = 0; i < n; i++) {
		aem_stack_push(stk, va_arg(ap, void*));
	}

	va_end(ap);
}

size_t aem_stack_transfer(struct aem_stack *restrict dest, struct aem_stack *restrict src, size_t n)
{
	if (!n)
		return 0;

	aem_assert(dest);
	aem_assert(src);

	if (src->n < n)
		return 0;

	size_t new_top = src->n - n;

	aem_stack_pushn(dest, n, &src->s[new_top]);
	aem_stack_trunc(src, new_top);

	return n;
}

void *aem_stack_pop(struct aem_stack *stk)
{
	if (!stk)
		return NULL;

	if (!stk->n)
		return NULL;

	void *p = stk->s[--stk->n];

	return p;
}

void *aem_stack_peek(struct aem_stack *stk)
{
	if (!stk)
		return NULL;

	if (stk->n <= 0)
		return NULL;

	return stk->s[stk->n-1];
}

void *aem_stack_index_end(struct aem_stack *stk, size_t i)
{
	if (!stk)
		return NULL;

	size_t i2 = stk->n - 1 - i;

	if (i >= stk->n)
		return NULL;

	return stk->s[i2];
}

void *aem_stack_index(struct aem_stack *stk, size_t i)
{
	if (!stk)
		return NULL;

	if (i >= stk->n)
		return NULL;

	return stk->s[i];
}

void **aem_stack_index_p(struct aem_stack *stk, size_t i)
{
	aem_assert(stk);

	//aem_stack_reserve_total(stk, i+1)
	// Push NULLs until i is a valid index.
	while (i >= stk->n) {
		aem_stack_push(stk, NULL);
	}

	return &stk->s[i];
}

void aem_stack_assign(struct aem_stack *stk, size_t i, void *s)
{
	aem_assert(stk);

	void **p = aem_stack_index_p(stk, i);
	aem_assert(p);

	*p = s;
}

size_t aem_stack_assign_empty(struct aem_stack *stk, void *s)
{
	aem_assert(stk);

	// Skip over elements until we find one that's NULL (or nonexistent)
	size_t i = 0;
	while (aem_stack_index(stk, i))
		i++;

	aem_stack_assign(stk, i, s);
	return i;
}

void *aem_stack_remove(struct aem_stack *stk, size_t i)
{
	aem_assert(stk);

	if (i >= stk->n)
		return NULL;

	// Replace object with NULL
	void *p = stk->s[i];
	stk->s[i] = NULL;

	// Decrease vector size as much as possible.
	// Pop all trailing NULL elements.
	while (stk->n && !stk->s[stk->n-1])
		stk->n--;

	return p;
}

int aem_stack_insert(struct aem_stack *stk, size_t i, void *p)
{
	aem_assert(stk);

	if (i > stk->n)
		return 1;

	aem_stack_reserve(stk, 1);
	memmove(&stk->s[i+1], &stk->s[i], (stk->n - i)*sizeof(p));
	stk->s[i] = p;
	stk->n++;

	return 0;
}

int aem_stack_insert_end(struct aem_stack *stk, size_t i, void *p)
{
	aem_assert(stk);

	if (i > stk->n)
		return 1;

	return aem_stack_insert(stk, stk->n - i, p);
}

void aem_stack_qsort(struct aem_stack *stk, int (*compar)(const void *p1, const void *p2))
{
	aem_assert(stk);

	qsort(stk->s, stk->n, sizeof(stk->s[0]), compar);
}
