#include "test_common.h"

int main(int argc, char **argv)
{
	aem_log_stderr();
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	aem_logf_ctx(AEM_LOG_NOTICE, "test utf8");

	size_t n = 128;

	aem_logf_ctx(AEM_LOG_INFO, "write utf8");
	struct aem_stringbuf str;
	aem_stringbuf_init(&str);

	for (size_t i = 0; i < n; i++)
	{
		unsigned int c = i*i*i*(i+1);
		if (aem_stringbuf_put(&str, c))
		{
			aem_logf_ctx(AEM_LOG_BUG, "aem_stringbuf_put_utf8: couldn't put %u", c);
			return 1;
		}
	}

	struct aem_stringslice p = aem_stringslice_new_str(&str);

	FILE *fp = fopen("utf8_test", "w");
	if (fp)
	{
		aem_stringbuf_file_write(&str, fp);
		fclose(fp);
	}

	aem_logf_ctx(AEM_LOG_INFO, "read utf8");

	for (size_t i = 0; i < n; i++)
	{
		unsigned int c2 = aem_stringslice_get(&p);

		unsigned int c = i*i*i*(i+1);

		if (c != c2)
		{
			aem_logf_ctx(AEM_LOG_BUG, "%zd: expect 0x%x, got 0x%x", i, c, c2);
			return 1;
		}
	}

	aem_stringbuf_dtor(&str);

	return show_test_results();
}
