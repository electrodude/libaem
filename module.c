#include <dlfcn.h>

#define AEM_INTERNAL
#include <aem/linked_list.h>
#include <aem/pathutil.h>
#include <aem/translate.h>

#include "module.h"

int aem_module_disable_dereg(struct aem_module *mod)
{
	if (aem_log_header(&aem_log_buf, AEM_LOG_ERROR)) {
		aem_stringbuf_puts(&aem_log_buf, "Module ");
		aem_module_identify(&aem_log_buf, mod);
		aem_stringbuf_puts(&aem_log_buf, " can't be unloaded.\n");
		aem_log_str(&aem_log_buf);
	}
	return -1;
}

struct aem_stringbuf aem_module_path = {0};
struct aem_log_module *aem_module_logmodule = &aem_log_module_default;

struct aem_module aem_modules = {
	.mod_prev = &aem_modules, .mod_next = &aem_modules
};

void aem_module_path_set(const char *dir)
{
	aem_assert(dir);

	// If empty, aem_sandbox_path uses current working directory.
	aem_stringbuf_reset(&aem_module_path);
	aem_stringbuf_puts(&aem_module_path, dir);

	aem_logf_ctx(AEM_LOG_DEBUG, "Module path: %s", aem_stringbuf_get(&aem_module_path));
}

int aem_module_load(struct aem_stringslice name, struct aem_stringslice args, struct aem_module **mod_p)
{
	// Initialize module struct
	struct aem_module *mod = malloc(sizeof(*mod));
	if (!mod) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc() failed!");
		return 1;
	}
	aem_stringbuf_init(&mod->name);
	aem_stringbuf_init(&mod->path);
	mod->handle = NULL;
	mod->def = NULL;
	AEM_LL2_INIT(mod, mod);
	mod->logmodule = aem_module_logmodule;
	mod->userdata = NULL;
	mod->state = AEM_MODULE_UNREGISTERED;

	aem_stringbuf_putss(&mod->name, name);

	int rc = 0;

	// Determine absolute path to module file
	if (aem_sandbox_path(&mod->path, aem_stringslice_new_str(&aem_module_path), name, ".so")) {
		if (aem_log_header(&aem_log_buf, AEM_LOG_SECURITY)) {
			aem_stringbuf_puts(&aem_log_buf, "Invalid module name: ");
			aem_string_escape(&aem_log_buf, name);
			aem_stringbuf_puts(&aem_log_buf, "\n");
			aem_log_str(&aem_log_buf);
		}
		rc = -1;
		goto fail;
	}

	void *handle = dlopen(aem_stringbuf_get(&mod->path), RTLD_NOW);

	if (!handle) {
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to load module \"%s\": %s", aem_stringbuf_get(&mod->path), dlerror());
		rc = 1;
		goto fail;
	}
	mod->handle = handle;
	mod->def = dlsym(mod->handle, "aem_module_def");

	// Fill in default name
	if (mod->def && mod->def->name) {
		aem_stringbuf_reset(&mod->name);
		aem_stringbuf_puts(&mod->name, mod->def->name);
	}

	// Make sure module definition is sane
	const struct aem_module_def *def = mod->def;
	if (!def) {
		aem_logf_ctx(AEM_LOG_ERROR, "Couldn't find module definition for \"%s\": %s", aem_stringbuf_get(&mod->path), dlerror());
		rc = 1;
		goto fail;
	}

	if (!def->name) {
		aem_logf_ctx(AEM_LOG_ERROR, "Module %s has NULL name!", aem_stringbuf_get(&mod->name));
		rc = 1;
		goto fail;
	}

	if (!def->version) {
		aem_logf_ctx(AEM_LOG_WARN, "Module %s has NULL version!", aem_stringbuf_get(&mod->name));
	}

	if (def->singleton) {
		// Make sure we don't load a singleton module twice.
		AEM_LL_FOR_ALL(mod2, &aem_modules, mod_next) {
			if (mod->def == mod2->def) {
				aem_logf_ctx(AEM_LOG_ERROR, "Module \"%s\" already loaded!", def->name);
				rc = 1;
				goto fail;
			}
		}
	} else if (!def->reg) {
		// Singleton modules that are just function libraries don't need register methods.
		aem_logf_ctx(AEM_LOG_ERROR, "Non-singleton module \"%s\" has no register method!", aem_stringbuf_get(&mod->name));
		rc = 1;
		goto fail;
	}

	// Register module
	aem_logf_ctx(AEM_LOG_DEBUG, "Registering module \"%s\"", aem_stringbuf_get(&mod->name));

	if (def->reg && (rc = def->reg(mod, args))) {
		aem_logf_ctx(AEM_LOG_ERROR, "Error while registering module \"%s\": %d", aem_stringbuf_get(&mod->name), rc);
		goto fail;
	}

	AEM_LL2_INSERT_BEFORE(&aem_modules, mod, mod);

	mod->state = AEM_MODULE_REGISTERED;

	aem_logf_ctx(AEM_LOG_NOTICE, "Registered module \"%s\"", aem_stringbuf_get(&mod->name));

	if (mod_p)
		*mod_p = mod;

	return 0;

