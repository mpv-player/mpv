include ../../config.mak

VIDIX_LIBS   = -L../../libdha -ldha
VIDIX_LIBS_M = $(VIDIX_LIBS) -lm
VIDIX_LIBS_X = $(VIDIX_LIBS_M) $(X_LIB)

CFLAGS           = -fPIC -I. -I.. $(OPTFLAGS)
RAGE128_CFLAGS   = $(CFLAGS) -DRAGE128
MGA_CRTC2_CFLAGS = $(CFLAGS) -DCRTC2

all:    cyberblade_vid.so radeon_vid.so rage128_vid.so mach64_vid.so \
        nvidia_vid.so mga_vid.so mga_crtc2_vid.so pm3_vid.so \
        sis_vid.so unichrome_vid.so savage_vid.so

%.so: %.o
	$(CC) -shared $^ $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

mga_crtc2_vid.o: mga_vid.c
	$(CC) -c $(MGA_CRTC2_CFLAGS) -o $@ $<

rage128_vid.o: radeon_vid.c
	$(CC) -c $(RAGE128_CFLAGS) -o $@ $<

sis_vid.so: sis_vid.o sis_bridge.o

radeon_vid.so: radeon_vid.o
rage128_vid.so: rage128_vid.o
radeon_vid.so rage128_vid.so:
	$(CC) -shared $< $(VIDIX_LIBS_X) -Wl,-soname,$@ -o $@

cyberblade_vid.so: cyberblade_vid.o
nvidia_vid.so: nvidia_vid.o
mga_vid.so: mga_vid.o
mga_crtc2_vid.so: mga_crtc2_vid.o
unichrome_vid.so: unichrome_vid.o
savage_vid.so: savage_vid.o
cyberblade_vid.so nvidia_vid.so mga_vid.so mga_crtc2_vid.so unichrome_vid.so \
savage_vid.so:
	$(CC) -shared $< $(VIDIX_LIBS_M) -Wl,-soname,$@ -o $@

clean:
	rm -f *.o *.so *~

distclean: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(wildcard *.c) 1>.depend

-include .depend

.PHONY: all clean distclean dep depend
