include config.mak

LIBNAME = libao2.a

SRCS=afmt.c audio_out.c ao_mpegpes.c ao_null.c ao_pcm.c ao_plugin.c pl_delay.c pl_format.c pl_surround.c remez.c pl_resample.c pl_volume.c pl_extrastereo.c pl_volnorm.c pl_eq.c $(OPTIONAL_SRCS)

OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. $(ARTS_INC) $(ESD_INC) $(JACK_INC) $(SDL_INC) $(X11_INC) $(EXTRA_INC) $(DXR2_INC) $(DVB_INC) $(POLYP_INC)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

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
