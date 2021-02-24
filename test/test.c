#include "test_common.h"

int main(int argc, char **argv)
{
	aem_log_stderr();
	aem_log_module_default.loglevel = AEM_LOG_DEBUG;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	aem_logf_ctx(AEM_LOG_NOTICE, "test start");

	return show_test_results();
}
