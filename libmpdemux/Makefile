
LIBNAME = libmpdemux.a

include ../config.mak

# Headers and header-dumpers
SRCS  = mp3_hdr.c \
        mpeg_hdr.c \
        asfheader.c \
        aviheader.c \
        aviprint.c \

# Core
SRCS += extension.c \
        mf.c \
        open.c \
        url.c \
        video.c \

ifeq ($(STREAM_CACHE),yes)
SRCS += cache2.c
endif

# Miscellaneous
SRCS += cdinfo.c \
        cue_read.c \
        parse_es.c \
        parse_mp4.c \
        yuv4mpeg.c \
        yuv4mpeg_ratio.c \

ifeq ($(CDDA),yes)
SRCS += cdda.c
  ifeq ($(MPLAYER_NETWORK),yes)
  SRCS += cddb.c
  endif
endif

# Stream readers/writers
SRCS += stream.c \
        stream_file.c \
        stream_null.c \

ifeq ($(HAVE_DVD),yes)
SRCS += stream_dvd.c
endif
ifeq ($(DVDNAV),yes)
SRCS += dvdnav_stream.c
endif
ifeq ($(VCD),yes)
SRCS += stream_vcd.c
endif
ifeq ($(FTP),yes)
SRCS += stream_ftp.c
endif
ifeq ($(LIBSMBCLIENT),yes)
SRCS += stream_smb.c
endif
ifeq ($(MPLAYER_NETWORK),yes)
  SRCS += stream_netstream.c
  ifeq ($(STREAMING_LIVE555),yes)
  SRCS += stream_livedotcom.c
  endif
endif
ifeq ($(VSTREAM),yes)
SRCS += stream_vstream.c
endif

# TV in
ifeq ($(TV),yes)
SRCS += tv.c frequencies.c tvi_dummy.c
  ifeq ($(TV_BSDBT848),yes)
  SRCS += tvi_bsdbt848.c
  endif
  ifeq ($(TV_V4L2),yes)
  SRCS += tvi_v4l2.c audio_in.c
  endif
  ifeq ($(TV_V4L),yes)
  SRCS += tvi_v4l.c audio_in.c
  endif
  ifneq ($(TV_V4L)$(TV_V4L2),)
    ifeq ($(ALSA1X),yes)
     SRCS += ai_alsa1x.c
    endif
    ifeq ($(ALSA9),yes)
     SRCS += ai_alsa.c
    endif
    ifeq ($(OSS),yes)
     SRCS += ai_oss.c
    endif
  endif
endif

# Demuxers
SRCS += demuxer.c \
        demux_aac.c \
        demux_asf.c \
        demux_audio.c \
        demux_avi.c \
        demux_demuxers.c \
        demux_film.c \
        demux_fli.c \
        demux_lmlm4.c \
        demux_mf.c \
        demux_mov.c \
        demux_mpc.c \
        demux_mpg.c \
        demux_nsv.c \
        demux_nuv.c \
        demux_pva.c \
        demux_rawaudio.c \
        demux_rawvideo.c \
        demux_realaud.c \
        demux_real.c \
        demux_roq.c \
        demux_smjpeg.c \
        demux_ts.c \
        demux_ty.c \
        demux_ty_osd.c \
        demux_viv.c \
        demux_vqf.c \
        demux_y4m.c \
        demux_mkv.c ebml.c \

ifeq ($(LIBVORBIS),yes)
SRCS += demux_ogg.c
endif
ifeq ($(LIBDV),yes)
SRCS += demux_rawdv.c
endif
ifeq ($(GIF),yes)
SRCS += demux_gif.c
endif
ifeq ($(XMMS_PLUGINS),yes)
SRCS += demux_xmms.c
endif
ifneq ($(W32_LIB),)
SRCS += demux_avs.c
endif

# Muxers
MUXERS = muxer.c \
        muxer_avi.c \
        muxer_mpeg.c \
        muxer_rawaudio.c \
        muxer_rawvideo.c \

LIBAV_INC =
ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I../libavutil
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
LIBAV_INC += -I../libavcodec
endif
ifeq ($(CONFIG_LIBAVFORMAT),yes)
LIBAV_INC += -I../libavformat
SRCS   += demux_lavf.c
MUXERS += muxer_lavf.c
endif
ifeq ($(CONFIG_LIBAVFORMAT_SO),yes)
SRCS   += demux_lavf.c
MUXERS += muxer_lavf.c
endif

ifeq ($(MPLAYER_NETWORK),yes)
SRCS += asf_streaming.c \
        http.c \
        network.c \
        cookies.c \
        asf_mmst_streaming.c \
        pnm.c \
        rtp.c \
        rtsp.c \

SRCS += realrtsp/asmrp.c \
        realrtsp/real.c \
        realrtsp/rmff.c \
        realrtsp/sdpplin.c \
        realrtsp/xbuffer.c \

SRCS += librtsp/rtsp.c \
        librtsp/rtsp_rtp.c \
        librtsp/rtsp_session.c \

SRCS += freesdp/common.c \
        freesdp/errorlist.c \
        freesdp/parser.c \

ifeq ($(STREAMING_LIVE555),yes)
CPLUSPLUSSRCS = demux_rtp.cpp demux_rtp_codec.cpp
CPLUSPLUSINCLUDE = $(LIVE_INCLUDES)
endif
endif

ifeq ($(DVBIN),yes)
SRCS += dvbin.c
SRCS += dvb_tune.c
endif

ifeq ($(MENCODER),yes)
SRCS += $(MUXERS)
endif

OBJS	= $(SRCS:.c=.o)
OBJS   += $(CPLUSPLUSSRCS:.cpp=.o)
INCLUDE = -I.. -I../loader $(CSS_INC) $(EXTRA_INC) $(LIBAV_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE) $(XMMS_CFLAGS) $(CDPARANOIA_INC) $(DVB_INC)
CPLUSPLUSFLAGS  = $(CFLAGS) $(CPLUSPLUSINCLUDE) -D__STDC_LIMIT_MACROS
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
	$(RANLIB) $(LIBNAME)

test:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../mp_msg.c ../osdep/shmem.c -o test \
	  ./libmpdemux.a ../libmpdvdkit2/libmpdvdkit.a ../libvo/aclib.o \
	  ../libmpcodecs/img_format.o ../libao2/afmt.o ../sub_cc.o \
	  ../m_option.o ../subreader.o \
	  $(ALSA_LIB) $(VORBIS_LIB) $(CDPARANOIA_LIB) -lz -lpthread

clean:
	rm -f *.o *.a *~ \
	realrtsp/*.o realrtsp/*.a realrtsp/*~ \
	librtsp/*.o librtsp/*.a librtsp/*~ \
	freesdp/*.o freesdp/*.a freesdp/*~

distclean: clean
	rm -f .depend test

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
