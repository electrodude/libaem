#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

int test_utf8(struct test *test);
int test_module(struct test *test);
int test_nfa(struct test *test);
int test_nfa_lexer(struct test *test);
int test_pathutil(struct test *test);
int test_stringslice(struct test *test);
int test_stringslice_numeric(struct test *test);

TESTS {
	TEST(test_utf8),
	TEST(test_module),
	TEST(test_nfa),
	TEST(test_nfa_lexer),
	TEST(test_pathutil),
	TEST(test_stringslice),
	TEST(test_stringslice_numeric),
	{0},
};
