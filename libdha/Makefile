# makefile

include ../config.mak

MAJOR_VERSION = 0
MINOR_VERSION = 1
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION) 


ifeq ($(TARGET_OS),CYGWIN)
SHORTNAME = libdha.dll
else
SHORTNAME = libdha.so.$(MAJOR_VERSION)
SONAME_FLAGS = -Wl,-soname,$(SHORTNAME)
#SHORTNAME = libdha.so
VSHORTNAME = libdha.so
endif
LIBNAME = libdha.so.$(VERSION)

SRCS=libdha.c mtrr.c pci.c pci_names.c
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -fPIC -I. -I..
LIBS = 
ifeq ($(TARGET_OS),OpenBSD)
ifeq ($(TARGET_ARCH_X86),yes)
LIBS += -li386
endif
endif

# If you want libdha to use svgalib_helper for hardware access,
# uncomment this statement, and change the -I to the correct directory
# that includes svgalib_helper.o:
ifneq ($(wildcard svgalib_helper),)
CFLAGS += -DDEV_SVGA=\"/dev/svga\" -DCONFIG_SVGAHELPER -Isvgalib_helper/
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o: pci_names.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	#$(CC) -shared $(SONAME_FLAGS) -o $(LIBNAME) $(OBJS) $(LIBS)
	$(CC) -shared -Wl,-soname -Wl,$(LIBNAME) -o $(LIBNAME) $(OBJS) $(LIBS)
	ln -sf $(LIBNAME) $(SHORTNAME)
	ln -sf $(LIBNAME) $(VSHORTNAME)

all:    $(LIBNAME) $(SHORTNAME)

pci_names.c:
	$(AWK) -f pci_db2c.awk oth/pci.db

test:
	$(CC) test.c -o test $(SHORTNAME)

clean:
	rm -f *.o *.so *.so.* *~

distclean:
	rm -f Makefile.bak *.o *.so *.so.* test *~ .depend
	rm -f pci_dev_ids.c pci_ids.h pci_names.c pci_names.h pci_vendors.h pci.db

dep:    depend

depend: pci_names.c
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

uninstall:
	rm -f $(prefix)/lib/libdha.so $(prefix)/lib/$(SHORTNAME) $(prefix)/lib/$(LIBNAME)
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
