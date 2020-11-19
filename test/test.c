#include <stdlib.h>
#include <stdio.h>

#include <aem/log.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>
#include <aem/stack.h>
#include <aem/linked_list.h>
#include <aem/iter_gen.h>

int main(int argc, char **argv)
{
	int rc = 0;

	aem_log_stderr();
	aem_log_module_default.loglevel = AEM_LOG_DEBUG;

	aem_logf_ctx(AEM_LOG_NOTICE, "test start\n");

	rc = 0;

	aem_logf_ctx(AEM_LOG_NOTICE, "test end\n");

	return rc;
}
