# makefile

include ../config.mak

VERSION = 0.1

SHORTNAME = libdha.so
LIBNAME = libdha-$(VERSION).so

SRCS=libdha.c pci.c pci_names.c
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -fPIC -I. -I.. -Wall -W

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(CC) -shared -o $(LIBNAME) $(OBJS)
	ln -sf $(LIBNAME) $(SHORTNAME)

all:    $(LIBNAME) $(SHORTNAME)

pci_names.c:
	gzip -d -c oth/pci.db.gz >pci.db
	gawk -f pci_db2c.awk pci.db

test:
	$(CC) test.c -o test $(SHORTNAME)

clean:
	rm -f *.o *.so *~

distclean:
	rm -f Makefile.bak *.o *.so test *~ .depend
	rm -f pci_dev_ids.c pci_ids.h pci_names.c pci_names.h pci_vendors.h pci.db

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

install:
	cp $(LIBNAME) $(prefix)/lib/$(LIBNAME)
	rm -f $(prefix)/lib/libdha.so
	ln -sf $(LIBNAME) $(prefix)/lib/libdha.so
	ldconfig

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
