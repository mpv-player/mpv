
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

CFLAGS = -I../libvo -DMPG12PLAY

ifeq ($(TARGET_MMX),yes)
SRCS += idct_mmx.c motion_comp_mmx.c
endif

ifeq ($(TARGET_ALTIVEC),yes)
SRCS += motion_comp_altivec.c idct_altivec.c
endif

ifeq ($(TARGET_VIS),yes)
SRCS += motion_comp_vis.c
endif

ifeq ($(TARGET_ARCH_ALPHA),yes)
SRCS += idct_alpha.c motion_comp_alpha.c
endif

include ../mpcommon.mak
