#include <stdlib.h>
#include <stdio.h>

#include "log.h"
#include "stringbuf.h"
#include "stringslice.h"
#include "stack.h"
#include "linked_list.h"

int main(int argc, char **argv)
{
	aem_log_fp = stderr;
	aem_log_level_curr = AEM_LOG_INFO;

	aem_logf_ctx(AEM_LOG_NOTICE, "test start\n");


	aem_logf_ctx(AEM_LOG_NOTICE, "test end\n");

	return 0;
}
