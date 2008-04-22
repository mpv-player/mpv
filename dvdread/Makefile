include ../config.mak

LIBNAME_COMMON = dvdread.a

SRCS_COMMON = cmd_print.c \
              dvd_input.c \
              dvd_reader.c \
              dvd_udf.c \
              ifo_print.c \
              ifo_read.c \
              md5.c \
              nav_print.c \
              nav_read.c \

CFLAGS = -D__USE_UNIX98 -D_GNU_SOURCE

CFLAGS-$(DVDCSS_INTERNAL) += -I../libdvdcss -DHAVE_DVDCSS_DVDCSS_H

include ../mpcommon.mak
