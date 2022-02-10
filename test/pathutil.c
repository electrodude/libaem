#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

#include <aem/pathutil.h>

struct aem_stringbuf buf = {0};
static void test_sandbox_path(const char *base, const char *path, const char *ext, int rc_expect, const char *result)
{
	aem_logf_ctx(AEM_LOG_INFO, "sandbox_path(\"%s\", \"%s\", \"%s\") expect (%d, \"%s\")", base, path, ext, rc_expect, result);

	aem_stringbuf_reset(&buf);
	int rc = aem_sandbox_path(&buf, aem_stringslice_new_cstr(base), aem_stringslice_new_cstr(path), ext);
	int match = aem_stringslice_eq(aem_stringslice_new_str(&buf), result);

	if (rc != rc_expect || !match) {
		test_errors++;
		aem_logf_ctx(AEM_LOG_BUG, "sandbox_path(\"%s\", \"%s\", \"%s\") returned (%d, \"%s\"), expected (%d, \"%s\")!", base, path, ext, rc, aem_stringbuf_get(&buf), rc_expect, result);
	}
}

int main(int argc, char **argv)
{
	aem_log_module_default.loglevel = AEM_LOG_NOTICE;
	aem_log_module_default_internal.loglevel = AEM_LOG_DEBUG;

	test_init(argc, argv);

	aem_logf_ctx(AEM_LOG_NOTICE, "test sandbox_path");

	//                base path        subpath               ext    rc result
	// Empty arguments
	test_sandbox_path(""             , ""                  , NULL  , 0, "./"                        );
	test_sandbox_path(""             , ""                  , ""    , 2, "./"                        );
	test_sandbox_path(""             , ""                  , ".ext", 2, "./.ext"                    );

	test_sandbox_path("base"         , ""                  , NULL  , 0, "base/"                     );
	test_sandbox_path("base"         , ""                  , ""    , 2, "base/"                     );
	test_sandbox_path("base"         , ""                  , ".ext", 2, "base/.ext"                 );

	test_sandbox_path(""             , "filename"          , NULL  , 0, "./filename"                );
	test_sandbox_path(""             , "filename"          , ""    , 0, "./filename"                );
	test_sandbox_path(""             , "filename"          , ".ext", 0, "./filename.ext"            );

	test_sandbox_path("base"         , "filename"          , NULL  , 0, "base/filename"             );
	test_sandbox_path("base"         , "filename"          , ""    , 0, "base/filename"             );
	test_sandbox_path("base"         , "filename"          , ".ext", 0, "base/filename.ext"         );

	// Slashes between base and subpath
	test_sandbox_path("base"         , "/filename"         , NULL  , 0, "base/filename"             );
	test_sandbox_path("base"         , "/filename"         , ""    , 0, "base/filename"             );
	test_sandbox_path("base"         , "/filename"         , ".ext", 0, "base/filename.ext"         );

	test_sandbox_path("base/"        , "filename"          , NULL  , 0, "base/filename"             );
	test_sandbox_path("base/"        , "filename"          , ""    , 0, "base/filename"             );
	test_sandbox_path("base/"        , "filename"          , ".ext", 0, "base/filename.ext"         );

	test_sandbox_path("base/"        , "/filename"         , NULL  , 0, "base/filename"             );
	test_sandbox_path("base/"        , "/filename"         , ""    , 0, "base/filename"             );
	test_sandbox_path("base/"        , "/filename"         , ".ext", 0, "base/filename.ext"         );

	test_sandbox_path("base"         , "filename/"         , NULL  , 0, "base/filename/"            );
	test_sandbox_path("base"         , "filename/"         , ""    , 2, "base/filename/"            );
	test_sandbox_path("base"         , "filename/"         , ".ext", 2, "base/filename/.ext"        );

	// Extensions
	test_sandbox_path("base"         , "filename.ext"      , NULL  , 0, "base/filename.ext"         );
	test_sandbox_path("base"         , "filename.ext"      , ""    , 0, "base/filename.ext"         );
	test_sandbox_path("base"         , "filename.ext"      , ".ext", 0, "base/filename.ext"         );
	test_sandbox_path("base"         , "filename.ext/"     , ".ext", 2, "base/filename.ext/.ext"    );
	test_sandbox_path("base"         , "filename.abc.ext"  , ".ext", 0, "base/filename.abc.ext"     );
	test_sandbox_path("base"         , "filename.ext.abc"  , ".ext", 0, "base/filename.ext.abc.ext" );

	// Directory traversal
	test_sandbox_path("/base/../dir" , "/filename"         , NULL  , 0, "/base/../dir/filename"     );
	test_sandbox_path("/base/../dir" , "//filename"        , NULL  , 0, "/base/../dir/filename"     );
	test_sandbox_path("/base/../dir" , "./filename"        , NULL  , 0, "/base/../dir/filename"     );
	test_sandbox_path("/base/../dir" , "../filename"       , NULL  , 1, "/base/../dir/filename"     );
	test_sandbox_path("/base/../dir" , "../filename"       , ".ext", 1, "/base/../dir/filename.ext" );
	test_sandbox_path("/base/../dir" , "../filename/"      , ".ext", 3, "/base/../dir/filename/.ext");

	test_sandbox_path("/base/../dir/", "/filename"         , NULL  , 0, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir/", "//filename"        , NULL  , 0, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir/", "./filename"        , NULL  , 0, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir/", "/./filename"       , NULL  , 0, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir/", "../filename"       , NULL  , 1, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir/", "/../filename"      , NULL  , 1, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir/", "../filename"       , ".ext", 1, "/base/../dir/filename.ext");
	test_sandbox_path("/base/../dir/", "../filename/"      , ".ext", 3, "/base/../dir/filename/.ext");

	// Complex directory traversal
	test_sandbox_path("/base/../dir" , "dir2//filename"    , NULL  , 0, "/base/../dir/dir2/filename");
	test_sandbox_path("/base/../dir" , "dir2/./filename"   , NULL  , 0, "/base/../dir/dir2/filename");
	test_sandbox_path("/base/../dir" , "dir2/../filename"  , NULL  , 0, "/base/../dir/filename"  );
	test_sandbox_path("/base/../dir" , "a/..///b/filename" , NULL  , 0, "/base/../dir/b/filename");
	test_sandbox_path("/base/../dir" , "/a/..///b/filename", NULL  , 0, "/base/../dir/b/filename");
	test_sandbox_path("/base/../dir" , "a/.././b/filename" , NULL  , 0, "/base/../dir/b/filename");
	test_sandbox_path("/base/../dir" , "a/../../b/filename", NULL  , 1, "/base/../dir/b/filename");


	aem_stringbuf_dtor(&buf);

	return show_test_results();
}
