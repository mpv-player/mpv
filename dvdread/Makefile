include ../config.mak

SRCS = dvd_input.c \
       dvd_reader.c \
       dvd_udf.c \
       ifo_print.c \
       ifo_read.c \
       nav_print.c \
       nav_read.c \
       md5.c \

LIBNAME = libdvdread.a

CFLAGS= -D__USE_UNIX98 -D_GNU_SOURCE -DSTDC_HEADERS \
	-DHAVE_LIMITS_H -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

ifeq ($(DVDCSS_INTERNAL),yes)
CFLAGS += -I../libdvdcss -DHAVE_DVDCSS_DVDCSS_H
endif

include ../mpcommon.mak
