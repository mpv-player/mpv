# makefile

include ../../config.mak

BINDIR = $(LIBDIR)/mplayer/vidix

VIDIX_LIBS = -L../../libdha -ldha
VIDIX_CFLAGS=-fPIC -I. -I.. $(OPTFLAGS)

CYBERBLADE_VID=cyberblade_vid.so
CYBERBLADE_SRCS=cyberblade_vid.c
CYBERBLADE_OBJS=cyberblade_vid.o
CYBERBLADE_LIBS=$(VIDIX_LIBS) -lm

RADEON_VID=radeon_vid.so
RADEON_SRCS=radeon_vid.c
RADEON_OBJS=radeon_vid.o
RADEON_LIBS=$(VIDIX_LIBS) -lm $(X_LIB)

RAGE128_VID=rage128_vid.so
RAGE128_SRCS=radeon_vid.c
RAGE128_OBJS=rage128_vid.o
RAGE128_LIBS=$(VIDIX_LIBS) -lm $(X_LIB)
RAGE128_CFLAGS=$(VIDIX_CFLAGS) -DRAGE128

PM3_VID=pm3_vid.so
PM3_SRCS=pm3_vid.c
PM3_OBJS=pm3_vid.o

MACH64_VID=mach64_vid.so
MACH64_SRCS=mach64_vid.c
MACH64_OBJS=mach64_vid.o

MGA_VID=mga_vid.so
MGA_SRCS=mga_vid.c
MGA_OBJS=mga_vid.o
MGA_LIBS=$(VIDIX_LIBS) -lm

MGA_CRTC2_VID=mga_crtc2_vid.so
MGA_CRTC2_SRCS=mga_vid.c
MGA_CRTC2_OBJS=mga_crtc2_vid.o
MGA_CRTC2_LIBS=$(VIDIX_LIBS) -lm
MGA_CRTC2_CFLAGS=$(VIDIX_CFLAGS) -DCRTC2

NVIDIA_VID=nvidia_vid.so
NVIDIA_SRCS=nvidia_vid.c
NVIDIA_OBJS=nvidia_vid.o
NVIDIA_LIBS=$(VIDIX_LIBS) -lm

SIS_VID=sis_vid.so
SIS_SRCS=sis_vid.c sis_bridge.c
SIS_OBJS=sis_vid.o sis_bridge.o

UNICHROME_VID=unichrome_vid.so
UNICHROME_SRCS=unichrome_vid.c
UNICHROME_OBJS=unichrome_vid.o
UNICHROME_LIBS=$(VIDIX_LIBS) -lm

SAVAGE_VID=savage_vid.so
SAVAGE_SRCS=savage_vid.c
SAVAGE_OBJS=savage_vid.o
SAVAGE_LIBS=$(VIDIX_LIBS) -lm

all:    $(CYBERBLADE_VID) $(RADEON_VID) $(RAGE128_VID) $(MACH64_VID) $(NVIDIA_VID) $(MGA_VID) $(MGA_CRTC2_VID) $(PM3_VID) $(SIS_VID) $(UNICHROME_VID) $(SAVAGE_VID)


.SUFFIXES: .c .o

# .PHONY: all clean

$(CYBERBLADE_OBJS):    $(CYBERBLADE_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(CYBERBLADE_VID):     $(CYBERBLADE_OBJS)
	$(CC) -shared $< $(CYBERBLADE_LIBS) -Wl,-soname,$@ -o $@

$(PM3_OBJS):    $(PM3_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(PM3_VID):     $(PM3_OBJS)
	$(CC) -shared $< $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

$(RADEON_OBJS):    $(RADEON_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(RADEON_VID):     $(RADEON_OBJS)
	$(CC) -shared $< $(RADEON_LIBS) -Wl,-soname,$@ -o $@

$(RAGE128_OBJS):    $(RAGE128_SRCS)
	$(CC) -c $(RAGE128_CFLAGS) -o $@ $<

$(RAGE128_VID):     $(RAGE128_OBJS)
	$(CC) -shared $< $(RAGE128_LIBS) -Wl,-soname,$@ -o $@

$(MACH64_OBJS):    $(MACH64_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(MACH64_VID):     $(MACH64_OBJS)
	$(CC) -shared $< $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

$(NVIDIA_OBJS):    $(NVIDIA_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(NVIDIA_VID):     $(NVIDIA_OBJS)
	$(CC) -shared $< $(NVIDIA_LIBS) -Wl,-soname,$@ -o $@

$(MGA_OBJS):    $(MGA_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(MGA_VID):     $(MGA_OBJS)
	$(CC) -shared $< $(MGA_LIBS) -Wl,-soname,$@ -o $@

$(MGA_CRTC2_OBJS):    $(MGA_CRTC2_SRCS)
	$(CC) -c $(MGA_CRTC2_CFLAGS) -o $@ $<

$(MGA_CRTC2_VID):     $(MGA_CRTC2_OBJS)
	$(CC) -shared $< $(MGA_CRTC2_LIBS) -Wl,-soname,$@ -o $@

$(SIS_OBJS):    $(SIS_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $(basename $@).c

$(SIS_VID):     $(SIS_OBJS)
	$(CC) -shared $< $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

$(UNICHROME_OBJS):    $(UNICHROME_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(UNICHROME_VID):     $(UNICHROME_OBJS)
	$(CC) -shared $< $(UNICHROME_LIBS) -Wl,-soname,$@ -o $@

$(SAVAGE_OBJS):    $(SAVAGE_SRCS)
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

$(SAVAGE_VID):     $(SAVAGE_OBJS)
	$(CC) -shared $< $(SAVAGE_LIBS) -Wl,-soname,$@ -o $@

clean:
	rm -f *.o *.so *~

distclean: clean
	rm -f .depend test

dep:    depend

depend:
# do nothing here

install:
	mkdir -p $(LIBDIR)/mplayer/vidix
	-install -m 755 $(INSTALLSTRIP) -p *.so $(LIBDIR)/mplayer/vidix
uninstall:
	rm -f $(LIBDIR)/mplayer/vidix/*.so
	-rmdir -p $(LIBDIR)/mplayer/vidix

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
