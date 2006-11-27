
LIBNAME = stream.a

include ../config.mak

SRCS += open.c \
        stream.c \
        stream_cue.c \
        stream_file.c \
        stream_mf.c \
        stream_null.c \
        url.c \

SRCS-$(CDDA)              += stream_cdda.c cdinfo.c
SRCS-$(CDDB)              += stream_cddb.c
SRCS-$(DVBIN)             += dvb_tune.c stream_dvb.c
SRCS-$(DVDNAV)            += stream_dvdnav.c
SRCS-$(DVDREAD)           += stream_dvd.c
SRCS-$(FTP)               += stream_ftp.c
SRCS-$(LIBSMBCLIENT)      += stream_smb.c
SRCS-$(MPLAYER_NETWORK)   += stream_netstream.c     \
                             asf_mmst_streaming.c   \
                             asf_streaming.c        \
                             cookies.c              \
                             http.c                 \
                             network.c              \
                             pnm.c                  \
                             rtp.c                  \
                             udp.c                  \
                             tcp.c                  \
                             stream_rtp.c           \
                             stream_rtsp.c          \
                             stream_udp.c           \
                             freesdp/common.c       \
                             freesdp/errorlist.c    \
                             freesdp/parser.c       \
                             librtsp/rtsp.c         \
                             librtsp/rtsp_rtp.c     \
                             librtsp/rtsp_session.c \
                             realrtsp/asmrp.c       \
                             realrtsp/real.c        \
                             realrtsp/rmff.c        \
                             realrtsp/sdpplin.c     \
                             realrtsp/xbuffer.c     \

SRCS-$(PVR)               += stream_pvr.c
SRCS-$(RADIO)             += stream_radio.c
SRCS-$(RADIO_CAPTURE)     += audio_in.c
SRCS-$(STREAM_CACHE)      += cache2.c
SRCS-$(STREAMING_LIVE555) += stream_livedotcom.c
SRCS-$(TV)                += stream_tv.c tv.c frequencies.c tvi_dummy.c
SRCS-$(TV_BSDBT848)       += tvi_bsdbt848.c
SRCS-$(TV_V4L1)           += tvi_v4l.c  audio_in.c
SRCS-$(TV_V4L2)           += tvi_v4l2.c audio_in.c
SRCS-$(VCD)               += stream_vcd.c
SRCS-$(VSTREAM)           += stream_vstream.c

#FIXME: These should have variables assigned in configure.
ifeq ($(TV_V4L),yes)
SRCS-$(ALSA1X)            += ai_alsa1x.c
SRCS-$(ALSA9)             += ai_alsa.c
SRCS-$(OSS)               += ai_oss.c
endif
ifeq ($(RADIO_CAPTURE),yes)
SRCS-$(ALSA1X)            += ai_alsa1x.c
SRCS-$(ALSA9)             += ai_alsa.c
SRCS-$(OSS)               += ai_oss.c
endif

CFLAGS = -I../loader

include ../mpcommon.mak

clean::
	rm -f realrtsp/*.o realrtsp/*.a realrtsp/*~ \
	librtsp/*.o librtsp/*.a librtsp/*~ \
	freesdp/*.o freesdp/*.a freesdp/*~
