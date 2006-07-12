include ../config.mak

LIBNAME = libao2.a

SRCS=audio_out.c \
     ao_mpegpes.c \
     ao_null.c \
     ao_pcm.c \
     $(AO_SRCS) \

OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. \
          $(ARTS_INC) \
          $(DVB_INC) \
          $(DXR2_INC) \
          $(ESD_INC) \
          $(JACK_INC) \
          $(OPENAL_INC) \
          $(POLYP_INC) \
          $(SDL_INC) \
          $(X11_INC) \

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

distclean: clean
	rm -f .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
