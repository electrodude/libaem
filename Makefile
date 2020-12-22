CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

CFLAGS+=-std=c99 -fPIC -Wall -Wextra
LDFLAGS+=

CFLAGS+=-O3
LDFLAGS+=-O3

CFLAGS+=-I./test/

ifeq (,$(findstring Windows,${OS}))
        HOST_SYS:=$(shell uname -s)
else
        HOST_SYS=Windows
endif

SOURCES_LIBAEM=stringbuf.c stringslice.c utf8.c stack.c stream.c pmcrcu.c log.c gc.c
ifeq (${HOST_SYS},Windows)
SOURCES_LIBAEM+=serial.windows.c
else
SOURCES_LIBAEM+=serial.unix.c net.c poll.c
endif

OBJECTS_LIBAEM=$(patsubst %.c,%.o,${SOURCES_LIBAEM})

SOURCES_LIBAEM_TEST=$(shell echo test.c)
OBJECTS_LIBAEM_TEST=$(patsubst %.c,%.o,${SOURCES_LIBAEM_TEST})

DEPDIR=.deps
DEPFLAGS=-MD -MP -MF ${DEPDIR}/$*.d

TESTS=test_test \
      test_utf8 \
      test_test_childproc \
      test_server \
      test_client

TESTS_BIN=$(patsubst test_%,test/bin/%,${TESTS})

$(shell mkdir -p ${DEPDIR}/test)
$(shell mkdir -p test/bin)

all:	libaem.a

test:	${TESTS}

test/bin/%:	test/%.o libaem.a
	${LD} $^ ${LDFLAGS} -o $@

test_%:	test/bin/%
	cd test && ./bin/$* || rm ./bin/$*

clean:
	rm -vf ${OBJECTS_LIBAEM} ${OBJECTS_LIBAEM_TEST} libaem.a test/*.o ${TESTS_BIN} ${DEPDIR}/*.d ${DEPDIR}/test/*.d

libaem.a:	${OBJECTS_LIBAEM}
	${AR} $@ $^
	${RANLIB} $@

%.o:	%.c
	${CC} ${CFLAGS} ${DEPFLAGS} -c $< -o $@

.PHONY:	all test clean

include $(wildcard ${DEPDIR}/*.d)
include $(wildcard ${DEPDIR}/test/*.d)

# vim: set ts=13 :
