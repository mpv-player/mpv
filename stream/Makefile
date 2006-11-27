
LIBNAME = stream.a

include ../config.mak

# Core
SRCS += open.c \
        url.c \

ifeq ($(STREAM_CACHE),yes)
SRCS += cache2.c
endif

ifeq ($(CDDA),yes)
SRCS += stream_cdda.c cdinfo.c
  ifeq ($(MPLAYER_NETWORK),yes)
  SRCS += stream_cddb.c
  endif
endif

# Stream readers/writers
SRCS += stream.c \
        stream_cue.c \
        stream_file.c \
        stream_mf.c \
        stream_null.c \

ifeq ($(DVDREAD),yes)
SRCS += stream_dvd.c
endif
ifeq ($(DVDNAV),yes)
SRCS += stream_dvdnav.c
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
endif
ifeq ($(STREAMING_LIVE555),yes)
SRCS += stream_livedotcom.c
endif
ifeq ($(VSTREAM),yes)
SRCS += stream_vstream.c
endif

# TV in
ifeq ($(TV),yes)
SRCS += stream_tv.c tv.c frequencies.c tvi_dummy.c
endif
ifeq ($(TV_BSDBT848),yes)
SRCS += tvi_bsdbt848.c
endif
ifeq ($(TV_V4L2),yes)
SRCS += tvi_v4l2.c audio_in.c
endif
ifeq ($(PVR),yes)
SRCS += stream_pvr.c
endif
ifeq ($(TV_V4L1),yes)
SRCS += tvi_v4l.c audio_in.c
endif
ifeq ($(TV_V4L),yes)
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

# Radio in
ifeq ($(RADIO),yes)
SRCS += stream_radio.c
endif
ifeq ($(RADIO_CAPTURE),yes)
SRCS += audio_in.c
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

ifeq ($(MPLAYER_NETWORK),yes)
SRCS += asf_streaming.c \
        http.c \
        network.c \
        cookies.c \
        asf_mmst_streaming.c \
        pnm.c \
        rtp.c \
        udp.c \
        tcp.c \
        stream_rtsp.c \
        stream_rtp.c \
        stream_udp.c \

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

endif

ifeq ($(DVBIN),yes)
SRCS += stream_dvb.c
SRCS += dvb_tune.c
endif

CFLAGS = -I../loader

include ../mpcommon.mak

clean::
	rm -f realrtsp/*.o realrtsp/*.a realrtsp/*~ \
	librtsp/*.o librtsp/*.a librtsp/*~ \
	freesdp/*.o freesdp/*.a freesdp/*~
