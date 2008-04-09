include ../config.mak

LIBNAME_COMMON = libaf.a

SRCS_COMMON = af.c \
              af_center.c \
              af_channels.c \
              af_comp.c \
              af_delay.c \
              af_dummy.c \
              af_equalizer.c \
              af_extrastereo.c \
              af_format.c \
              af_gate.c \
              af_hrtf.c \
              af_karaoke.c \
              af_pan.c \
              af_resample.c \
              af_scaletempo.c \
              af_sinesuppress.c \
              af_sub.c \
              af_surround.c \
              af_sweep.c \
              af_tools.c \
              af_volnorm.c \
              af_volume.c \
              filter.c \
              format.c \
              reorder_ch.c \
              window.c \

SRCS_COMMON-$(HAVE_SYS_MMAN_H)      += af_export.c
SRCS_COMMON-$(LADSPA)               += af_ladspa.c
SRCS_COMMON-$(LIBAVCODEC)           += af_lavcresample.c
SRCS_COMMON-$(LIBAVCODEC_A)         += af_lavcac3enc.c

include ../mpcommon.mak
