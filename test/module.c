#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

#include <aem/linked_list.h>
#include <aem/module.h>

static void test_module_load(const char *name, const char *args, int rc_expect, struct aem_module **mod_p)
{
	aem_logf_ctx(AEM_LOG_INFO, "module_load(\"%s\", \"%s\") expect (%d)", name, args, rc_expect);

	struct aem_stringslice args_ss = aem_stringslice_new_cstr(args);
	struct aem_module *mod = malloc(sizeof(*mod));
	aem_assert(mod);
	aem_module_init(mod);
	aem_stringbuf_puts(&mod->name, name);
	int rc = aem_module_load(mod, args_ss);

	if (mod_p)
		*mod_p = mod;

	if (rc != rc_expect) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "module_load(\"%s\", \"%s\") returned (%d), expected (%d)!", name, args, rc, rc_expect);
	}

	if (rc) {
		aem_module_unload(mod);
		aem_module_dtor(mod);
		free(mod);
	}
}

static void test_modules_load(const char *spec, const char *remain, int rc_expect)
{
	aem_logf_ctx(AEM_LOG_INFO, "modules_load(\"%s\") expect (%d)", spec, rc_expect);

	// TODO: Testcase invariant: remain is suffix of spec
	// TODO: Testcase invariant: remain not empty => rc_expect not zero

	struct aem_stringslice spec_ss = aem_stringslice_new_cstr(spec);

	struct aem_stringslice spec_ret = spec_ss;
	int rc = aem_modules_load(&spec_ret);

	if (rc != rc_expect) {
		test_errors++;
		if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
			aem_stringbuf_puts(&aem_log_buf, "modules_load(");
			debug_slice(&aem_log_buf, spec_ss);
			aem_stringbuf_printf(&aem_log_buf, ") returned (%d, ", rc);
			debug_slice(&aem_log_buf, spec_ret);
			aem_stringbuf_printf(&aem_log_buf, "), expected (%d, \"%s\")\n", rc_expect, remain);
			aem_log_str(&aem_log_buf);
		}
	}
}

static void test_module_unload(const char *name, int rc_expect)
{
	aem_logf_ctx(AEM_LOG_INFO, "module_unload(\"%s\") expect (%d)", name, rc_expect);

	struct aem_stringslice name_ss = aem_stringslice_new_cstr(name);
	struct aem_module *mod = aem_module_lookup(name_ss);
	int rc = aem_module_unload(mod);

	if (rc != rc_expect) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "module_unload(\"%s\") returned (%d), expected (%d)!", name, rc, rc_expect);
	}

	if (mod) {
		aem_module_dtor(mod);
		free(mod);
	}
}

int main(int argc, char **argv)
{
	aem_log_stderr();
	aem_log_module_default.loglevel = AEM_LOG_FATAL;
	aem_log_module_default_internal.loglevel = AEM_LOG_FATAL;

	aem_logf_ctx(AEM_LOG_NOTICE, "test module");

	aem_module_path_set("lib");

	struct aem_module *mod_test_0, *mod_test_1, *mod_singleton;

	test_module_load("../module_illegal"    , "" , -1, NULL);
	test_module_load("module_noent"         , "" , -1, NULL);
	test_module_load("module_empty"         , "" , -1, NULL);
	test_module_load("module_invalid"       , "" , -1, NULL);
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

	if (aem_log_header(&aem_log_buf, AEM_LOG_NOTICE)) {
		aem_stringbuf_puts(&aem_log_buf, "\033[95;1mCurrently loaded modules:\033[0;1m\n");
		AEM_LL_FOR_ALL(mod, &aem_modules, mod_next) {
			aem_assert(mod->def);
			aem_stringbuf_puts(&aem_log_buf, "\tModule ");
			aem_module_identify(&aem_log_buf, mod);
			aem_stringbuf_printf(&aem_log_buf, ", path ");
			aem_stringbuf_append(&aem_log_buf, &mod->path);
			aem_stringbuf_puts(&aem_log_buf, "\n");
		}
			aem_log_str(&aem_log_buf);
	}

	test_module_unload("module_test_singleton",  0);
	test_module_unload("module_test_singleton", -1);
	test_module_unload("module_test-0"        ,  0);
	test_module_unload("module_test-1"        ,  0);
	test_module_unload("module_test"          ,  0);
	test_module_unload("module_test-2"        ,  0);
	test_module_unload("module_test-3"        , -1);
	test_module_unload("module_empty"         , -1);
	test_module_unload("module_invalid"       , -1);

	aem_stringbuf_dtor(&aem_module_path);
	aem_stringbuf_shrinkwrap(&aem_log_buf);

	return show_test_results();
}
