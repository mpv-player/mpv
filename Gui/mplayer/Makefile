
include ../../config.mak
include ../config.mak

include ../bitmap/bitmap.mak
include ../gui.mak

LIB = mplayer

INCDIR  = -I. -I../event -I../wm -I../skin $(GTKINC)

OPTIMIZE =  $(OPTFLAGS) -fomit-frame-pointer \
	    -fexpensive-optimizations -fschedule-insns2 -Wall

CFLAGS = $(OPTIMIZE) $(INCDIR) $(DEBUG)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

### TARGETS ###

default:	.depend	$(MPLAYEROBJS)

all:	default

clean:
	rm -rf $(MPLAYEROBJS) *.o *~ *.bak .depend

distclean: clean

dep:    depend

depend: .depend

.depend: Makefile ../config.mak ../gui.mak ../bitmap/bitmap.mak
	$(CC) -MM $(CFLAGS) $(MPLAYERSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
