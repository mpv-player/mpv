
LIBNAME = libmpeg2.a

include ../config.mak

SRCS	= alloc.c \
	  cpu_accel.c\
	  cpu_state.c \
	  decode.c \
	  header.c \
	  idct.c \
	  motion_comp.c \
	  slice.c \

SRCS-$(TARGET_MMX)        += idct_mmx.c motion_comp_mmx.c
SRCS-$(TARGET_ALTIVEC)    += motion_comp_altivec.c idct_altivec.c
SRCS-$(TARGET_VIS)        += motion_comp_vis.c
SRCS-$(TARGET_ARCH_ALPHA) += idct_alpha.c motion_comp_alpha.c

CFLAGS = -I../libvo -DMPG12PLAY

include ../mpcommon.mak
