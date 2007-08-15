include ../config.mak

LIBNAME_COMMON = libdvdread.a

SRCS_COMMON = dvd_input.c \
              dvd_reader.c \
              dvd_udf.c \
              ifo_print.c \
              ifo_read.c \
              nav_print.c \
              nav_read.c \
              md5.c \
              cmd_print.c \

CFLAGS = -D__USE_UNIX98 -D_GNU_SOURCE

CFLAGS-$(DVDCSS_INTERNAL) += -I../libdvdcss -DHAVE_DVDCSS_DVDCSS_H

include ../mpcommon.mak
