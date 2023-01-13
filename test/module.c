#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

#include <aem/linked_list.h>
#include <aem/module.h>

struct module {
	struct aem_module mod;

	struct module *mod_prev; // AEM_LL2
	struct module *mod_next;
};

struct module modules = {
	.mod_prev = &modules, .mod_next = &modules,
};

struct module *module_load(struct aem_stringslice name, struct aem_stringslice args, int *rc_p)
{
	struct module *mod = malloc(sizeof(*mod));
	aem_assert(mod);
	aem_module_init(&mod->mod);
	AEM_LL2_INIT(mod, mod);

	aem_stringbuf_putss(&mod->mod.name, name);

	int rc = aem_module_resolve_path(&mod->mod);
	if (rc)
		goto fail;

	rc = aem_module_open(&mod->mod);
	if (rc)
		goto fail;

	rc = aem_module_register(&mod->mod, args);
	if (rc)
		goto fail;

fail:
	if (rc_p)
		*rc_p = rc;

	if (rc) {
		AEM_LL2_REMOVE(mod, mod);

		aem_module_unload(&mod->mod);
		aem_module_dtor(&mod->mod);
		free(mod);
		return NULL;
	}

	AEM_LL2_INSERT_BEFORE(&modules, mod, mod);

	return mod;
}

int module_unload(struct module *mod)
{
	if (!mod)
		return -1;

	if (!aem_module_unload_check(&mod->mod)) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Module ");
			aem_module_identify(out, &mod->mod);
			aem_stringbuf_puts(out, " can't be unloaded.");
		}
		return -1;
	}

	AEM_LL2_REMOVE(mod, mod);

	int rc = aem_module_unload(&mod->mod);
	if (rc)
		return rc;

	aem_module_dtor(&mod->mod);
	free(mod);

	return rc;
}

struct module *module_lookup(struct aem_stringslice name)
{
	// First, search module names
	AEM_LL_FOR_ALL(mod, &modules, mod_next) {
		if (aem_stringslice_eq(name, aem_stringbuf_get(&mod->mod.name)))
			return mod;
	}
	// Second, search module definition names
	AEM_LL_FOR_ALL(mod, &modules, mod_next) {
		aem_assert(mod->mod.def);
		if (aem_stringslice_eq(name, mod->mod.def->name))
			return mod;
	}

	return NULL;
}

int aem_module_check_reg_singleton(struct aem_module *mod)
{
	aem_assert(mod);
	AEM_LL_FOR_ALL(mod2, &modules, mod_next) {
		if (mod2->mod.def == mod->def)
			return 0;
	}

	return 1;
}

int modules_load(struct aem_stringslice *config)
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

		int rc;
		struct module *mod = module_load(name, line, &rc);

		if (rc) {
			AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
				aem_stringbuf_puts(out, "Failed to load module ");
				aem_stringbuf_putss(out, name);
				aem_stringbuf_printf(out, ": %d", rc);
			}
			return rc;
		}
	}

	return 0;
}

static void test_module_load(const char *name, const char *args, int rc_expect, struct module **mod_p)
{
	aem_logf_ctx(AEM_LOG_INFO, "module_load(\"%s\", \"%s\") expect (%d)", name, args, rc_expect);

	struct aem_stringslice args_ss = aem_stringslice_new_cstr(args);
	struct aem_stringslice name_ss = aem_stringslice_new_cstr(name);
	int rc;
	struct module *mod = module_load(name_ss, args_ss, &rc);

	if (mod_p)
		*mod_p = mod;

	if (rc != rc_expect) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "module_load(\"%s\", \"%s\") returned (%d), expected (%d)!", name, args, rc, rc_expect);
	}
}

