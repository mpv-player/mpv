
LIBNAME = libmpdemux.a

include ../config.mak

SRCS = mp3_hdr.c video.c mpeg_hdr.c cache2.c asfheader.c aviheader.c aviprint.c aviwrite.c demux_asf.c demux_avi.c demux_mov.c parse_mp4.c demux_mpg.c demux_viv.c demuxer.c dvdauth.c dvdnav_stream.c open.c parse_es.c stream.c tv.c tvi_dummy.c tvi_v4l.c tvi_bsdbt848.c frequencies.c demux_fli.c demux_real.c demux_y4m.c yuv4mpeg.c yuv4mpeg_ratio.c demux_nuv.c demux_film.c demux_roq.c mf.c demux_mf.c demux_audio.c demux_demuxers.c opt-reg.c mpdemux.c demux_ogg.c demux_bmp.c cdda.c demux_rawaudio.c cddb.c
ifeq ($(STREAMING),yes)
SRCS += asf_streaming.c url.c http.c network.c rtp.c asf_mmst_streaming.c
endif

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I../loader $(CSS_INC) $(EXTRA_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

.SUFFIXES: .c .o

# .PHONY: all clean

all:	$(LIBNAME)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../mp_msg.c ../linux/shmem.c -o test -L. -lmpdemux -ldvdread -lz -lpthread

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
