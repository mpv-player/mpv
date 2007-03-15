include ../config.mak

LIBNAME_MPLAYER        = lidha.a
LIBNAME_MPLAYER_SHARED = lidha.so

LIBS = $(LIBNAME_MPLAYER_SHARED)

MAJOR_VERSION = 1
MINOR_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION)

SHORTNAME = libdha.so.$(MAJOR_VERSION)
VSHORTNAME = libdha.so
ifeq ($(TARGET_WIN32),yes)
LIBNAME = libdha.a
SHORTNAME = libdha.a
else
LIBNAME = libdha.so.$(VERSION)
endif

SRCS_MPLAYER = libdha.c \
               mtrr.c \
               pci.c \
               pci_names.c \

OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

CFLAGS = -fPIC -I. -I.. $(OPTFLAGS)

ifeq ($(TARGET_OS),OpenBSD)
ifeq ($(TARGET_ARCH_X86),yes)
EXTRALIBS += -li386
endif
endif

# If you want libdha to use svgalib_helper for hardware access,
# uncomment this statement, and change the -I to the correct directory
# that includes svgalib_helper.o:
ifneq ($(wildcard svgalib_helper),)
CFLAGS += -DDEV_SVGA=\"/dev/svga\" -DCONFIG_SVGAHELPER -Isvgalib_helper/
endif

all: pci_names.c $(LIBS)

$(LIBNAME_MPLAYER): $(OBJS_MPLAYER)
	$(AR) r $@ $^

$(LIBNAME_MPLAYER_SHARED): $(OBJS_MPLAYER)
	$(CC) -shared -Wl,-soname -Wl,$@ -o $@ $^ $(EXTRALIBS)
	ln -sf $@ $(SHORTNAME)
	ln -sf $@ $(VSHORTNAME)

pci_names.c:	oth/pci.db
	LC_ALL=C $(AWK) -f pci_db2c.awk $<

test: pci.o
	$(CC) test.c -o $@ $^

clean:
	rm -f *.o *.a *~ *.so *.so.* pci_*.c pci_*.h pci.db

distclean: clean
	rm -f .depend test

dep depend: pci_names.c
	$(CC) -MM $(CFLAGS) $(SRCS_MPLAYER) 1>.depend

install:
	-mkdir -p $(LIBDIR)
	$(INSTALL) -m 755 $(INSTALLSTRIP) -p $(LIBNAME) $(LIBDIR)/$(LIBNAME)
	ln -sf $(LIBNAME) $(LIBDIR)/$(SHORTNAME)
	-$(LDCONFIG)

uninstall:
	rm -f $(LIBDIR)/libdha.so $(LIBDIR)/$(SHORTNAME) $(LIBDIR)/$(LIBNAME)
	-$(LDCONFIG)

-include .depend
