#include <stdlib.h>

#include "log.h"

#include "gc.h"

void aem_gc_free_default(struct aem_gc_object *obj, struct aem_gc_context *ctx)
{
	(void)ctx; // unused parameter

	free(obj);
}


void aem_gc_init(struct aem_gc_context *ctx)
{
	ctx->objects.ctx_next = NULL;

	aem_iter_gen_init_master(&ctx->objects.iter);
}

void aem_gc_dtor(struct aem_gc_context *ctx)
{
	aem_gc_run(ctx);

	if (ctx->objects.ctx_next != NULL)
	{
		aem_logf_ctx(AEM_LOG_BUG, "not all objects collected, leaking\n");
	}
}

void aem_gc_register(struct aem_gc_object *obj, struct aem_gc_vtbl *vtbl, struct aem_gc_context *ctx)
{
	obj->vtbl = vtbl;

	obj->ctx_next = ctx->objects.ctx_next;

	aem_iter_gen_init(&obj->iter, &ctx->objects.iter);

	obj->root = 0;

	ctx->objects.ctx_next = obj;
}

void aem_gc_run(struct aem_gc_context *ctx)
{
	// reset iterator master
	aem_iter_gen_reset_master(&ctx->objects.iter);

	// mark all roots
	for (struct aem_gc_object *curr = ctx->objects.ctx_next; curr != NULL; curr = curr->ctx_next)
	{
		if (curr->root)
		{
			aem_gc_mark(curr, ctx);
		}
	}

	// destruct all dead objects
	for (struct aem_gc_object *curr = ctx->objects.ctx_next; curr != NULL; curr = curr->ctx_next)
	{
		// if it wasn't hit by the mark cycle, it's dead
		if (!aem_iter_gen_hit(&curr->iter, &ctx->objects.iter))
		{
			aem_logf_ctx(AEM_LOG_DEBUG, "dtor %p(%s)\n", curr, curr->vtbl->name);
			if (curr->vtbl->dtor != NULL)
			{
				curr->vtbl->dtor(curr, ctx);
			}
		}
	}

	// free all dead objects
	for (struct aem_gc_object *prev = &ctx->objects; prev->ctx_next != NULL; /* increment is conditional at end of loop */)
	{
		struct aem_gc_object *curr = prev->ctx_next;
		if (!aem_iter_gen_hit(&curr->iter, &ctx->objects.iter))
		{
			struct aem_gc_object *next = curr->ctx_next;
			prev->ctx_next = next;

			if (curr->vtbl->free != NULL)
			{
				curr->vtbl->free(curr, ctx);
			}

			// don't `prev = prev->ctx_next;`, since
			// `prev->ctx_next` changed and its new value needs to
			// be processed
		}
		else
		{
			prev = prev->ctx_next;
		}
	}
}

void aem_gc_mark(struct aem_gc_object *obj, struct aem_gc_context *ctx)
{
	int id = aem_iter_gen_id(&obj->iter, &ctx->objects.iter);

	if (id < 0) return;

	if (obj->vtbl->mark != NULL)
	{
		obj->vtbl->mark(obj, ctx);
	}
}
