
include ../config.mak

LIBNAME = libvo.a
LIBNAME2 = libosd.a

SRCS=aspect.c \
     geometry.c \
     spuenc.c \
     video_out.c \
     vo_mpegpes.c \
     vo_null.c \
     vo_yuv4mpeg.c \
     $(VO_SRCS) \

SRCS2 = aclib.c \
        osd.c \
        sub.c \

ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I../libavutil
endif

OBJS_TEMP=$(basename $(SRCS))
OBJS=$(OBJS_TEMP:%=%.o)

OBJS2=$(SRCS2:.c=.o)

ifeq ($(BITMAP_FONT),yes)
SRCS2 += font_load.c
endif
ifeq ($(FREETYPE),yes)
SRCS2 += font_load_ft.c
endif

ifeq ($(VIDIX),yes)
SRCS += vosub_vidix.c
endif

ifeq ($(EXTERNAL_VIDIX),yes)
SRCS += vosub_vidix.c
endif

INCLUDE = -I. -I.. -I../osdep $(LIBAV_INC)
CFLAGS  = $(INCLUDE) $(OPTFLAGS)

#CFLAGS += -Wall

.SUFFIXES: .c .o .m

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.m.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:    $(LIBNAME) $(LIBNAME2)

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

$(LIBNAME2):     $(OBJS2)
	$(AR) r $(LIBNAME2) $(OBJS2)
	$(RANLIB) $(LIBNAME2)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

ifneq ($(wildcard .depend),)
include .depend
endif
