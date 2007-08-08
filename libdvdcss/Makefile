include ../config.mak

LIBNAME_COMMON = libdvdcss.a

SRCS_COMMON = css.c \
              device.c \
              error.c \
              ioctl.c \
              libdvdcss.c \
              #bsdi_ioctl \

CFLAGS = -D__USE_UNIX98 -D_GNU_SOURCE -DVERSION=\"1.2.9\"

include ../mpcommon.mak
