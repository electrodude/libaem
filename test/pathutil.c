#define _POSIX_C_SOURCE 199309L
#include <libgen.h>

#include "test_common.h"

#include <aem/pathutil.h>

struct aem_stringbuf buf = {0};
static void test_sandbox_path(const char *base, const char *path, const char *ext, int rc_expect, const char *result)
{
	aem_logf_ctx(AEM_LOG_INFO, "sandbox_path(\"%s\", \"%s\", \"%s\") expect (%d, \"%s\")", base, path, ext, rc_expect, result);

	aem_stringbuf_reset(&buf);
	int rc = aem_sandbox_path(&buf, aem_stringslice_new_cstr(base), aem_stringslice_new_cstr(path), ext);
	int match = aem_stringslice_eq(aem_stringslice_new_str(&buf), result);

	TEST_EXPECT(out, rc == rc_expect && match) {
		aem_stringbuf_printf(out, "sandbox_path(\"%s\", \"%s\", \"%s\") returned (%d, \"%s\"), expected (%d, \"%s\")!", base, path, ext, rc, aem_stringbuf_get(&buf), rc_expect, result);
	}
}

static void test_dirname(const char *path, const char *result)
{
	aem_logf_ctx(AEM_LOG_INFO, "dirname(\"%s\") expect (\"%s\")", path, result);

	aem_stringbuf_reset(&buf);
	aem_stringbuf_puts(&buf, path);
	aem_stringbuf_get(&buf);
	const char *result_sys = dirname(buf.s);

	// Make sure testcase is sane
	{
		if (strcmp(result_sys, result))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: given result \"%s\" doesn't match system dirname result: \"%s\"", result, result_sys);
	}
	{
		struct aem_stringslice path_ss = aem_stringslice_new_cstr(path);
		if (strcmp(result, ".") && !aem_stringslice_match_prefix(&path_ss, aem_stringslice_new_cstr(result)))
			aem_logf_ctx(AEM_LOG_BUG, "Testcase fails invariant: given result \"%s\" is not '.' or a prefix of path \"%s\"", result, path);
	}

	struct aem_stringslice dir = aem_dirname(aem_stringslice_new_cstr(path));
	int match = aem_stringslice_eq(dir, result);

	TEST_EXPECT(out, match) {
		aem_stringbuf_printf(out, "dirname(\"%s\") returned (\"", path);
		aem_stringbuf_putss(out, dir);
		aem_stringbuf_printf(out, "\"), expected (\"%s\")!", result);
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


	// aem_dirname
	test_dirname(NULL, ".");
	test_dirname("", ".");
	test_dirname(".", ".");
	test_dirname("..", ".");
	test_dirname("...", ".");
	test_dirname("f", ".");
	test_dirname("file", ".");
	test_dirname("/file", "/");
	test_dirname("/d/", "/");
	test_dirname("dir/f", "dir");
	test_dirname("d/f", "d");
	test_dirname("d/D/", "d");
	test_dirname("/base/dir/path", "/base/dir");
	test_dirname("/base/dir/path/", "/base/dir");
	test_dirname("/base/dir/path///", "/base/dir");
	test_dirname("/base/dir///path", "/base/dir");


	aem_stringbuf_dtor(&buf);

	return show_test_results();
}
