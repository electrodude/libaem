#include <dlfcn.h>

#define AEM_INTERNAL
#include <aem/pathutil.h>
#include <aem/translate.h>

#include "module.h"

int aem_module_disable_dereg(struct aem_module *mod)
{
	(void)mod;
	return 0;
}

struct aem_stringbuf aem_module_path = {0};
struct aem_log_module *aem_module_logmodule = &aem_log_module_default;

void aem_module_path_set(const char *dir)
{
	aem_assert(dir);

	// If empty, aem_sandbox_path uses current working directory.
	aem_stringbuf_reset(&aem_module_path);
	aem_stringbuf_puts(&aem_module_path, dir);

	aem_logf_ctx(AEM_LOG_DEBUG, "Module path: %s", aem_stringbuf_get(&aem_module_path));
}

void aem_module_init(struct aem_module *mod)
{
	aem_assert(mod);

	aem_stringbuf_init(&mod->name);
	aem_stringbuf_init(&mod->path);
	mod->handle = NULL;
	mod->def = NULL;
	mod->logmodule = aem_module_logmodule;
	mod->state = AEM_MODULE_UNREGISTERED;
}
void aem_module_dtor(struct aem_module *mod)
{
	aem_assert(mod);

	// TODO: And if this returns <0?
	aem_module_unload(mod);

	aem_stringbuf_dtor(&mod->name);
	aem_stringbuf_dtor(&mod->path);
}

int aem_module_resolve_path(struct aem_module *mod)
{
	aem_assert(mod);

	struct aem_stringslice name = aem_stringslice_new_str(&mod->name);

	// Determine path to module file
	aem_stringbuf_reset(&mod->path);
	if (aem_sandbox_path(&mod->path, aem_stringslice_new_str(&aem_module_path), name, ".so")) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Invalid module name: ");
			aem_string_escape(out, name);
		}
		return -1;
	}

	return 0;
}
int aem_module_open(struct aem_module *mod)
{
	aem_assert(mod);

	if (!mod->path.n) {
		aem_logf_ctx(AEM_LOG_BUG, "Module path not set!  Set mod->path yourself, or set mod->name and then call aem_module_resolve_path ");
		return -1;
	}

	// Load module
	void *handle = dlopen(aem_stringbuf_get(&mod->path), RTLD_NOW | RTLD_GLOBAL);
	if (!handle) {
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to load module \"%s\": %s", aem_stringbuf_get(&mod->path), dlerror());
		return -1;
	}

	mod->handle = handle;
	mod->def = aem_module_get_sym(mod, "aem_module_def");

	// Fill in default name
	if (mod->def && mod->def->name) {
		aem_stringbuf_reset(&mod->name);
		aem_stringbuf_puts(&mod->name, mod->def->name);
	}

	// Make sure module definition is sane
	const struct aem_module_def *def = mod->def;
	if (!def) {
		aem_logf_ctx(AEM_LOG_ERROR, "Couldn't find module definition for \"%s\": %s", aem_stringbuf_get(&mod->path), dlerror());
		return -1;
	}

	if (!def->name) {
		aem_logf_ctx(AEM_LOG_ERROR, "Module %s has NULL name!", aem_stringbuf_get(&mod->name));
		return -1;
	}

	if (!def->version) {
		aem_logf_ctx(AEM_LOG_WARN, "Module %s has NULL version!", aem_stringbuf_get(&mod->name));
	}

	if (def->check_reg) {
		int rc = def->check_reg(mod);
		if (!rc) {
			return -1;
		}
	}

	mod->state = AEM_MODULE_LOADED;

	return 0;
}

int aem_module_register(struct aem_module *mod, struct aem_stringslice args)
{
	aem_assert(mod);
	const struct aem_module_def *def = mod->def;
	aem_assert(def);

	aem_assert(mod->state == AEM_MODULE_LOADED);

	// Register module
	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		aem_stringbuf_puts(out, "Registering module ");
		aem_module_identify(out, mod);
	}

	int rc = 0;
	if (def->reg && (rc = def->reg(mod, args))) {
		aem_logf_ctx(AEM_LOG_ERROR, "Error %d while registering module \"%s\"", rc, aem_stringbuf_get(&mod->name));
		return rc;
	}

	mod->state = AEM_MODULE_REGISTERED;

	AEM_LOG_MULTI(out, AEM_LOG_NOTICE) {
		aem_stringbuf_puts(out, "Registered module ");
		aem_module_identify(out, mod);
	}

	return 0;
}

int aem_module_unload_check(struct aem_module *mod)
{
	if (!mod)
		return -1;

	const struct aem_module_def *def = mod->def;
	if (!def)
		return -1;

	if (!def->check_dereg)
		return 1;

	return def->check_dereg(mod);
}
int aem_module_unload(struct aem_module *mod)
{
	if (!mod)
		return -1;

	if (mod->state == AEM_MODULE_REGISTERED) {
		const struct aem_module_def *def = mod->def;
		aem_assert(def);
		aem_logf_ctx(AEM_LOG_DEBUG, "Deregistering module \"%s\"", aem_stringbuf_get(&mod->name));
		if (def->dereg) {
			def->dereg(mod);
			aem_logf_ctx(AEM_LOG_DEBUG, "Deregistered module \"%s\"", aem_stringbuf_get(&mod->name));
		} else {
			aem_logf_ctx(AEM_LOG_DEBUG2, "Module \"%s\" didn't need to deregister anything.", aem_stringbuf_get(&mod->name));
		}
		mod->state = AEM_MODULE_UNREGISTERED;
	}
	mod->def = NULL;

	if (mod->handle) {
		aem_logf_ctx(AEM_LOG_NOTICE, "Unloading module \"%s\"", aem_stringbuf_get(&mod->name));
		if (dlclose(mod->handle)) {
			aem_logf_ctx(AEM_LOG_ERROR, "Failed to dlclose() module \"%s\": %s", aem_stringbuf_get(&mod->name), dlerror());
			// TODO: Clear mod->handle?
			return -1;
		}
	}
	mod->handle = NULL;

	return 0;
}

void *aem_module_get_sym(struct aem_module *mod, const char *symbol)
{
	if (!mod->handle)
		return NULL;

	return dlsym(mod->handle, symbol);
}

void aem_module_identify(struct aem_stringbuf *out, struct aem_module *mod)
{
	aem_assert(out);

	if (!mod) {
		aem_stringbuf_puts(out, "(null module)");
		return;
	}

	aem_stringbuf_putc(out, '"');
	aem_stringbuf_append(out, &mod->name);
	aem_stringbuf_putc(out, '"');

	const struct aem_module_def *def = mod->def;
	if (def) {
		aem_stringbuf_puts(out, " (");
		aem_stringbuf_puts(out, def->name);
		if (def->version) {
			aem_stringbuf_puts(out, "-");
			aem_stringbuf_puts(out, def->version);
		}
		aem_stringbuf_puts(out, ")");
	} else {
		aem_stringbuf_puts(out, " (null def)");
	}
}
