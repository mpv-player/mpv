
include config.mak

LIBNAME = libvo.a

SRCS=rgb15to16mmx.c yuv2rgb_mmx.c yuv2rgb.c video_out.c vo_null.c vo_pgm.c vo_md5.c vo_odivx.c $(OPTIONAL_SRCS)
OBJS=rgb15to16mmx.o yuv2rgb_mmx.o yuv2rgb.o video_out.o vo_null.o vo_pgm.o vo_md5.o vo_odivx.o $(OPTIONAL_OBJS)

CFLAGS  = $(OPTFLAGS) -DMPG12PLAY
INCLUDE = -I. -I..
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	makedepend
	rm -f Makefile.bak *.o *.a *~

dep:    depend

depend:
	makedepend -- $(CFLAGS) -- $(SRCS) &> /dev/null

# DO NOT DELETE
