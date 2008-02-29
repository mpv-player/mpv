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
SRCS_MPLAYER-$(VIDIX_MGA_CRTC2)            += mga_crtc2_vid.c
SRCS_MPLAYER-$(VIDIX_NVIDIA)               += nvidia_vid.c
SRCS_MPLAYER-$(VIDIX_PM2)                  += pm2_vid.c
SRCS_MPLAYER-$(VIDIX_PM3)                  += pm3_vid.c
SRCS_MPLAYER-$(VIDIX_RADEON)               += radeon_vid.c
SRCS_MPLAYER-$(VIDIX_RAGE128)              += rage128_vid.c
SRCS_MPLAYER-$(VIDIX_S3)                   += s3_vid.c
SRCS_MPLAYER-$(VIDIX_SIS)                  += sis_vid.c sis_bridge.c
SRCS_MPLAYER-$(VIDIX_UNICHROME)            += unichrome_vid.c

# If you want libdha to use svgalib_helper for hardware access,
# uncomment this statement, and change the -I to the correct directory
# that includes svgalib_helper.o:
#CFLAGS += -DDEV_SVGA=\"/dev/svga\" -DCONFIG_SVGAHELPER -Isvgalib_helper/

PCI_FILES = pci_dev_ids.c pci_ids.h pci_names.c pci_names.h pci_vendors.h

libs: $(PCI_FILES)

dep depend:: $(PCI_FILES)

include ../mpcommon.mak

$(OBJS_MPLAYER): $(PCI_FILES)

$(PCI_FILES): pci.db
	LC_ALL=C awk -f pci_db2c.awk $< $(VIDIX_PCIDB)

clean::
	rm -f $(PCI_FILES)
