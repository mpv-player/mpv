include ../config.mak

LIBNAME = libaf.a

SRCS=af.c af_mp.c af_dummy.c af_delay.c af_channels.c af_format.c af_resample.c window.c filter.c af_volume.c af_equalizer.c af_tools.c af_comp.c af_gate.c af_pan.c af_surround.c

OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -Wall 
.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS) Makefile
	$(AR) r $(LIBNAME) $(OBJS)

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
