include ../config.mak

LIBNAME_MPLAYER = libinput.a

SRCS_MPLAYER                  = input.c
SRCS_MPLAYER-$(APPLE_REMOTE) += ar.c
SRCS_MPLAYER-$(JOYSTICK)     += joystick.c
SRCS_MPLAYER-$(LIRC)         += lirc.c

include ../mpcommon.mak
