#include <aem/module.h>

struct aem_module_def aem_module_def = {
	.name = "module_invalid",
	.version = "0",
	// Invalid: not singleton, yet no .reg method
	.singleton = 0,
	// Should get unloaded anyway, because loading didn't succeed
	.no_unload = 1,
};
