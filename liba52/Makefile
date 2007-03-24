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

test: test.c ../osdep/libosdep.a $(LIBNAME_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ -lm
