include ../config.mak

LIBNAME_MPLAYER = libdha.a

SRCS_MPLAYER = libdha.c \
               mtrr.c \
               pci.c \
               pci_names.c \

# If you want libdha to use svgalib_helper for hardware access,
# uncomment this statement, and change the -I to the correct directory
# that includes svgalib_helper.o:
#CFLAGS += -DDEV_SVGA=\"/dev/svga\" -DCONFIG_SVGAHELPER -Isvgalib_helper/

libs: pci_names.c

dep depend:: pci_names.c

include ../mpcommon.mak

pci_names.c: pci.db
	LC_ALL=C $(AWK) -f pci_db2c.awk $<

test: pci.o
	$(CC) test.c -o $@ $^

clean::
	rm -f pci_*.c pci_*.h
