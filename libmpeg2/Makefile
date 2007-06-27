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

SRCS_COMMON-$(HAVE_MMX)        += idct_mmx.c motion_comp_mmx.c
SRCS_COMMON-$(HAVE_ALTIVEC)    += motion_comp_altivec.c idct_altivec.c
SRCS_COMMON-$(HAVE_VIS)        += motion_comp_vis.c
SRCS_COMMON-$(ARCH_ALPHA) += idct_alpha.c motion_comp_alpha.c
SRCS_COMMON-$(ARCH_ARMV4L) += motion_comp_arm.c motion_comp_iwmmxt.c motion_comp_arm_s.S

include ../mpcommon.mak
