
include config.mak

LIBNAME = libvo.a

SRCS=aspect.c aclib.c osd.c font_load.c yuv2rgb.c video_out.c vo_null.c vo_pgm.c vo_md5.c vo_mpegpes.c x11_common.c $(OPTIONAL_SRCS) img_format.c
OBJS=$(SRCS:.c=.o)

ifeq ($(TARGET_ARCH_X86),yes)
SRCS += yuv2rgb_mmx.c
ifeq ($(TARGET_OS),Linux)
SRCS += vo_vesa.c
endif
endif

CFLAGS  = $(OPTFLAGS) -I. -I.. $(SDL_INC) $(X11_INC) $(EXTRA_INC) $(MLIB_INC) -DMPG12PLAY #-Wall
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
