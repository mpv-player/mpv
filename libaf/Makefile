include config.mak

LIBNAME = libaf.a

SRCS=af.c af_mp.c af_dummy.c af_delay.c af_channels.c af_format.c af_resample.c \
window.c filter.c af_volume.c af_equalizer.c af_tools.c af_comp.c af_gate.c \
af_pan.c af_surround.c af_sub.c af_export.c af_volnorm.c af_extrastereo.c \
af_lavcresample.c af_sweep.c af_hrtf.c $(OPTIONAL_SRCS)

OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -Wall $(EXTRA_INC)

ifeq ($(TARGET_OS),MINGW32)
CFLAGS += -D_IO_H_
endif

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS) Makefile
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

$(OBJS):af.h control.h dsp.h filter.h window.h af_mp.h

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
