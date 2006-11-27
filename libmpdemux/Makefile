
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
        video.c \

# Miscellaneous
SRCS += parse_es.c \
        parse_mp4.c \
        mpeg_packetizer.c \
        yuv4mpeg.c \
        yuv4mpeg_ratio.c \
        mf.c \

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

SRCS-$(CONFIG_LIBAVFORMAT)      += demux_lavf.c
SRCS-$(CONFIG_LIBAVFORMAT_SO)   += demux_lavf.c
SRCS-$(GIF)                     += demux_gif.c
SRCS-$(LIBDV)                   += demux_rawdv.c
SRCS-$(LIBNUT)                  += demux_nut.c
SRCS-$(LIBVORBIS)               += demux_ogg.c
SRCS-$(STREAMING_LIVE555)       += demux_rtp.cpp demux_rtp_codec.cpp
SRCS-$(WIN32DLL)                += demux_avs.c
SRCS-$(XMMS_PLUGINS)            += demux_xmms.c

# Muxers
MUXERS = muxer.c \
        muxer_avi.c \
        muxer_mpeg.c \
        muxer_rawaudio.c \
        muxer_rawvideo.c \

MUXERS-$(CONFIG_LIBAVFORMAT)    += muxer_lavf.c
MUXERS-$(CONFIG_LIBAVFORMAT_SO) += muxer_lavf.c

SRCS-$(MENCODER) += $(MUXERS)
SRCS-$(MENCODER) += $(MUXERS-yes)

LIBAV_INC-$(CONFIG_LIBAVUTIL)   += -I../libavutil
LIBAV_INC-$(CONFIG_LIBAVCODEC)  += -I../libavcodec
LIBAV_INC-$(CONFIG_LIBAVFORMAT) += -I../libavformat

CFLAGS = -I.. -I../stream -I../loader $(LIBAV_INC)

CPLUSPLUSFLAGS  = $(CFLAGS) -D__STDC_LIMIT_MACROS

.cpp.o:
	$(CC) -c $(CPLUSPLUSFLAGS) -o $@ $<

include ../mpcommon.mak
