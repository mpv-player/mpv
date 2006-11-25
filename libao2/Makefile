include ../config.mak

LIBNAME = libao2.a

SRCS=audio_out.c \
     ao_mpegpes.c \
     ao_null.c \
     ao_pcm.c \
     $(AO_SRCS) \

OBJS=$(SRCS:.c=.o)

CFLAGS  = -I. -I.. $(OPTFLAGS)

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

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

ifneq ($(wildcard .depend),)
include .depend
endif
