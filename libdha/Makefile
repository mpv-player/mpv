# makefile

include ../config.mak

VERSION = 0.1

SHORTNAME = libdha.so
LIBNAME = libdha-$(VERSION).so

SRCS=libdha.c pci.c
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

test:
	$(CC) test.c -o test $(SHORTNAME)

clean:
	rm -f *.o *.so *~

distclean:
	rm -f Makefile.bak *.o *.so test *~ .depend

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
