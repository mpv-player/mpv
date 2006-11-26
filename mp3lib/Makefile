
include ../config.mak

LIBNAME = libMP3.a

ifeq ($(TARGET_ARCH_SGI_MIPS),yes)
OPTFLAGS := $(OPTFLAGS:-O4=-O0)
endif
CFLAGS  = -I.. $(OPTFLAGS)

SRCS = sr1.c
ifeq ($(TARGET_ARCH_X86_32),yes)
SRCS += decode_i586.c
ifeq ($(TARGET_MMX),yes)
SRCS += decode_MMX.c dct64_MMX.c tabinit_MMX.c
endif
ifeq ($(TARGET_3DNOW),yes)
SRCS += dct36_3dnow.c dct64_3dnow.c
endif
ifeq ($(TARGET_3DNOWEX),yes)
SRCS += dct36_k7.c dct64_k7.c
endif
ifeq ($(TARGET_SSE),yes)
SRCS += dct64_sse.c
endif
endif
ifeq ($(TARGET_ARCH_POWERPC),yes)
ifeq ($(TARGET_ALTIVEC),yes)
SRCS += dct64_altivec.c
ifeq ($(TARGET_OS),Darwin)
CFLAGS += -faltivec
else
CFLAGS += -maltivec -mabi=altivec
endif
endif
endif

include ../mpcommon.mak

decode_i586.o: decode_i586.c
	$(CC) -c $(CFLAGS) -fomit-frame-pointer -o $@ $<

.s.o:
	$(CC) -c $(CFLAGS) -o $@ $<

test1:	libMP3.a test.c
	$(CC) $(CFLAGS) test.c ../libvo/aclib.c -o test1 -I.. ./libMP3.a ../mp_msg-mencoder.o ../cpudetect.o -lm

test2:	libMP3.a test2.c
	$(CC) $(CFLAGS) test2.c ../libvo/aclib.c -o test2 -I.. ./libMP3.a ../mp_msg-mencoder.o ../cpudetect.o -lm

distclean::
	rm -f test1 test2
