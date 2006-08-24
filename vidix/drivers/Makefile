
include ../../config.mak

BINDIR = $(LIBDIR)/mplayer/vidix

VIDIX_LIBS = -L../../libdha -ldha
VIDIX_LIBS_M = $(VIDIX_LIBS) -lm
VIDIX_LIBS_X = $(VIDIX_LIBS_M) $(X_LIB)
VIDIX_CFLAGS=-fPIC -I. -I.. $(OPTFLAGS)

RAGE128_CFLAGS=$(VIDIX_CFLAGS) -DRAGE128

MGA_CRTC2_CFLAGS=$(VIDIX_CFLAGS) -DCRTC2

all:    cyberblade_vid.so radeon_vid.so rage128_vid.so mach64_vid.so \
        nvidia_vid.so mga_vid.so mga_crtc2_vid.so pm3_vid.so \
        sis_vid.so unichrome_vid.so savage_vid.so


.SUFFIXES: .c .o

# .PHONY: all clean

cyberblade_vid.o: cyberblade_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

cyberblade_vid.so: cyberblade_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

pm3_vid.o: pm3_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

pm3_vid.so: pm3_vid.o
	$(CC) -shared $< $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

radeon_vid.o: radeon_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

radeon_vid.so: radeon_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_X) -Wl,-soname,$@ -o $@

rage128_vid.o: radeon_vid.c
	$(CC) -c $(RAGE128_CFLAGS) -o $@ $<

rage128_vid.so: rage128_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_X) -Wl,-soname,$@ -o $@

mach64_vid.o: mach64_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

mach64_vid.so: mach64_vid.o
	$(CC) -shared $< $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

nvidia_vid.o: nvidia_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

nvidia_vid.so: nvidia_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

mga_vid.o: mga_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

mga_vid.so: mga_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

mga_crtc2_vid.o: mga_vid.c
	$(CC) -c $(MGA_CRTC2_CFLAGS) -o $@ $<

mga_crtc2_vid.so: mga_crtc2_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

sis_vid.o sis_bridge.o: sis_vid.c sis_bridge.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $(basename $@).c

sis_vid.so: sis_vid.o sis_bridge.o
	$(CC) -shared $^ $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

unichrome_vid.o: unichrome_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

unichrome_vid.so: unichrome_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

savage_vid.o: savage_vid.c
	$(CC) -c $(VIDIX_CFLAGS) -o $@ $<

savage_vid.so: savage_vid.o
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

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
