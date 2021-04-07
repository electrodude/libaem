#include "registry.h"

struct aem_registry *aem_registry_init(struct aem_registry *reg)
{
	aem_stack_init(&reg->stk);
	reg->on_get_miss = NULL;
	reg->dtor = NULL;
	reg->flags = 0;

	return reg;
}
void aem_registry_dtor(struct aem_registry *reg)
{
	if (!reg)
		return;

	AEM_STACK_FOREACH(i, &reg->stk) {
		aem_registry_remove(reg, i);
	}

	aem_stack_dtor(&reg->stk);
}

struct aem_registrable *aem_registrable_init(struct aem_registrable *item)
{
	aem_stringbuf_init(&item->name);
	item->reg = NULL;
	item->id = -1;

	return item;
}
void aem_registrable_dtor(struct aem_registrable *item)
{
	if (!item)
		return;

	if (item->id >= 0)
		aem_registrable_deregister(item);

	aem_assert(!item->reg);

	aem_stringbuf_dtor(&item->name);
}

const char *aem_registrable_name(struct aem_registrable *item)
{
	if (!item)
		return NULL;

	return aem_stringbuf_get(&item->name);
}
ssize_t aem_registrable_id(struct aem_registrable *item)
{
	if (!item)
		return -1;

	return item->id;
}

ssize_t aem_registrable_register(struct aem_registrable *item, struct aem_registry *reg)
{
	// Bail if already registered
	if (item->id >= 0)
		return item->id;

	if (reg->flags & AEM_REGISTRY_NO_DUPS) {
		struct aem_registrable *item2 = aem_registry_by_name(reg, aem_stringslice_new_str(&item->name));
		if (item2) {
			//aem_logf_ctx(AEM_LOG_BUG, "Duplicate registration name: \"%s\" already taken by #%zd", aem_registrable_name(item), aem_registrable_id(item2));
			return -1;
		}
	}

	//aem_assert(item->name.n);

	aem_assert(reg);
	item->reg = reg;

	aem_assert(item->id == -1);
	size_t id = aem_stack_assign_empty(&item->reg->stk, item);
	item->id = id;

	return id;
}
void aem_registrable_deregister(struct aem_registrable *item)
{
	if (!item)
		return;

	// Bail if not registered
	if (item->id == -1)
		return;

	aem_assert(item->id >= 0);
	aem_assert(item->reg);

	// Deregister the item, and verify we didn't somehow deregister the wrong one.
	aem_assert(aem_stack_remove(&item->reg->stk, item->id) == item);
	item->id = -1;

	item->reg = NULL;
}


struct aem_registrable *aem_registry_by_id(struct aem_registry *reg, ssize_t id)
{
	if (!reg)
		return NULL;

	if (id < 0)
		return NULL;

	struct aem_registrable *item = aem_stack_index(&reg->stk, id);

	return item;
}
struct aem_registrable *aem_registry_by_name(struct aem_registry *reg, struct aem_stringslice name)
{
	if (!reg)
		return NULL;

	// TODO: Use a hash table
	AEM_STACK_FOREACH(i, &reg->stk) {
		// TODO: struct aem_registrable *item = reg->stk.s[(i + hash(name)) % reg->stk.n];
		struct aem_registrable *item = reg->stk.s[i];
		if (!item)
			continue;

		if (!aem_stringslice_cmp(aem_stringslice_new_str(&item->name), name))
			return item;
	}

	return NULL;
}
struct aem_registrable *aem_registry_lookup(struct aem_registry *reg, struct aem_stringslice key)
{
	if (!reg)
		return NULL;

	// Try to look up by key
	struct aem_registrable *by_name = aem_registry_by_name(reg, key);

	// Get a '#' if there is one
	int prefer_id = aem_stringslice_match(&key, "#");

	// If we found one by key that didn't start with "#", return it now
	if (!prefer_id && by_name)
		return by_name;

	// Try to look up by ID
	int id;
	struct aem_registrable *by_id = aem_stringslice_match_int_base(&key, 10, &id) && !aem_stringslice_ok(key) ? aem_registry_by_id(reg, id) : NULL;

	// If we couldn't find one by ID, return the one we found by key
	if (!by_id)
		return by_name;

	return by_id;
}

struct aem_registrable *aem_registry_get(struct aem_registry *reg, struct aem_stringslice name)
{
	if (!reg)
		return NULL;

	struct aem_registrable *item = aem_registry_by_name(reg, name);

	// If we found nothing but have an on_get_miss callback, see if it can get us something.
	if (!item && reg->on_get_miss) {
		item = reg->on_get_miss(reg, name);
		// If it returned something, assert that it is registered with this registry.
		if (item)
			aem_assert(item->reg == reg);
	}

	return item;
}

int aem_registry_remove(struct aem_registry *reg, size_t id)
{
	aem_assert(reg);

	struct aem_registrable *item = aem_registry_by_id(reg, id);

	if (!item)
		return -1;

	if (reg->dtor) {
		reg->dtor(item);
		return 0;
	} else {
		aem_logf_ctx(AEM_LOG_BUG, "Removing item %s (#%zd) from registry with no ->dtor method!", aem_registrable_name(item), aem_registrable_id(item));
		aem_registrable_deregister(item);
	}

	return 1;
}
