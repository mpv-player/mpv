# makefile

include ../../config.mak

BINDIR = $(LIBDIR)/mplayer/vidix


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

MACH64_VID=mach64_vid.so
MACH64_SRCS=mach64_vid.c
MACH64_OBJS=mach64_vid.o
MACH64_LIBS=-L../../libdha -ldha
MACH64_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W -DRAGE128

NVIDIA_VID=nvidia_vid.so
NVIDIA_SRCS=nvidia_vid.c
NVIDIA_OBJS=nvidia_vid.o
NVIDIA_LIBS=-L../../libdha -ldha -lm
NVIDIA_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W

GENFB_VID=genfb_vid.so
GENFB_SRCS=genfb_vid.c
GENFB_OBJS=genfb_vid.o
GENFB_LIBS=-L../../libdha -ldha -lm
GENFB_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W

MGA_VID=mga_vid.so
MGA_SRCS=mga_vid.c
MGA_OBJS=mga_vid.o
MGA_LIBS=-L../../libdha -ldha -lm
MGA_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W

MGA_CRTC2_VID=mga_crtc2_vid.so
MGA_CRTC2_SRCS=mga_vid.c
MGA_CRTC2_OBJS=mga_crtc2_vid.o
MGA_CRTC2_LIBS=-L../../libdha -ldha -lm
MGA_CRTC2_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -Wall -W -DCRTC2

all:    $(RADEON_VID) $(RAGE128_VID) $(MACH64_VID) $(NVIDIA_VID) $(GENFB_VID) $(MGA_VID) $(MGA_CRTC2_VID)


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

$(MACH64_OBJS):    $(MACH64_SRCS)
	$(CC) -c $(MACH64_CFLAGS) -o $@ $<

$(MACH64_VID):     $(MACH64_OBJS)
	$(LD) $(MACH64_LIBS) -shared -soname $(MACH64_VID) -o $(MACH64_VID) $(MACH64_OBJS)

$(NVIDIA_OBJS):    $(NVIDIA_SRCS)
	$(CC) -c $(NVIDIA_CFLAGS) -o $@ $<

$(NVIDIA_VID):     $(NVIDIA_OBJS)
	$(LD) $(NVIDIA_LIBS) -shared -soname $(NVIDIA_VID) -o $(NVIDIA_VID) $(NVIDIA_OBJS)

$(GENFB_OBJS):    $(GENFB_SRCS)
	$(CC) -c $(GENFB_CFLAGS) -o $@ $<

$(GENFB_VID):     $(GENFB_OBJS)
	$(LD) $(GENFB_LIBS) -shared -soname $(GENFB_VID) -o $(GENFB_VID) $(GENFB_OBJS)

$(MGA_OBJS):    $(MGA_SRCS)
	$(CC) -c $(MGA_CFLAGS) -o $@ $<

$(MGA_VID):     $(MGA_OBJS)
	$(LD) $(MGA_LIBS) -shared -soname $(MGA_VID) -o $(MGA_VID) $(MGA_OBJS)

$(MGA_CRTC2_OBJS):    $(MGA_CRTC2_SRCS)
	$(CC) -c $(MGA_CRTC2_CFLAGS) -o $@ $<

$(MGA_CRTC2_VID):     $(MGA_CRTC2_OBJS)
	$(LD) $(MGA_CRTC2_LIBS) -shared -soname $(MGA_CRTC2_VID) -o $(MGA_CRTC2_VID) $(MGA_CRTC2_OBJS)

clean:
	rm -f *.o *.so *~

distclean:
	rm -f Makefile.bak *.o *.so test *~ .depend

dep:    depend

depend:
	echo "depend not supported"

install:
	mkdir -p $(BINDIR)
	install -m 755 -s -p *.so $(BINDIR)
#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
