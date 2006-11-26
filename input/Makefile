
include ../config.mak

LIBNAME = libinput.a

SRCS=input.c
ifeq ($(JOYSTICK),yes)
SRCS += joystick.c
endif
ifeq ($(LIRC),yes)
SRCS += lirc.c
endif

include ../mpcommon.mak
