include ../config.mak

LIBNAME_MPLAYER = libvidix.a

SRCS_MPLAYER    = vidixlib.c \
		  dha.c \
		  mtrr.c \
		  pci.c \
		  pci_names.c \
		  pci_dev_ids.c \
                  $(wildcard *.c)

OBJS_MPLAYER    = mga_crtc2_vid.o \
                  rage128_vid.o \

# If you want libdha to use svgalib_helper for hardware access,
# uncomment this statement, and change the -I to the correct directory
# that includes svgalib_helper.o:
#CFLAGS += -DDEV_SVGA=\"/dev/svga\" -DCONFIG_SVGAHELPER -Isvgalib_helper/

libs: pci_names.c

dep depend:: pci_names.c

include ../mpcommon.mak

mga_crtc2_vid.o: mga_vid.c
	$(CC) -c $(CFLAGS) -DCRTC2 -o $@ $<

rage128_vid.o: radeon_vid.c
	$(CC) -c $(CFLAGS) -DRAGE128 -o $@ $<

pci_names.c pci_dev_ids.c: pci.db
	LC_ALL=C $(AWK) -f pci_db2c.awk $<

clean::
	rm -f pci_*.c pci_*.h
