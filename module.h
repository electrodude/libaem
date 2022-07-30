#ifndef AEM_MODULE_H
#define AEM_MODULE_H

#include <aem/log.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

struct aem_module;
struct aem_module_def {
	const char *name;
	const char *version;

	// Called after module is loaded
	int (*reg)(struct aem_module *mod, struct aem_stringslice args);
	// Called before module is unloaded
	void (*dereg)(struct aem_module *mod);

	// Called before a module is registered to determine whether it can be
	// safely loaded.  Return 1 if OK, else 0.  NULL => OK
	int (*check_reg)(struct aem_module *mod);
	// Called before a module is deregistered to determine whether it can
	// be safely unloaded.  Return 1 if OK, else 0.  NULL => OK
	int (*check_dereg)(struct aem_module *mod);

	struct aem_log_module *logmodule;
};

// Set def->check_reg to this for modules that should never have more than one
// instance loaded at a time.
// You must define this yourself to return 0 if a module with the same mod->def
// is already loaded.
extern int aem_module_check_reg_singleton(struct aem_module *mod);
// Set def->check_dereg to this for modules that should never be unloaded
int aem_module_disable_dereg(struct aem_module *mod);

enum aem_module_state {
	AEM_MODULE_UNREGISTERED,
	AEM_MODULE_REGISTERED,
};
struct aem_module {
	struct aem_stringbuf name;
	struct aem_stringbuf path;

	void *handle;
	const struct aem_module_def *def;

	struct aem_log_module *logmodule;

	enum aem_module_state state;
};

extern struct aem_stringbuf aem_module_path;
void aem_module_path_set(const char *dir);

extern struct aem_log_module *aem_module_logmodule;

void aem_module_init(struct aem_module *mod);
// Calls aem_module_unload and then frees memory
void aem_module_dtor(struct aem_module *mod);

int aem_module_resolve_path(struct aem_module *mod);
// Load a module.  Call after calling aem_module_init and setting either the
// name or path fields.  Returns non-zero on error, in which case you must
// destroy the module yourself via aem_module_unload or aem_module_dtor.
int aem_module_load(struct aem_module *mod, struct aem_stringslice args);
// Returns 1 if a module can be unloaded, else 0
int aem_module_unload_check(struct aem_module *mod);
// Unload a module, even if only partially loaded.  Idempotent.
// You should normally call aem_module_unload_check on a fully-loaded module
// before unloading it via this function.
int aem_module_unload(struct aem_module *mod);

void *aem_module_get_sym(struct aem_module *mod, const char *symbol);
void aem_module_identify(struct aem_stringbuf *out, struct aem_module *mod);

#endif /* AEM_MODULE_H */
