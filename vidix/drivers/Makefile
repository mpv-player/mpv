# makefile

include ../../config.mak

BINDIR = $(prefix)/lib/mplayer/vidix


RADEON_VID=radeon_vid.so
RADEON_SRCS=radeon_vid.c
RADEON_OBJS=radeon_vid.o
RADEON_LIBS=-L../../libdha -ldha -lm
RADEON_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W

RAGE128_VID=rage128_vid.so
RAGE128_SRCS=radeon_vid.c
RAGE128_OBJS=rage128_vid.o
RAGE128_LIBS=-L../../libdha -ldha
RAGE128_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W -DRAGE128

all:    $(RADEON_VID) $(RAGE128_VID)


.SUFFIXES: .c .o

# .PHONY: all clean

$(RADEON_OBJS):    $(RADEON_SRCS)
	$(CC) -c $(RADEON_CFLAGS) -o $@ $<

$(RADEON_VID):     $(RADEON_OBJS)
	$(LD) $(RADEON_LIBS) -shared -soname $(RADEON_VID) -o $(RADEON_VID) $(RADEON_OBJS)

$(RAGE128_OBJS):    $(RAGE128_SRCS)
	$(CC) -c $(RAGE128_CFLAGS) -o $@ $<

$(RAGE128_VID):     $(RAGE128_OBJS)
	$(LD) $(RAGE128_LIBS) -shared -soname $(RAGE128_VID) -o $(RAGE128_VID) $(RAGE128_OBJS)

clean:
	rm -f *.o *.so *~

distclean:
	rm -f Makefile.bak *.o *.so test *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

install:
	mkdir -p $(BINDIR)
	cp *.so $(BINDIR)
#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
