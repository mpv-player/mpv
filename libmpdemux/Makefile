
LIBNAME = libmpdemux.a

include ../config.mak

SRCS = mp3_hdr.c video.c mpeg_hdr.c cache2.c asfheader.c aviheader.c aviprint.c aviwrite.c demux_asf.c demux_avi.c demux_mov.c parse_mp4.c demux_mpg.c demux_pva.c demux_viv.c demuxer.c dvdauth.c dvdnav_stream.c open.c parse_es.c stream.c tv.c tvi_dummy.c tvi_v4l.c tvi_bsdbt848.c frequencies.c demux_fli.c demux_real.c demux_y4m.c yuv4mpeg.c yuv4mpeg_ratio.c demux_nuv.c demux_film.c demux_roq.c mf.c demux_mf.c demux_audio.c demux_demuxers.c opt-reg.c mpdemux.c demux_ogg.c demux_bmp.c cdda.c demux_rawaudio.c cddb.c demux_rawdv.c ai_alsa.c ai_oss.c audio_in.c
ifeq ($(STREAMING),yes)
SRCS += asf_streaming.c url.c http.c network.c asf_mmst_streaming.c
ifeq ($(STREAMING_LIVE_DOT_COM),yes)
CPLUSPLUSSRCS = demux_rtp.cpp
CPLUSPLUSINCLUDE = -I$(LIVE_LIB_DIR)/liveMedia/include
CPLUSPLUSINCLUDE += -I$(LIVE_LIB_DIR)/UsageEnvironment/include
CPLUSPLUSINCLUDE += -I$(LIVE_LIB_DIR)/BasicUsageEnvironment/include
CPLUSPLUSINCLUDE += -I$(LIVE_LIB_DIR)/groupsock/include
else
SRCS += rtp.c
endif
endif

OBJS	= $(SRCS:.c=.o)
OBJS   += $(CPLUSPLUSSRCS:.cpp=.o)
INCLUDE = -I../loader $(CSS_INC) $(EXTRA_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE)
CPLUSPLUSFLAGS  = $(CFLAGS) $(CPLUSPLUSINCLUDE)
CPLUSPLUS = $(CC)

.SUFFIXES: .c .cpp .o

# .PHONY: all clean

all:	$(LIBNAME)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CPLUSPLUS) -c $(CPLUSPLUSFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../mp_msg.c ../linux/shmem.c -o test ./libmpdemux.a -ldvdread -lz -lpthread

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
