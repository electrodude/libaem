# Include this in your main Makefile, e.g.
#include aem/make.inc

CFLAGS+=-std=c99 -Wall -Wextra

CFLAGS+=-I.

LIBAEM_DIR=aem

LIBS+=$(LIBAEM_DIR)/libaem.a

DEPDIR=.deps
DEPFLAGS=-MD -MP -MF $(DEPDIR)/$*.d

$(shell mkdir -p $(DEPDIR))

%.o:    %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(LIBAEM_DIR)/libaem.a:
	$(MAKE) -C $(LIBAEM_DIR) -w libaem.a

libaem_clean:
	rm -vf $(DEPDIR)/*.d
	$(MAKE) -C $(LIBAEM_DIR) -w clean

.PHONY: libaem_clean

.PHONY: $(LIBAEM_DIR)/libaem.a

include $(wildcard $(DEPDIR)/*.d)

# vim: set syn=make :
