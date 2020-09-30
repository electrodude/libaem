#ifndef AEM_GC_H
#define AEM_GC_H

#include <stddef.h>

#include "iter_gen.h"

struct aem_gc_object;
struct aem_gc_context;

struct aem_gc_vtbl {
	const char *name;

	void (*free)(struct aem_gc_object *obj, struct aem_gc_context *ctx);
	void (*dtor)(struct aem_gc_object *obj, struct aem_gc_context *ctx);
	void (*mark)(struct aem_gc_object *obj, struct aem_gc_context *ctx);
};

void aem_gc_free_default(struct aem_gc_object *obj, struct aem_gc_context *ctx);

struct aem_gc_object {
	const struct aem_gc_vtbl *vtbl;

	// Singly linked list of instances belonging to the same aem_gc_context
	// rooted in ctx->objects
	struct aem_gc_object *ctx_next;

	struct aem_iter_gen iter;

	size_t refs;
};

struct aem_gc_context {
	struct aem_gc_object objects;

	// objects.iter is master iterator
};

void aem_gc_init(struct aem_gc_context *ctx);
void aem_gc_dtor(struct aem_gc_context *ctx);

void aem_gc_register(struct aem_gc_object *obj, const struct aem_gc_vtbl *vtbl, struct aem_gc_context *ctx);

void aem_gc_run(struct aem_gc_context *ctx);

void aem_gc_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx);


void aem_gc_ref(struct aem_gc_object *obj);
void aem_gc_unref(struct aem_gc_object *obj);


#endif /* AEM_GC_H */
