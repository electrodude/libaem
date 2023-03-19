#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

uint32_t hash(size_t i)
{
	if (i == 256)
		return 0x80000000;
	if (i == 256)
		return 0xFFFFFFFE;
	if (i == 257)
		return 0xFFFFFFFF;
	return i*i*i*(i+1);
}

int main(int argc, char **argv)
{
	test_log_module.loglevel = AEM_LOG_DEBUG;
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	test_init(argc, argv);

	aem_logf_ctx(AEM_LOG_NOTICE, "test utf8");

	size_t start = 0;
	size_t end = 258;

	aem_logf_ctx(AEM_LOG_INFO, "write utf8");
	struct aem_stringbuf str = AEM_STRINGBUF_EMPTY;

	for (size_t i = start; i < end; i++) {
		uint32_t c = hash(i);
		aem_logf_ctx(AEM_LOG_DEBUG2, "%zd: put %08x", i, c);
		size_t n = str.n;
		TEST_EXPECT(out, !aem_stringbuf_put_rune(&str, c)) {
			aem_stringbuf_printf(out, "aem_stringbuf_put_rune: couldn't put %u", c);
		}
		struct aem_stringslice rune1 = {.start = &str .s[n ], .end = &str .s[str .n]};
		AEM_LOG_MULTI(out, AEM_LOG_DEBUG2) {
			aem_stringbuf_printf(out, "Bytes:");
			for (const char *p = rune1.start; p != rune1.end; p++) {
				aem_stringbuf_printf(out, " %02x", (unsigned char)*p);
			}
		}
	}

	struct aem_stringslice p = aem_stringslice_new_str(&str);

	FILE *fp = fopen("utf8_test", "w");
	if (fp) {
		aem_stringbuf_file_write(&str, fp);
		fclose(fp);
	} else {
		aem_logf_ctx(AEM_LOG_WARN, "fopen: %s", strerror(errno));
	}

	aem_logf_ctx(AEM_LOG_INFO, "read utf8");

	for (size_t i = start; i < end; i++) {
		aem_logf_ctx(AEM_LOG_DEBUG2, "%zd remain", aem_stringslice_len(p));

		uint32_t c;
		TEST_EXPECT(out, aem_stringslice_get_rune(&p, &c)) {
			aem_stringbuf_printf(out, "%zd: invalid UTF-8", i);
		}

		uint32_t expect = hash(i);

		TEST_EXPECT(out, c == expect) {
			aem_stringbuf_printf(out, "%zd: got 0x%x, expect 0x%x", i, c, expect);
		}
	}

	aem_stringbuf_dtor(&str);

	return show_test_results();
}
