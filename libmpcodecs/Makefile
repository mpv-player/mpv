
include ../config.mak

LIBNAME = libmpcodecs.a

SRCS=dec_video.c vd.c vd_null.c vd_cinepak.c vd_qtrpza.c vd_ffmpeg.c vd_dshow.c

OBJS=$(SRCS:.c=.o)

CFLAGS  = -Wall $(OPTFLAGS) -I. -I.. -I../libmpdemux -I../loader $(EXTRA_INC)

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

