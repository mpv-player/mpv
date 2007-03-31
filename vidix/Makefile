include ../config.mak

LIBNAME_MPLAYER = libvidix.a

SRCS_MPLAYER    = vidixlib.c \
                  $(wildcard drivers/*.c)

OBJS_MPLAYER    = drivers/mga_crtc2_vid.o \
                  drivers/rage128_vid.o \

include ../mpcommon.mak

drivers/mga_crtc2_vid.o: drivers/mga_vid.c
	$(CC) -c $(CFLAGS) -DCRTC2 -o $@ $<

drivers/rage128_vid.o: drivers/radeon_vid.c
	$(CC) -c $(CFLAGS) -DRAGE128 -o $@ $<

clean::
	rm -f drivers/*.o drivers/*~
