CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

CFLAGS=-std=c99 -Wall -Wextra
LDFLAGS=

CFLAGS+=-O3
LDFLAGS+=-O3

SOURCES_LIBAEM=$(shell echo stringbuf.c stringslice.c utf8.c stack.c)
OBJECTS_LIBAEM=$(patsubst %.c,%.o,${SOURCES_LIBAEM})

SOURCES_LIBAEM_TEST=$(shell echo test.c)
OBJECTS_LIBAEM_TEST=$(patsubst %.c,%.o,${SOURCES_LIBAEM_TEST})

all:	libaem.a

test:	libaem_test
	./libaem_test

clean:
	rm -vf *.o libaem_test libaem.a depends.d

libaem_test:	${OBJECTS_LIBAEM_TEST} libaem.a
	${LD} $^ ${LDFLAGS} -o $@

libaem.a:	${OBJECTS_LIBAEM}
	${AR} $@ $^
	${RANLIB} $@

%.o:	%.c
	${CC} ${CFLAGS} -c $< -o $@

depends.d:	${SOURCES_LIBAEM}
	@${CC} ${CFLAGS} -MM $^ > $@

.PHONY:	all test clean depends.d

include depends.d

# vim: set ts=13 :
