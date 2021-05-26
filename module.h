#ifndef AEM_MODULE_H
#define AEM_MODULE_H

#include <aem/log.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

struct aem_module;
struct aem_module_def {
	const char *name;
	const char *version;

	int (*reg)(struct aem_module *mod, struct aem_stringslice args);
	int (*dereg)(struct aem_module *mod);
	int (*check_dereg)(struct aem_module *mod);

	char singleton : 1;
};

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

	struct aem_module *mod_prev; // AEM_LL2
	struct aem_module *mod_next;

	struct aem_log_module *logmodule;

	void *userdata; // User-defined pointer to module-specific data

	enum aem_module_state state;
};

// This is mainly the root of the aem_module doubly-linked list, but it's also the
// owner of all built-in commands.
extern struct aem_module aem_modules;

extern struct aem_stringbuf aem_module_path;
void aem_module_path_set(const char *dir);

extern struct aem_log_module *aem_module_logmodule;

int aem_module_load(struct aem_stringslice name, struct aem_stringslice args, struct aem_module **mod_p);
int aem_modules_load(struct aem_stringslice *config);
int aem_module_unload(struct aem_module *mod);

void aem_module_identify(struct aem_stringbuf *out, struct aem_module *mod);
struct aem_module *aem_module_lookup(struct aem_stringslice name);

#endif /* AEM_MODULE_H */
