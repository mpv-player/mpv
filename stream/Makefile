include ../config.mak

LIBNAME_COMMON = stream.a

SRCS_COMMON = open.c \
              stream.c \
              stream_cue.c \
              stream_file.c \
              stream_mf.c \
              stream_null.c \
              url.c \

SRCS_COMMON-$(CDDA)              += stream_cdda.c cdinfo.c
SRCS_COMMON-$(CDDB)              += stream_cddb.c
SRCS_COMMON-$(DVBIN)             += dvb_tune.c stream_dvb.c
SRCS_COMMON-$(DVDNAV)            += stream_dvdnav.c
SRCS_COMMON-$(DVDREAD)           += stream_dvd.c
SRCS_COMMON-$(FTP)               += stream_ftp.c
SRCS_COMMON-$(LIBSMBCLIENT)      += stream_smb.c
SRCS_COMMON-$(MPLAYER_NETWORK)   += stream_netstream.c     \
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

SRCS_COMMON-$(PVR)               += stream_pvr.c
SRCS_COMMON-$(RADIO)             += stream_radio.c
SRCS_COMMON-$(RADIO_CAPTURE)     += audio_in.c
SRCS_COMMON-$(STREAM_CACHE)      += cache2.c
SRCS_COMMON-$(STREAMING_LIVE555) += stream_livedotcom.c
SRCS_COMMON-$(TV)                += stream_tv.c tv.c frequencies.c tvi_dummy.c
SRCS_COMMON-$(TV_BSDBT848)       += tvi_bsdbt848.c
SRCS_COMMON-$(TV_V4L1)           += tvi_v4l.c  audio_in.c
SRCS_COMMON-$(TV_V4L2)           += tvi_v4l2.c audio_in.c
SRCS_COMMON-$(VCD)               += stream_vcd.c
SRCS_COMMON-$(VSTREAM)           += stream_vstream.c

#FIXME: These should have variables assigned in configure.
ifeq ($(TV_V4L),yes)
SRCS_COMMON-$(ALSA1X)            += ai_alsa1x.c
SRCS_COMMON-$(ALSA9)             += ai_alsa.c
SRCS_COMMON-$(OSS)               += ai_oss.c
endif
ifeq ($(RADIO_CAPTURE),yes)
SRCS_COMMON-$(ALSA1X)            += ai_alsa1x.c
SRCS_COMMON-$(ALSA9)             += ai_alsa.c
SRCS_COMMON-$(OSS)               += ai_oss.c
endif

include ../mpcommon.mak

clean::
	rm -f realrtsp/*.o realrtsp/*.a realrtsp/*~ \
	librtsp/*.o librtsp/*.a librtsp/*~ \
	freesdp/*.o freesdp/*.a freesdp/*~
