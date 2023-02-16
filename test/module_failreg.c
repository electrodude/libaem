#include <aem/module.h>

#undef aem_log_module_current
#define aem_log_module_current (&module_failreg_log_module)
struct aem_log_module module_failreg_log_module = {.name = "test:modfail", .loglevel = AEM_LOG_NOTICE};

static int module_failreg_register(struct aem_module *mod, struct aem_stringslice args)
{
	(void)args;
	mod->logmodule = &module_failreg_log_module;

	aem_logf_ctx(AEM_LOG_ERROR, "My register method always fails!");

	return -9;
}
static void module_failreg_deregister(struct aem_module *mod)
{
	aem_logf_ctx(AEM_LOG_NOTICE, "Deregistering %s", aem_stringbuf_get(&mod->name));
}

const struct aem_module_def aem_module_def = {
	.name = "module_failreg",
	.version = NULL, // Oops!!

	.reg = module_failreg_register,
	.dereg = module_failreg_deregister,

	// Should still get unloaded despite this, because loading didn't succeed
	.check_dereg = aem_module_disable_dereg,
};
