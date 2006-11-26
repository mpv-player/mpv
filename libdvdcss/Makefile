include ../config.mak

SRCS = css.c \
       device.c \
       error.c \
       ioctl.c \
       libdvdcss.c \
       #bsdi_ioctl \

LIBNAME = libdvdcss.a

CFLAGS= -D__USE_UNIX98 -D_GNU_SOURCE \
	-DHAVE_LIMITS_H -DHAVE_ERRNO_H -DHAVE_INTTYPES_H -DHAVE_UNISTD_H \
	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DVERSION=\"1.2.9\"

ifeq ($(TARGET_OS),CYGWIN)
CFLAGS+=-DSYS_CYGWIN
endif

ifeq ($(TARGET_OS),Darwin)
CFLAGS+=-D__DARWIN__
endif

include ../mpcommon.mak
