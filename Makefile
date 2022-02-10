CC=gcc
AR=ar rcu
RANLIB=ranlib

ifeq (${DEBUG},)
CFLAGS+=-O3
else
CFLAGS+=-g -Og -DAEM_DEBUG -DAEM_BREAK_ON_ABORT
#CFLAGS+=-DAEM_BREAK_ON_BUG
LDFLAGS+=-g
ifeq (${DEBUG},asan)
CFLAGS+=-fsanitize=address -fno-omit-frame-pointer
LDFLAGS+=-lasan
endif
ifeq (${DEBUG},valgrind)
CFLAGS+=-DVALGRIND
TEST_PROG_PFX=valgrind -q
#TEST_PROG_PFX=valgrind -q --leak-check=full --show-leak-kinds=all
endif
ifeq (${DEBUG},gdb)
TEST_PROG_PFX=gdb
endif
endif

ifeq (${RCU_IMPL},urcu)
CFLAGS+=$(shell pkg-config --cflags --static liburcu)
LDFLAGS+=$(shell pkg-config --libs --static liburcu)
CFLAGS+=-DAEM_HAVE_URCU
endif

CFLAGS+=-std=c99 -fPIC -fno-strict-aliasing -Wall -Wextra -Wwrite-strings -Werror-implicit-function-declaration
LDFLAGS+=-ldl -rdynamic

CFLAGS+=-I./test/

# Anything that isn't Windows is Unix
ifeq (,$(findstring Windows,${OS}))
        HOST_SYS:=$(shell uname -s)
else
        HOST_SYS=Windows
endif

SOURCES_LIBAEM=memory.c stringbuf.c stringslice.c utf8.c stack.c translate.c ansi-term.c pathutil.c registry.c regex.c nfa.c nfa-util.c stream.c streams.c pmcrcu.c log.c module.c gc.c
ifeq (${HOST_SYS},Windows)
SOURCES_LIBAEM+=serial.windows.c
else
SOURCES_LIBAEM+=serial.unix.c net.c poll.c unix.c
endif

OBJECTS_LIBAEM=$(patsubst %.c,%.o,${SOURCES_LIBAEM})

SOURCES_LIBAEM_TEST=$(shell echo test.c)
OBJECTS_LIBAEM_TEST=$(patsubst %.c,%.o,${SOURCES_LIBAEM_TEST})

DEPDIR=.deps
DEPFLAGS=-MD -MP -MF ${DEPDIR}/$(subst /,--,$*).d
$(shell mkdir -p ${DEPDIR})

all: libaem.a

TESTS=test_utf8 \
      test_module \
      test_pathutil \
      test_stringslice \
      test_stringslice_numeric
#      test_childproc \
#      test_server \
#      test_client \

TEST_PROGS=${TESTS} childproc_child

test_childproc:	test/bin/childproc_child
test_module: 	test/lib/module_empty.so test/lib/module_failreg.so test/lib/module_test.so test/lib/module_test_singleton.so

$(shell mkdir -p test/bin test/lib)

test: ${TESTS}

test/bin/%: test/%.o test/test_common.o libaem.a
	${CC} $^ ${LDFLAGS} -o $@

test/lib/%.so: test/%.o test/test_common.o libaem.a
	${CC} -shared $^ ${LDFLAGS} -o $@

test_%: test/bin/%
	cd test && ${TEST_PROG_PFX} ./bin/$*

clean:
	rm -rvf ${OBJECTS_LIBAEM} ${OBJECTS_LIBAEM_TEST} libaem.a test/*.o test/bin test/lib ${DEPDIR}

libaem.a: ${OBJECTS_LIBAEM}
	${AR} $@ $^
	${RANLIB} $@

%.o: %.c
	${CC} ${CFLAGS} ${DEPFLAGS} -o $@ -c $<

.PHONY: all test clean

include $(wildcard ${DEPDIR}/*.d)
