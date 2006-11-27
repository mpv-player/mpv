
LIBNAME = liba52.a

include ../config.mak

SRCS    = crc.c \
          resample.c \
          bit_allocate.c \
          bitstream.c \
          downmix.c \
          imdct.c \
          parse.c \

include ../mpcommon.mak

test:        $(LIBNAME) test.c
	$(CC) $(CFLAGS)  test.c ../cpudetect.c -o test ../osdep/libosdep.a ./liba52.a -lm

distclean::
	rm -f test
