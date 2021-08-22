#include <aem/module.h>

#undef aem_log_module_current
#define aem_log_module_current (&module_test_log_module)
struct aem_log_module module_test_log_module = {.loglevel = AEM_LOG_NOTICE};

static int module_test_register(struct aem_module *mod, struct aem_stringslice args)
{
	mod->logmodule = &module_test_log_module;
	if (aem_log_header(&aem_log_buf, AEM_LOG_NOTICE)) {
		aem_stringbuf_puts(&aem_log_buf, "Arguments: \"");
		aem_stringbuf_putss(&aem_log_buf, args);
		aem_stringbuf_puts(&aem_log_buf, "\"\n");
		aem_log_str(&aem_log_buf);
	}
	if (aem_stringslice_len(args)) {
		aem_stringbuf_putc(&mod->name, '-');
		aem_stringbuf_putss(&mod->name, args);
	}

	aem_logf_ctx(AEM_LOG_NOTICE, "Registered %s", aem_stringbuf_get(&mod->name));

	return 0;
}
static void module_test_deregister(struct aem_module *mod)
{
	aem_logf_ctx(AEM_LOG_NOTICE, "Deregistering %s", aem_stringbuf_get(&mod->name));
	//aem_logf_ctx(aem_log_module_current->loglevel, "Current log level: %s", aem_log_level_describe(aem_log_module_current->loglevel));
}

const struct aem_module_def aem_module_def = {
	.name = "module_test",
	.version = "0",

	.reg = module_test_register,
	.dereg = module_test_deregister,
};
