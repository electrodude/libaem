CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

#TEST_PROG_PFX=valgrind -q

CFLAGS+=-std=c99 -fPIC -Wall -Wextra
LDFLAGS+=
LDFLAGS+=-ldl -rdynamic

CFLAGS+=-O3
LDFLAGS+=-O3
#CFLAGS+=-Og -g -DAEM_DEBUG -DAEM_BREAK_ON_ABORT -DAEM_BREAK_ON_BUG
#LDFLAGS+=-g

CFLAGS+=-I./test/

ifeq (,$(findstring Windows,${OS}))
        HOST_SYS:=$(shell uname -s)
else
        HOST_SYS=Windows
endif

SOURCES_LIBAEM=stringbuf.c stringslice.c utf8.c stack.c translate.c pathutil.c registry.c stream.c streams.c pmcrcu.c log.c module.c gc.c
ifeq (${HOST_SYS},Windows)
SOURCES_LIBAEM+=serial.windows.c
else
SOURCES_LIBAEM+=serial.unix.c net.c poll.c unix.c
endif

OBJECTS_LIBAEM=$(patsubst %.c,%.o,${SOURCES_LIBAEM})

SOURCES_LIBAEM_TEST=$(shell echo test.c)
OBJECTS_LIBAEM_TEST=$(patsubst %.c,%.o,${SOURCES_LIBAEM_TEST})

DEPDIR=.deps
DEPFLAGS=-MD -MP -MF ${DEPDIR}/$*.d

all:	libaem.a

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
test_module: 	test/lib/module_empty.so test/lib/module_invalid.so test/lib/module_test.so test/lib/module_test_singleton.so

$(shell mkdir -p ${DEPDIR}/test)
$(shell mkdir -p test/bin test/lib)

test:	${TESTS}

test/bin/%:	test/%.o test/test_common.o libaem.a
	${LD} $^ ${LDFLAGS} -o $@

test/lib/%.so:	test/%.o test/test_common.o libaem.a
	${LD} -shared $^ ${LDFLAGS} -o $@

test_%:	test/bin/%
	cd test && ${TEST_PROG_PFX} ./bin/$*

clean:
	rm -vf ${OBJECTS_LIBAEM} ${OBJECTS_LIBAEM_TEST} libaem.a test/*.o test/bin/* test/lib/* ${DEPDIR}/*.d ${DEPDIR}/test/*.d

libaem.a:	${OBJECTS_LIBAEM}
	${AR} $@ $^
	${RANLIB} $@

%.o:	%.c
	${CC} ${CFLAGS} ${DEPFLAGS} -c $< -o $@

.PHONY:	all test clean

include $(wildcard ${DEPDIR}/*.d)
include $(wildcard ${DEPDIR}/test/*.d)

# vim: set ts=16 :
