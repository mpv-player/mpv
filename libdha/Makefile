# makefile

include ../config.mak

MAJOR_VERSION = 1
MINOR_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION) 


ifeq ($(TARGET_OS),CYGWIN)
SHORTNAME = libdha.dll
else
SHORTNAME = libdha.so.$(MAJOR_VERSION)
SONAME_FLAGS = -Wl,-soname,$(SHORTNAME)
#SHORTNAME = libdha.so
VSHORTNAME = libdha.so
endif
ifeq ($(TARGET_WIN32),yes)
LIBNAME = libdha.a
SHORTNAME = libdha.a
else
LIBNAME = libdha.so.$(VERSION)
endif

SRCS=libdha.c \
     mtrr.c \
     pci.c \
     pci_names.c \

OBJS=$(SRCS:.c=.o)

CFLAGS  = -fPIC -I. -I.. $(OPTFLAGS)

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
ifeq ($(TARGET_WIN32),yes)
	$(AR) r $(LIBNAME) $(OBJS)
else
	#$(CC) -shared $(SONAME_FLAGS) -o $(LIBNAME) $(OBJS) $(LIBS)
	$(CC) -shared -Wl,-soname -Wl,$(LIBNAME) -o $(LIBNAME) $(OBJS) $(LIBS)
	ln -sf $(LIBNAME) $(SHORTNAME)
	ln -sf $(LIBNAME) $(VSHORTNAME)
endif

all:    $(LIBNAME) $(SHORTNAME)

pci_names.c:	oth/pci.db
	LC_ALL=C $(AWK) -f pci_db2c.awk $<

test:
	$(CC) test.c -o test $(SHORTNAME)

clean:
	rm -f *.o *.a *~ *.so *.so.*
	rm -f pci_*.c pci_*.h pci.db

distclean: clean
	rm -f .depend test

dep depend: pci_names.c
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

install:
	-mkdir -p $(LIBDIR)
	install -m 755 $(INSTALLSTRIP) -p $(LIBNAME) $(LIBDIR)/$(LIBNAME)
	rm -f $(LIBDIR)/libdha.so
	ln -sf $(LIBNAME) $(LIBDIR)/$(SHORTNAME)
ifeq ($(TARGET_OS),OpenBSD)
	-ldconfig -R
else
	-ldconfig
endif

uninstall:
	rm -f $(LIBDIR)/libdha.so $(LIBDIR)/$(SHORTNAME) $(LIBDIR)/$(LIBNAME)
ifeq ($(TARGET_OS),OpenBSD)
	-ldconfig -R
else
	-ldconfig
endif

ifneq ($(wildcard .depend),)
include .depend
endif