fail:
	aem_module_unload(mod);
	return rc;
}

int aem_modules_load(struct aem_stringslice *config)
{
	aem_assert(config);

	while (aem_stringslice_ok(*config)) {
		struct aem_stringslice line = aem_stringslice_match_line(config);

		// Ignore leading whitespace
		aem_stringslice_match_ws(&line);

		// Ignore empty lines
		if (!aem_stringslice_ok(line))
			continue;

		// Ignore comment lines beginning with "#"
		if (aem_stringslice_match(&line, "#"))
			continue;

		struct aem_stringslice name = aem_stringslice_match_word(&line);
		line = aem_stringslice_trim(line);

		if (!aem_stringslice_ok(name)) {
			aem_logf_ctx(AEM_LOG_ERROR, "Invalid syntax: expected module name");
			return -1;
		}

		int rc = aem_module_load(name, line, NULL);

		if (rc)
			return rc;
	}

	return 0;
}

int aem_module_unload(struct aem_module *mod)
{
	if (!mod)
		return -1;

	if (mod->state == AEM_MODULE_REGISTERED) {
		const struct aem_module_def *def = mod->def;
		aem_assert(def);
		if (def->check_dereg) {
			int rc = def->check_dereg(mod);
			if (rc)
				return rc;
		}
	}

	aem_logf_ctx(AEM_LOG_NOTICE, "Unloading module \"%s\"", aem_stringbuf_get(&mod->name));
	AEM_LL2_REMOVE(mod, mod);

	if (mod->state == AEM_MODULE_REGISTERED) {
		const struct aem_module_def *def = mod->def;
		aem_assert(def);
		if (def->dereg) {
			aem_logf_ctx(AEM_LOG_DEBUG, "Deregistering module \"%s\"", aem_stringbuf_get(&mod->name));
			int rc = def->dereg(mod);
			if (!rc) {
				aem_logf_ctx(AEM_LOG_DEBUG, "Deregistered module \"%s\"", aem_stringbuf_get(&mod->name));
			} else {
				aem_logf_ctx(AEM_LOG_ERROR, "Error while deregistering module \"%s\": %d", aem_stringbuf_get(&mod->name), rc);
				return rc;
			}
		} else {
			aem_logf_ctx(AEM_LOG_DEBUG2, "Module \"%s\" doesn't need to deregister anything.", aem_stringbuf_get(&mod->name));
		}
		mod->state = AEM_MODULE_UNREGISTERED;
	}

	if (mod->handle) {
		if (dlclose(mod->handle)) {
			aem_logf_ctx(AEM_LOG_ERROR, "Failed to dlclose() module \"%s\": %s", aem_stringbuf_get(&mod->name), dlerror());
			return -1;
		}
	}

	aem_stringbuf_dtor(&mod->name);
	aem_stringbuf_dtor(&mod->path);
	free(mod);

	return 0;
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

struct aem_module *aem_module_lookup(struct aem_stringslice name)
{
	// First, search module names
	AEM_LL_FOR_ALL(mod, &aem_modules, mod_next) {
		aem_assert(mod->def);
		if (aem_stringslice_eq(name, aem_stringbuf_get(&mod->name))) {
			return mod;
		}
	}
	// Second, search module definition names
	AEM_LL_FOR_ALL(mod, &aem_modules, mod_next) {
		aem_assert(mod->def);
		if (aem_stringslice_eq(name, mod->def->name)) {
			return mod;
		}
	}

	return NULL;
}