static void test_modules_load(const char *spec, const char *remain, int rc_expect)
{
	aem_logf_ctx(AEM_LOG_INFO, "modules_load(\"%s\") expect (%d)", spec, rc_expect);

	// TODO: Testcase invariant: remain is suffix of spec
	// TODO: Testcase invariant: remain not empty => rc_expect not zero

	struct aem_stringslice spec_ss = aem_stringslice_new_cstr(spec);

	struct aem_stringslice spec_ret = spec_ss;
	int rc = modules_load(&spec_ret);

	if (rc != rc_expect) {
		test_errors++;
		AEM_LOG_MULTI(out, AEM_LOG_BUG) {
			aem_stringbuf_puts(out, "modules_load(");
			debug_slice(out, spec_ss);
			aem_stringbuf_printf(out, ") returned (%d, ", rc);
			debug_slice(out, spec_ret);
			aem_stringbuf_printf(out, "), expected (%d, \"%s\")", rc_expect, remain);
		}
	}
}

static void test_module_unload(const char *name, int rc_expect)
{
	aem_logf_ctx(AEM_LOG_INFO, "module_unload(\"%s\") expect (%d)", name, rc_expect);

	struct aem_stringslice name_ss = aem_stringslice_new_cstr(name);
	struct module *mod = module_lookup(name_ss);
	int rc = module_unload(mod);

	if (rc != rc_expect) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "module_unload(\"%s\") returned (%d), expected (%d)!", name, rc, rc_expect);
	}
}

int main(int argc, char **argv)
{
	aem_log_module_default.loglevel = AEM_LOG_FATAL;
	aem_log_module_default_internal.loglevel = AEM_LOG_FATAL;

	test_init(argc, argv);

	struct aem_log_module logmodule_modules = {.loglevel = AEM_LOG_DEBUG};
	aem_module_logmodule = &logmodule_modules;

	aem_logf_ctx(AEM_LOG_NOTICE, "test module");

	aem_module_path_set("lib");

	struct module *mod_test_0, *mod_test_1, *mod_singleton;

	test_module_load("../module_illegal"    , "" , -1, NULL);
	test_module_load("module_noent"         , "" , -1, NULL);
	test_module_load("module_empty"         , "" , -1, NULL);
	test_module_load("module_failreg"       , "" , -9, NULL);
	test_module_load("module_test"          , "0",  0, NULL);
	test_module_unload("module_test-0"        ,  0);
	test_module_load("./module_test"        , "0",  0, &mod_test_0);
	test_module_load("../lib/module_test"   , "S", -1, NULL);
	test_modules_load("module_test 1", "", 0);
	test_module_load("module_test_singleton", "" ,  0, &mod_singleton);
	test_module_load("module_test"          , "1",  0, &mod_test_1);
	test_modules_load("module_test 2\nmodule_test_singleton\nmodule_test 3", "module_test 3", -1);
	test_module_load("module_test_singleton", "" , -1, NULL);

	AEM_LOG_MULTI(out, AEM_LOG_NOTICE) {
		aem_stringbuf_puts(out, "\033[95;1mCurrently loaded modules:\033[0;1m\n");
		AEM_LL_FOR_ALL(mod, &modules, mod_next) {
			aem_assert(mod->mod.def);
			aem_stringbuf_puts(out, "\tModule ");
			aem_module_identify(out, &mod->mod);
			aem_stringbuf_printf(out, ", path ");
			aem_stringbuf_append(out, &mod->mod.path);
			aem_stringbuf_puts(out, "\n");
		}
		out->n--; // Remove last \n
		aem_stringbuf_puts(out, "\033[0;m");
	}

	test_module_unload("module_test_singleton",  0);
	test_module_unload("module_test_singleton", -1);
	test_module_unload("module_test-0"        ,  0);
	test_module_unload("module_test-1"        ,  0);
	test_module_unload("module_test"          ,  0);
	test_module_unload("module_test-2"        ,  0);
	test_module_unload("module_test-3"        , -1);
	test_module_unload("module_illegal"       , -1);
	test_module_unload("module_noent"         , -1);
	test_module_unload("module_empty"         , -1);
	test_module_unload("module_failreg"       , -1);

	aem_stringbuf_dtor(&aem_module_path);

	return show_test_results();
}
