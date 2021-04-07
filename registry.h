#ifndef AEM_REGISTRY_H
#define AEM_REGISTRY_H

#include <sys/types.h>

#include <aem/stack.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

#define AEM_REGISTRY_NO_DUPS 0x1

/// Registry
struct aem_registrable;
struct aem_registry {
	struct aem_stack stk;

	struct aem_registrable *(*on_get_miss)(struct aem_registry *reg, struct aem_stringslice name);
	void (*dtor)(struct aem_registrable *item);
	int flags;
};

struct aem_registry *aem_registry_init(struct aem_registry *reg);
void aem_registry_dtor(struct aem_registry *reg);


/// Registerable
struct aem_registrable {
	struct aem_stringbuf name;
	struct aem_registry *reg;
	ssize_t id;
};

struct aem_registrable *aem_registrable_init(struct aem_registrable *item);
void aem_registrable_dtor(struct aem_registrable *item);
const char *aem_registrable_name(struct aem_registrable *item);
ssize_t aem_registrable_id(struct aem_registrable *item);

ssize_t aem_registrable_register(struct aem_registrable *item, struct aem_registry *reg);
void aem_registrable_deregister(struct aem_registrable *item);


/// Lookup
struct aem_registrable *aem_registry_by_id(struct aem_registry *reg, ssize_t id);
struct aem_registrable *aem_registry_by_name(struct aem_registry *reg, struct aem_stringslice name);
struct aem_registrable *aem_registry_lookup(struct aem_registry *reg, struct aem_stringslice key);

struct aem_registrable *aem_registry_get(struct aem_registry *reg, struct aem_stringslice name);
int aem_registry_remove(struct aem_registry *reg, size_t id);

#endif /* AEM_REGISTRY_H */
