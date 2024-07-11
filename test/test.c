#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

TEST_MAIN(int argc, char **argv)
{
	(void)argc; (void)argv;

	aem_logf_ctx(AEM_LOG_NOTICE, "test start");

	return 0;
}
