#include <stdlib.h>

#include "log.h"
#include "linked_list.h"

#include "gc.h"

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

	if (!AEM_LL_EMPTY(&ctx->objects, ctx_next))
	{
		aem_logf_ctx(AEM_LOG_BUG, "not all objects collected, leaking\n");
	}
}

void aem_gc_register(struct aem_gc_object *obj, const struct aem_gc_vtbl *vtbl, struct aem_gc_context *ctx)
{
	obj->vtbl = vtbl;

	aem_iter_gen_init(&obj->iter, &ctx->objects.iter);

	obj->root = 0;

	AEM_LL1_INSERT_AFTER(&ctx->objects, obj, ctx_next);
}

void aem_gc_run(struct aem_gc_context *ctx)
{
	// reset iterator master
	aem_iter_gen_reset_master(&ctx->objects.iter);

	// mark all roots
	AEM_LL2_FOR_ALL(curr, &ctx->objects, _, ctx_next)
	{
		if (curr->root)
		{
			aem_gc_mark(curr, ctx);
		}
	}

	// destruct all dead objects
	AEM_LL2_FOR_ALL(curr, &ctx->objects, _, ctx_next)
	{
		// if it wasn't hit by the mark cycle, it's dead
		if (!aem_iter_gen_hit(&curr->iter, &ctx->objects.iter))
		{
			aem_logf_ctx(AEM_LOG_DEBUG, "dtor %p(%s)\n", curr, curr->vtbl->name);
			if (curr->vtbl->dtor)
			{
				curr->vtbl->dtor(curr, ctx);
			}
		}
	}

	// free all dead objects
	AEM_LL_FILTER_ALL(curr, &ctx->objects, ctx_next)
	{
		if (!aem_iter_gen_hit(&curr->iter, &ctx->objects.iter))
		{

			if (curr->vtbl->free)
			{
				curr->vtbl->free(curr, ctx);
			}

			curr = NULL; // signal for curr to be removed
		}
	}
}

void aem_gc_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx)
{
	int id = aem_iter_gen_id(&obj->iter, &ctx->objects.iter);

	if (id < 0) return;

	if (obj->vtbl->mark)
	{
		obj->vtbl->mark(obj, ctx);
	}
}
