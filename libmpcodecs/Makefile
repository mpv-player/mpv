
include ../config.mak

LIBNAME = libmpcodecs.a

SRCS=dec_video.c vd.c vd_null.c vd_cinepak.c vd_qtrpza.c vd_ffmpeg.c vd_dshow.c vd_vfw.c vd_odivx.c vd_divx4.c vd_raw.c vd_xanim.c vd_msvidc.c vd_fli.c vd_qtrle.c vd_qtsmc.c vd_roqvideo.c vd_cyuv.c vd_nuv.c vd_libmpeg2.c vd_msrle.c vd_huffyuv.c

ifeq ($(PNG),yes)
SRCS += vd_mpng.c
endif

ifeq ($(JPEG),yes)
SRCS += vd_ijpg.c
endif

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

