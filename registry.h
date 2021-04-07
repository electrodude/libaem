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

#define AEM_REGISTRY_FOREACH(i, T, e, N, r) \
	/* Don't evaluate h more than once */ \
	for (struct aem_registry *_r = (r); _r; _r = NULL) \
	/* Check each slot */ \
	for (size_t i = 0; i < _r->stk.n; i++) \
	/* Skip empty slots */ \
	for (struct aem_registrable *_e = _r->stk.s[i]; _e; _e = NULL) \
	/* Cast to desired type */ \
	for (T *e = aem_container_of(_e, T, N); (e); e = NULL)

/// Registerable
struct aem_registrable {
	struct aem_stringbuf name;
	struct aem_registry *reg;
	ssize_t id;
};

struct aem_registrable *aem_registrable_init(struct aem_registrable *item);
void aem_registrable_dtor(struct aem_registrable *item);

// Get object's name
const char *aem_registrable_name(struct aem_registrable *item);
// Get object's ID
ssize_t aem_registrable_id(struct aem_registrable *item);

ssize_t aem_registrable_register(struct aem_registrable *item, struct aem_registry *reg);
void aem_registrable_deregister(struct aem_registrable *item);


/// Lookup
// Get by ID
struct aem_registrable *aem_registry_by_id(struct aem_registry *reg, ssize_t id);
// Get by name
struct aem_registrable *aem_registry_by_name(struct aem_registry *reg, struct aem_stringslice name);
// Get by name or ID (prefix with # to prefer ID)
struct aem_registrable *aem_registry_lookup(struct aem_registry *reg, struct aem_stringslice key);
// Get by name, creating if not found
struct aem_registrable *aem_registry_get(struct aem_registry *reg, struct aem_stringslice name);

int aem_registry_remove(struct aem_registry *reg, size_t id);


/// Macros
#define AEM_REGISTRY_DECLARE_METHODS(T, name) \
	/* Get object's name */ \
	const char *name##_name(T *param); \
	/* Get object's ID */ \
	ssize_t name##_id(T *param); \
	/* Get object by ID */ \
	T *name##_by_id(ssize_t id); \
	/* Get object by name */ \
	T *name##_by_name(struct aem_stringslice name); \
	/* Get object by name or ID */ \
	T *name##_lookup(struct aem_stringslice name); \
	/* Get object by name, creating if not found */ \
	T *name##_get(struct aem_stringslice name);

#define AEM_REGISTRY_DEFINE_METHODS(T, name, reg, registry) \
	const char *name##_name(T *item) { return aem_registrable_name(item ? &item->reg : NULL); } \
	ssize_t name##_id(T *item) { return aem_registrable_id(item ? &item->reg : NULL); } \
	T *name##_by_id  (ssize_t id)                  { struct aem_registrable *item = aem_registry_by_id  (registry, id  ); if (!item) return NULL; return aem_container_of(item, T, reg); } \
	T *name##_by_name(struct aem_stringslice name) { struct aem_registrable *item = aem_registry_by_name(registry, name); if (!item) return NULL; return aem_container_of(item, T, reg); } \
	T *name##_lookup (struct aem_stringslice key ) { struct aem_registrable *item = aem_registry_lookup (registry, key ); if (!item) return NULL; return aem_container_of(item, T, reg); } \
	T *name##_get    (struct aem_stringslice name) { struct aem_registrable *item = aem_registry_get    (registry, name); if (!item) return NULL; return aem_container_of(item, T, reg); }

#endif /* AEM_REGISTRY_H */
