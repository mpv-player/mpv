include ../config.mak

LIBNAME_COMMON  = libosd.a
LIBNAME_MPLAYER = libvo.a

SRCS_MPLAYER = aspect.c \
               geometry.c \
               spuenc.c \
               video_out.c \
               vo_mpegpes.c \
               vo_null.c \
               vo_yuv4mpeg.c \
               $(VO_SRCS) \

SRCS_MPLAYER-$(EXTERNAL_VIDIX) +=  vosub_vidix.c
SRCS_MPLAYER-$(VIDIX)          +=  vosub_vidix.c

SRCS_COMMON = aclib.c \
              osd.c \
              sub.c \

SRCS_COMMON-$(BITMAP_FONT) +=  font_load.c
SRCS_COMMON-$(FREETYPE)    +=  font_load_ft.c

include ../mpcommon.mak
