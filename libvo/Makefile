
include config.mak

LIBNAME = libvo.a

SRCS=aspect.c aclib.c osd.c font_load.c spuenc.c video_out.c vo_null.c vo_pgm.c vo_md5.c vo_mpegpes.c x11_common.c vo_yuv4mpeg.c $(OPTIONAL_SRCS) sub.c
OBJS=$(SRCS:.c=.o)

ifeq ($(VIDIX),yes)
SRCS += vosub_vidix.c
endif

CFLAGS  = $(OPTFLAGS) -I. -I.. $(SDL_INC) $(X11_INC) $(EXTRA_INC) $(DXR2_INC) $(DVB_INC) -DMPG12PLAY #-Wall
ifeq ($(VIDIX),yes)
CFLAGS += -DVIDIX_PATH='"$(prefix)/lib/mplayer/vidix/"'
endif
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
