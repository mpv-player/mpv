include ../config.mak

LIBNAME_COMMON = liba52.a

SRCS_COMMON = crc.c \
              resample.c \
              bit_allocate.c \
              bitstream.c \
              downmix.c \
              imdct.c \
              parse.c \

include ../mpcommon.mak

test: $(LIBNAME_COMMON) test.c
	$(CC) $(CFLAGS)  test.c ../cpudetect.c -o test ../osdep/libosdep.a ./liba52.a -lm

distclean::
	rm -f test
