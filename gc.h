#ifndef AEM_GC_H
#define AEM_GC_H

#include <stddef.h>

#include <aem/compiler.h>
#include <aem/iter_gen.h>

/// GC object
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


/// GC root
struct aem_gc_root {
	struct aem_gc_root *root_prev; // AEM_LL2
	struct aem_gc_root *root_next;

	void (*mark)(struct aem_gc_root *root, struct aem_gc_context *ctx);
};

#define AEM_GC_ROOT_MARK_DECL(_tp, _obj) void _tp##_root_mark(struct aem_gc_root *root, struct aem_gc_context *ctx)
#define AEM_GC_GET_ROOT(_tp, _root) struct _tp *_root = aem_container_of(root, struct _tp, root);

// You must set root->mark yourself before calling this.
void aem_gc_root_register(struct aem_gc_root *root, struct aem_gc_context *ctx);
void aem_gc_root_deregister(struct aem_gc_context *ctx, struct aem_gc_root *root);

// Convenience macro that automagically sets _root->root.mark - use with AEM_GC_ROOT_MARK_DECL
#define AEM_GC_ROOT_REGISTER(_tp, _root, _ctx) do { struct aem_gc_root *aem_gc_root = &(_root)->root; struct aem_gc_context *aem_gc_ctx = (_ctx); aem_gc_root->mark = _tp##_root_mark; aem_gc_root_register(aem_gc_root, aem_gc_ctx); } while (0)


/// GC context
struct aem_gc_context {
	struct aem_gc_object objects;

	// objects.iter is master iterator

	struct aem_gc_root roots;
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

#define AEM_GC_FREE_DECL(_tp, _obj) void _tp##_gc_free(struct aem_gc_object *obj, struct aem_gc_context *ctx)
#define AEM_GC_DTOR_DECL(_tp, _obj) void _tp##_gc_dtor(struct aem_gc_object *obj, struct aem_gc_context *ctx)
#define AEM_GC_MARK_DECL(_tp, _obj) void _tp##_gc_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx)

#define AEM_GC_GET_OBJ(_tp, _obj) struct _tp *_obj = aem_container_of(obj, struct _tp, gc);

#define AEM_GC_VTBL_INST(_tp)     \
struct aem_gc_vtbl _tp##_vtbl = { \
	.name = #_tp,             \
	.free = _tp##_gc_free,       \
	.dtor = _tp##_gc_dtor,       \
	.mark = _tp##_gc_mark,       \
};


void aem_gc_ref(struct aem_gc_object *obj);
void aem_gc_unref(struct aem_gc_object *obj);


#endif /* AEM_GC_H */
