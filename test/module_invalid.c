#include <aem/module.h>

const struct aem_module_def aem_module_def = {
	.name = "module_invalid",
	.version = "0",
	// Invalid: not singleton, yet no .reg method
	.singleton = 0,
	// Should still get unloaded despite this, because loading didn't succeed
	.check_dereg = aem_module_disable_dereg,
};
