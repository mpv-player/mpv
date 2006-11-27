include ../config.mak

LIBNAME = libaf.a

SRCS=af.c \
     af_center.c \
     af_channels.c \
     af_comp.c \
     af_delay.c \
     af_dummy.c \
     af_equalizer.c \
     af_export.c \
     af_extrastereo.c \
     af_format.c \
     af_gate.c \
     af_hrtf.c \
     af_karaoke.c \
     af_pan.c \
     af_resample.c \
     af_sinesuppress.c \
     af_sub.c \
     af_surround.c \
     af_sweep.c \
     af_tools.c \
     af_volnorm.c \
     af_volume.c \
     filter.c \
     format.c \
     window.c \
     $(AF_SRCS) \

ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I../libavutil
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
LIBAV_INC += -I../libavcodec
SRCS+=af_lavcresample.c
endif
ifeq ($(CONFIG_LIBAVCODEC_SO),yes)
SRCS+=af_lavcresample.c
endif

CFLAGS  = -I.. $(LIBAV_INC) -D_GNU_SOURCE

ifeq ($(TARGET_OS),MINGW32)
CFLAGS += -D_IO_H_
endif

include ../mpcommon.mak
