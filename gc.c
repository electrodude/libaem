#include <stdlib.h>

#define AEM_INTERNAL
#define aem_log_module_current (&gc_log_module)
#include <aem/log.h>
#include <aem/linked_list.h>

#include "gc.h"

static struct aem_log_module gc_log_module = {.loglevel = AEM_LOG_INFO};

void aem_gc_free_default(struct aem_gc_object *obj, struct aem_gc_context *ctx)
{
	(void)ctx; // unused parameter

	free(obj);
}


void aem_gc_init(struct aem_gc_context *ctx)
{
	AEM_LL1_INIT(&ctx->objects, ctx_next);

	aem_iter_gen_init_master(&ctx->objects.iter);
}

void aem_gc_dtor(struct aem_gc_context *ctx)
{
	aem_gc_run(ctx);

	if (!AEM_LL_EMPTY(&ctx->objects, ctx_next)) {
		aem_logf_ctx(AEM_LOG_BUG, "Not all objects collected, leaking");
	}
}

void aem_gc_register(struct aem_gc_object *obj, const struct aem_gc_vtbl *vtbl, struct aem_gc_context *ctx)
{
	obj->vtbl = vtbl;

	aem_iter_gen_init(&obj->iter, &ctx->objects.iter);

	obj->refs = 1;

	AEM_LL1_INSERT_AFTER(&ctx->objects, obj, ctx_next);
}

void aem_gc_run(struct aem_gc_context *ctx)
{
	// Reset iterator master
	aem_iter_gen_reset_master(&ctx->objects.iter);

	// Mark all roots
	AEM_LL_FOR_ALL(curr, &ctx->objects, ctx_next) {
		if (curr->refs) {
			aem_gc_mark(curr, ctx);
		}
	}

	// Destruct all dead objects
	AEM_LL_FOR_ALL(curr, &ctx->objects, ctx_next) {
		// If it wasn't hit by the mark cycle, it's dead
		if (!aem_iter_gen_hit(&curr->iter, &ctx->objects.iter)) {
			aem_logf_ctx(AEM_LOG_DEBUG, "dtor %p(%s)", curr, curr->vtbl->name);
			if (curr->vtbl->dtor) {
				curr->vtbl->dtor(curr, ctx);
			}
		}
	}

	// Free all dead objects
	AEM_LL_FILTER_ALL(curr, &ctx->objects, ctx_next) {
		if (!aem_iter_gen_hit(&curr->iter, &ctx->objects.iter)) {

			if (curr->vtbl->free) {
				curr->vtbl->free(curr, ctx);
			}

			curr = NULL; // signal for curr to be removed
		}
	}
}

void aem_gc_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx)
{
	int id = aem_iter_gen_id(&obj->iter, &ctx->objects.iter);

	if (id < 0)
		return;

	if (obj->vtbl->mark) {
		obj->vtbl->mark(obj, ctx);
	}
}


void aem_gc_ref(struct aem_gc_object *obj)
{
	aem_assert(obj);
	obj->refs++;
}

void aem_gc_unref(struct aem_gc_object *obj)
{
	aem_assert(obj);
	aem_assert(obj->refs);
	obj->refs--;
}
