
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

ifeq ($(LIBNUT),yes)
SRCS += demux_nut.c
endif
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
ifeq ($(WIN32DLL),yes)
SRCS += demux_avs.c
endif

# Muxers
MUXERS = muxer.c \
        muxer_avi.c \
        muxer_mpeg.c \
        muxer_rawaudio.c \
        muxer_rawvideo.c \

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

ifeq ($(MENCODER),yes)
SRCS += $(MUXERS)
endif

ifeq ($(STREAMING_LIVE555),yes)
SRCS += demux_rtp.cpp demux_rtp_codec.cpp
endif

OBJS	= $(SRCS:.c=.o)
OBJS   += $(SRCS:.cpp=.o)
INCLUDE = -I.. -I../stream -I../loader $(LIBAV_INC)
CFLAGS  = $(INCLUDE) $(OPTFLAGS)
CPLUSPLUSFLAGS  = $(CFLAGS) -D__STDC_LIMIT_MACROS

.SUFFIXES: .c .cpp .o

# .PHONY: all clean

all:	$(LIBNAME)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CC) -c $(CPLUSPLUSFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

ifneq ($(wildcard .depend),)
include .depend
endif
