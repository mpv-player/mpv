include ../../config.mak

SRCS_MPLAYER = $(wildcard *.c)
VIDIX_LIBS   = -L../../libdha -ldha
CFLAGS       = -fPIC

libs:   cyberblade_vid.so radeon_vid.so rage128_vid.so mach64_vid.so \
        nvidia_vid.so mga_vid.so mga_crtc2_vid.so pm3_vid.so \
        sis_vid.so unichrome_vid.so savage_vid.so

%.so: %.o
	$(CC) -shared $^ $(VIDIX_LIBS) -Wl,-soname,$@ -o $@

mga_crtc2_vid.o: mga_vid.c
	$(CC) -c $(CFLAGS) -DCRTC2 -o $@ $<

rage128_vid.o: radeon_vid.c
	$(CC) -c $(CFLAGS) -DRAGE128 -o $@ $<

sis_vid.so: sis_vid.o sis_bridge.o

cyberblade_vid.so nvidia_vid.so mga_vid.so mga_crtc2_vid.so unichrome_vid.so \
savage_vid.so: VIDIX_LIBS += -lm

include ../../mpcommon.mak

clean::
	rm -f *.so
