#include <aem/module.h>

const struct aem_module_def aem_module_def = {
	.name = "module_test_singleton",
	.version = "0",
	.singleton = 1,
};
