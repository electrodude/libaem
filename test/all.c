#define _POSIX_C_SOURCE 199309L

#include "test_common.h"

int test_utf8(int argc, char **argv);
int test_module(int argc, char **argv);
int test_nfa(int argc, char **argv);
int test_nfa_lexer(int argc, char **argv);
int test_pathutil(int argc, char **argv);
int test_stringslice(int argc, char **argv);
int test_stringslice_numeric(int argc, char **argv);

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
