#include <aem/module.h>

const struct aem_module_def aem_module_def = {
	.name = "module_test_singleton",
	.version = "0",
	.check_reg = aem_module_check_reg_singleton,
};
