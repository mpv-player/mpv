include ../config.mak

LIBNAME_MPLAYER = libvidix.a

SRCS_MPLAYER    = vidixlib.c \
                  $(wildcard *.c)

OBJS_MPLAYER    = mga_crtc2_vid.o \
                  rage128_vid.o \

include ../mpcommon.mak

mga_crtc2_vid.o: mga_vid.c
	$(CC) -c $(CFLAGS) -DCRTC2 -o $@ $<

rage128_vid.o: radeon_vid.c
	$(CC) -c $(CFLAGS) -DRAGE128 -o $@ $<
