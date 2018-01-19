CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

CFLAGS+=-std=c99 -fPIC -Wall -Wextra
LDFLAGS+=

CFLAGS+=-O3
LDFLAGS+=-O3

ifeq (,$(findstring Windows,${OS}))
        HOST_SYS:=$(shell uname -s)
else
        HOST_SYS=Windows
endif

SOURCES_LIBAEM=stringbuf.c stringslice.c utf8.c stack.c log.c gc.c
ifeq (${HOST_SYS},Windows)
SOURCES_LIBAEM+=serial.windows.c
else
SOURCES_LIBAEM+=serial.unix.c
endif

OBJECTS_LIBAEM=$(patsubst %.c,%.o,${SOURCES_LIBAEM})

SOURCES_LIBAEM_TEST=$(shell echo test.c)
OBJECTS_LIBAEM_TEST=$(patsubst %.c,%.o,${SOURCES_LIBAEM_TEST})

DEPDIR=.deps
DEPFLAGS=-MD -MP -MF ${DEPDIR}/$*.d

$(shell mkdir -p ${DEPDIR})

all:	libaem.a

test:	libaem_test
	./libaem_test

clean:
	rm -vf ${OBJECTS_LIBAEM} ${OBJECTS_LIBAEM_TEST} ${DEPDIR}/*.d libaem_test libaem.a

libaem_test:	${OBJECTS_LIBAEM_TEST} libaem.a
	${LD} $^ ${LDFLAGS} -o $@

libaem.a:	${OBJECTS_LIBAEM}
	${AR} $@ $^
	${RANLIB} $@

%.o:	%.c
	${CC} ${CFLAGS} ${DEPFLAGS} -c $< -o $@

.PHONY:	all test clean

include $(wildcard ${DEPDIR}/*.d)

# vim: set ts=13 :
