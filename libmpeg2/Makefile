include ../config.mak

LIBNAME_COMMON = libmpeg2.a

SRCS_COMMON = alloc.c \
	      cpu_accel.c\
	      cpu_state.c \
	      decode.c \
	      header.c \
	      idct.c \
	      motion_comp.c \
	      slice.c \

SRCS_COMMON-$(TARGET_MMX)        += idct_mmx.c motion_comp_mmx.c
SRCS_COMMON-$(TARGET_ALTIVEC)    += motion_comp_altivec.c idct_altivec.c
SRCS_COMMON-$(TARGET_VIS)        += motion_comp_vis.c
SRCS_COMMON-$(TARGET_ARCH_ALPHA) += idct_alpha.c motion_comp_alpha.c

include ../mpcommon.mak
