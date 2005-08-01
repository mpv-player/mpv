
LIBNAME = libmpdemux.a

include ../config.mak

# Headers and header-dumpers
SRCS  = mp3_hdr.c \
        mpeg_hdr.c \
        asfheader.c \
        aviheader.c \
        aviprint.c \

# Core
SRCS += cache2.c \
        extension.c \
        mf.c \
        open.c \
        url.c \
        video.c \

# Miscellaneous
SRCS += cdda.c \
        cddb.c \
        cdinfo.c \
        cue_read.c \
        dvdnav_stream.c \
        parse_es.c \
        parse_mp4.c \
        yuv4mpeg.c \
        yuv4mpeg_ratio.c \

# Stream readers/writers
SRCS += stream.c \
        stream_dvd.c \
        stream_file.c \
        stream_ftp.c \
        stream_livedotcom.c \
        stream_netstream.c \
        stream_null.c \
        stream_smb.c \
        stream_vcd.c \
        stream_vstream.c \

# TV in
SRCS += tv.c \
        frequencies.c \
        tvi_bsdbt848.c \
        tvi_dummy.c \
        tvi_v4l2.c \
        tvi_v4l.c \

# Audio in
SRCS += audio_in.c \
        ai_alsa1x.c \
        ai_alsa.c \
        ai_oss.c \

# Demuxers
SRCS += demuxer.c \
        demux_aac.c \
        demux_asf.c \
        demux_audio.c \
        demux_avi.c \
        demux_demuxers.c \
        demux_film.c \
        demux_fli.c \
        demux_gif.c \
        demux_lavf.c \
        demux_lmlm4.c \
        demux_mf.c \
        demux_mov.c \
        demux_mpc.c \
        demux_mpg.c \
        demux_nsv.c \
        demux_nuv.c \
        demux_ogg.c \
        demux_pva.c \
        demux_rawaudio.c \
        demux_rawdv.c \
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

ifeq ($(XMMS_PLUGINS),yes)
SRCS += demux_xmms.c
endif 
ifneq ($(W32_LIB),)
SRCS += demux_avs.c
endif
ifeq ($(MATROSKA),yes)
SRCS += demux_mkv.c ebml.c
endif

# Muxers
SRCS += muxer.c \
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
SRCS += muxer_lavf.c
endif

ifeq ($(MPLAYER_NETWORK),yes)
SRCS += asf_streaming.c \
        http.c \
        network.c \
        cookies.c \
        asf_mmst_streaming.c \
        pnm.c \
        rtp.c \

SRCS += realrtsp/asmrp.c \
        realrtsp/real.c \
        realrtsp/rmff.c \
        realrtsp/rtsp.c \
        realrtsp/rtsp_session.c \
        realrtsp/sdpplin.c \
        realrtsp/xbuffer.c \

ifeq ($(STREAMING_LIVE_DOT_COM),yes)
CPLUSPLUSSRCS = demux_rtp.cpp demux_rtp_codec.cpp
CPLUSPLUSINCLUDE = $(LIVE_INCLUDES)
endif
endif

ifeq ($(DVBIN),yes)
SRCS += dvbin.c
SRCS += dvb_tune.c
endif

OBJS	= $(SRCS:.c=.o)
OBJS   += $(CPLUSPLUSSRCS:.cpp=.o)
INCLUDE = -I.. -I../loader $(CSS_INC) $(EXTRA_INC) $(LIBAV_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE) $(XMMS_CFLAGS) $(CDPARANOIA_INC) $(DVB_INC)
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
	$(RANLIB) $(LIBNAME)

test:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../mp_msg.c ../osdep/shmem.c -o test ./libmpdemux.a ../libmpdvdkit2/libmpdvdkit.a ../libvo/aclib.o ../libmpcodecs/img_format.o ../libao2/afmt.o ../sub_cc.o ../m_option.o ../subreader.o $(ALSA_LIB) $(VORBIS_LIB) $(CDPARANOIA_LIB) -lz -lpthread

clean:
	rm -f *.o *.a *~ realrtsp/*.o realrtsp/*.a realrtsp/*~

distclean:
	rm -f test Makefile.bak *.o *.a *~ realrtsp/*.o realrtsp/*.a realrtsp/*~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
