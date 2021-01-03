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
#define AEM_GC_REGISTER(_tp, _obj, _ctx) aem_gc_register(&(_obj)->gc, &_tp##_vtbl, (_ctx))

void aem_gc_run(struct aem_gc_context *ctx);

void aem_gc_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx);
#define AEM_GC_MARK_OBJ(_obj) do { \
	__typeof__(_obj) _obj_ = (_obj); \
	if (_obj_) \
		aem_gc_mark(&_obj_->gc, ctx); \
} while (0)

#define AEM_GC_FREE_DECL(_tp, _obj) void _tp##_free(struct aem_gc_object *obj, struct aem_gc_context *ctx)
#define AEM_GC_DTOR_DECL(_tp, _obj) void _tp##_dtor(struct aem_gc_object *obj, struct aem_gc_context *ctx)
#define AEM_GC_MARK_DECL(_tp, _obj) void _tp##_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx)

#define AEM_GC_GET_OBJ(_tp, _obj) struct _tp *_obj = caa_container_of(obj, struct _tp, gc);

#define AEM_GC_VTBL_INST(_tp)     \
struct aem_gc_vtbl _tp##_vtbl = { \
	.name = #_tp,             \
	.free = _tp##_free,       \
	.dtor = _tp##_dtor,       \
	.mark = _tp##_mark,       \
};


void aem_gc_ref(struct aem_gc_object *obj);
void aem_gc_unref(struct aem_gc_object *obj);


#endif /* AEM_GC_H */
