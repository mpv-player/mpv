# makefile

include ../../config.mak

BINDIR = $(LIBDIR)/mplayer/vidix

CYBERBLADE_VID=cyberblade_vid.so
CYBERBLADE_SRCS=cyberblade_vid.c
CYBERBLADE_OBJS=cyberblade_vid.o
CYBERBLADE_LIBS=-L../../libdha -ldha -lm
CYBERBLADE_CFLAGS=$(OPTFLAGS) -fPIC -I. -I..

RADEON_VID=radeon_vid.so
RADEON_SRCS=radeon_vid.c
RADEON_OBJS=radeon_vid.o
RADEON_LIBS=-L../../libdha -ldha -lm
RADEON_CFLAGS=$(OPTFLAGS) -fPIC -I. -I..

RAGE128_VID=rage128_vid.so
RAGE128_SRCS=radeon_vid.c
RAGE128_OBJS=rage128_vid.o
RAGE128_LIBS=-L../../libdha -ldha
RAGE128_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -DRAGE128

PM3_VID=pm3_vid.so
PM3_SRCS=pm3_vid.c
PM3_OBJS=pm3_vid.o
PM3_LIBS=-L../../libdha -ldha
PM3_CFLAGS=$(OPTFLAGS) -fPIC -I. -I..

MACH64_VID=mach64_vid.so
MACH64_SRCS=mach64_vid.c
MACH64_OBJS=mach64_vid.o
MACH64_LIBS=-L../../libdha -ldha
MACH64_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -DRAGE128

MGA_VID=mga_vid.so
MGA_SRCS=mga_vid.c
MGA_OBJS=mga_vid.o
MGA_LIBS=-L../../libdha -ldha -lm
MGA_CFLAGS=$(OPTFLAGS) -fPIC -I. -I..

MGA_CRTC2_VID=mga_crtc2_vid.so
MGA_CRTC2_SRCS=mga_vid.c
MGA_CRTC2_OBJS=mga_crtc2_vid.o
MGA_CRTC2_LIBS=-L../../libdha -ldha -lm
MGA_CRTC2_CFLAGS=$(OPTFLAGS) -fPIC -I. -I.. -DCRTC2

all:    $(CYBERBLADE_VID) $(RADEON_VID) $(RAGE128_VID) $(MACH64_VID) $(NVIDIA_VID) $(GENFB_VID) $(MGA_VID) $(MGA_CRTC2_VID) $(PM3_VID)


.SUFFIXES: .c .o

# .PHONY: all clean

$(CYBERBLADE_OBJS):    $(CYBERBLADE_SRCS)
	$(CC) -c $(CYBERBLADE_CFLAGS) -o $@ $<

$(CYBERBLADE_VID):     $(CYBERBLADE_OBJS)
	$(CC) -shared $(CYBERBLADE_OBJS) $(CYBERBLADE_LIBS) -Wl,-soname,$(CYBERBLADE_VID) -o $(CYBERBLADE_VID)

$(PM3_OBJS):    $(PM3_SRCS)
	$(CC) -c $(PM3_CFLAGS) -o $@ $<

$(PM3_VID):     $(PM3_OBJS)
	$(CC) -shared $(PM3_OBJS) $(PM3_LIBS) -Wl,-soname,$(PM3_VID) -o $(PM3_VID)

$(RADEON_OBJS):    $(RADEON_SRCS)
	$(CC) -c $(RADEON_CFLAGS) -o $@ $<

$(RADEON_VID):     $(RADEON_OBJS)
	$(CC) -shared $(RADEON_OBJS) $(RADEON_LIBS) -Wl,-soname,$(RADEON_VID) -o $(RADEON_VID)

$(RAGE128_OBJS):    $(RAGE128_SRCS)
	$(CC) -c $(RAGE128_CFLAGS) -o $@ $<

$(RAGE128_VID):     $(RAGE128_OBJS)
	$(CC) -shared $(RAGE128_OBJS) $(RAGE128_LIBS) -Wl,-soname,$(RAGE128_VID) -o $(RAGE128_VID)

$(MACH64_OBJS):    $(MACH64_SRCS)
	$(CC) -c $(MACH64_CFLAGS) -o $@ $<

$(MACH64_VID):     $(MACH64_OBJS)
	$(CC) -shared $(MACH64_OBJS) $(MACH64_LIBS) -Wl,-soname,$(MACH64_VID) -o $(MACH64_VID)

$(NVIDIA_OBJS):    $(NVIDIA_SRCS)
	$(CC) -c $(NVIDIA_CFLAGS) -o $@ $<

$(NVIDIA_VID):     $(NVIDIA_OBJS)
	$(CC) -shared $(NVIDIA_OBJS) $(NVIDIA_LIBS) -Wl,-soname,$(NVIDIA_VID) -o $(NVIDIA_VID)

$(GENFB_OBJS):    $(GENFB_SRCS)
	$(CC) -c $(GENFB_CFLAGS) -o $@ $<

$(GENFB_VID):     $(GENFB_OBJS)
	$(CC) -shared $(GENFB_OBJS) $(GENFB_LIBS) -Wl,-soname,$(GENFB_VID) -o $(GENFB_VID)

$(MGA_OBJS):    $(MGA_SRCS)
	$(CC) -c $(MGA_CFLAGS) -o $@ $<

$(MGA_VID):     $(MGA_OBJS)
	$(CC) -shared $(MGA_OBJS) $(MGA_LIBS) -Wl,-soname,$(MGA_VID) -o $(MGA_VID)

$(MGA_CRTC2_OBJS):    $(MGA_CRTC2_SRCS)
	$(CC) -c $(MGA_CRTC2_CFLAGS) -o $@ $<

$(MGA_CRTC2_VID):     $(MGA_CRTC2_OBJS)
	$(CC) -shared $(MGA_CRTC2_OBJS) $(MGA_CRTC2_LIBS) -Wl,-soname,$(MGA_CRTC2_VID) -o $(MGA_CRTC2_VID)

clean:
	rm -f *.o *.so *~

distclean:
	rm -f Makefile.bak *.o *.so test *~ .depend

dep:    depend

depend:
# do nothing here

install:
	mkdir -p $(LIBDIR)/mplayer/vidix
	-install -m 755 -s -p *.so $(LIBDIR)/mplayer/vidix
uninstall:
	rm -f $(LIBDIR)/mplayer/vidix/*.so
	rmdir -p --ignore-fail-on-non-empty $(LIBDIR)/mplayer/vidix

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
