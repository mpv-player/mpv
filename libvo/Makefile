
include config.mak

LIBNAME = libvo.a

SRCS=osd.c font_load.c rgb15to16mmx.c yuv2rgb_mmx.c yuv2rgb.c video_out.c vo_null.c vo_pgm.c vo_md5.c vo_odivx.c x11_common.c $(OPTIONAL_SRCS)
OBJS=osd.o font_load.o rgb15to16mmx.o yuv2rgb_mmx.o yuv2rgb.o video_out.o vo_null.o vo_pgm.o vo_md5.o vo_odivx.o x11_common.o $(OPTIONAL_OBJS)

CFLAGS  = $(OPTFLAGS) -I. -I.. -DMPG12PLAY
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     .depend $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend
depend: .depend

.depend: Makefile config.mak ../config.mak ../config.h
	makedepend -f- -- $(CFLAGS) -- $(SRCS) 1>.depend 2>/dev/null

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
