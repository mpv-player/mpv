
include ../config.mak

LIBNAME2 = libosd.a
ifeq ($(MPLAYER),yes)
LIBNAME = libvo.a
endif

SRCS=aspect.c \
     geometry.c \
     spuenc.c \
     video_out.c \
     vo_mpegpes.c \
     vo_null.c \
     vo_yuv4mpeg.c \
     $(VO_SRCS) \

SRCS-$(EXTERNAL_VIDIX) +=  vosub_vidix.c
SRCS-$(VIDIX)          +=  vosub_vidix.c

SRCS2 = aclib.c \
        osd.c \
        sub.c \

SRCS2-$(BITMAP_FONT) +=  font_load.c
SRCS2-$(FREETYPE)    +=  font_load_ft.c

LIBAV_INC-$(CONFIG_LIBAVUTIL) +=  -I../libavutil

CFLAGS = -I.. -I../osdep $(LIBAV_INC)

include ../mpcommon.mak

.m.o:
	$(CC) -c $(CFLAGS) -o $@ $<
