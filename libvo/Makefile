
include config.mak

LIBNAME = libvo.a

SRCS=geometry.c aspect.c aclib.c osd.c font_load.c gtf.c spuenc.c video_out.c vo_null.c vo_mpegpes.c vo_yuv4mpeg.c $(OPTIONAL_SRCS) sub.c font_load_ft.c
OBJS=$(SRCS:.c=.o)

ifeq ($(VIDIX),yes)
SRCS += vosub_vidix.c
endif

CFLAGS  = $(OPTFLAGS) -I. -I.. -I../osdep $(FREETYPE_INC) $(SDL_INC) $(X11_INC) $(EXTRA_INC) $(DXR2_INC) $(DIRECTFB_INC) -DMPG12PLAY #-Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

vo_mpegpes.o: vo_mpegpes.c
	$(CC) -c $(CFLAGS) $(DVB_INC) -o $@ $<

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
