
include ../config.mak

LIBNAME = libvo.a
LIBNAME2 = libosd.a

LIBS =$(LIBNAME2)
ifeq ($(MPLAYER),yes)
LIBS+=$(LIBNAME)
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

OBJS_TEMP=$(basename $(SRCS))
OBJS=$(OBJS_TEMP:%=%.o)

OBJS2=$(SRCS2:.c=.o)

LIBAV_INC-$(CONFIG_LIBAVUTIL) +=  -I../libavutil

SRCS         += $(SRCS-yes)
SRCS2        += $(SRCS2-yes)
LIBAV_INC    += $(LIBAV_INC-yes)

CFLAGS = -I.. -I../osdep $(LIBAV_INC)

include ../mpcommon.mak

.m.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:    $(LIBS)

$(LIBNAME2): $(OBJS2)
	$(AR) r $@ $^
	$(RANLIB) $@
