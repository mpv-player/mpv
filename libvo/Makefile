
include ../config.mak

LIBNAME = libvo.a

SRCS=aclib.c \
     aspect.c \
     geometry.c \
     osd.c \
     spuenc.c \
     sub.c \
     video_out.c \
     vo_mpegpes.c \
     vo_null.c \
     vo_yuv4mpeg.c \
     $(VO_SRCS) \

LIBAV_INC =
ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I../libavutil
endif

OBJS_TEMP=$(basename $(SRCS))
OBJS=$(OBJS_TEMP:%=%.o)

ifeq ($(BITMAP_FONT),yes)
SRCS += font_load.c
endif

ifeq ($(FREETYPE),yes)
SRCS += font_load_ft.c
endif

ifeq ($(VIDIX),yes)
SRCS += vosub_vidix.c
endif

ifeq ($(EXTERNAL_VIDIX),yes)
SRCS += vosub_vidix.c
endif

INCLUDE = -I. -I.. -I../osdep $(LIBAV_INC)
CFLAGS  = $(INCLUDE) $(OPTFLAGS) -DMPG12PLAY

#CFLAGS += -Wall

.SUFFIXES: .c .o .m

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.m.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
