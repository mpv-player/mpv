include ../config.mak

LIBNAME_MPLAYER = libvidix.a

SRCS_MPLAYER    = vidixlib.c \
                  drivers.c \
                  dha.c \
                  mtrr.c \
                  pci.c \
                  pci_names.c \
                  pci_dev_ids.c\

SRCS_MPLAYER-$(VIDIX_CYBERBLADE)           += cyberblade_vid.c
SRCS_MPLAYER-$(VIDIX_IVTV)                 += ivtv_vid.c
SRCS_MPLAYER-$(VIDIX_MACH64)               += mach64_vid.c
SRCS_MPLAYER-$(VIDIX_MGA)                  += mga_vid.c
SRCS_MPLAYER-$(VIDIX_NVIDIA)               += nvidia_vid.c
SRCS_MPLAYER-$(VIDIX_PM2)                  += pm2_vid.c
SRCS_MPLAYER-$(VIDIX_PM3)                  += pm3_vid.c
SRCS_MPLAYER-$(VIDIX_RADEON)               += radeon_vid.c
SRCS_MPLAYER-$(VIDIX_SAVAGE)               += savage_vid.c
SRCS_MPLAYER-$(VIDIX_SIS)                  += sis_vid.c sis_bridge.c
SRCS_MPLAYER-$(VIDIX_UNICHROME)            += unichrome_vid.c

ifeq ($(VIDIX_MGA_CRTC2), yes)
OBJS_MPLAYER += mga_crtc2_vid.o
endif
ifeq ($(VIDIX_RAGE128),yes)
OBJS_MPLAYER += rage128_vid.o
endif

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
	LC_ALL=C awk -f pci_db2c.awk $< $(VIDIX_PCIDB)

clean::
	rm -f pci_*.c pci_*.h
