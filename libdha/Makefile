# makefile

include ../config.mak

VERSION = 0.1

ifeq ($(TARGET_OS),CYGWIN)
SHORTNAME = libdha.dll
else
SHORTNAME = libdha.so
endif
LIBNAME = libdha-$(VERSION).so

SRCS=libdha.c mtrr.c pci.c pci_names.c
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -fPIC -I. -I.. -Wall -W
LIBS = 
ifeq ($(TARGET_OS),OpenBSD)
LIBS += -li386
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(CC) -shared -o $(LIBNAME) $(OBJS) $(LIBS)
	ln -sf $(LIBNAME) $(SHORTNAME)

all:    $(LIBNAME) $(SHORTNAME)

pci_names.c:
	gzip -d -c oth/pci.db.gz >pci.db
	$(AWK) -f pci_db2c.awk pci.db

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
	mkdir -p $(prefix)/lib
	install -m 755 -s -p $(LIBNAME) $(prefix)/lib/$(LIBNAME)
	rm -f $(prefix)/lib/libdha.so
	ln -sf $(LIBNAME) $(prefix)/lib/$(SHORTNAME)
ifeq ($(TARGET_OS),OpenBSD)
	ldconfig -R
else
	ldconfig
endif

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
