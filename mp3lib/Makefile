
include ../config.mak

LIBNAME = libMP3.a

ifeq ($(TARGET_ARCH_SGI_MIPS),yes)
OPTFLAGS := $(OPTFLAGS:-O4=-O0)
endif

SRCS = sr1.c
ifeq ($(TARGET_ARCH_X86_32),yes)
SRCS += decode_i586.c
SRCS-$(TARGET_MMX)     += decode_MMX.c dct64_MMX.c tabinit_MMX.c
SRCS-$(TARGET_3DNOW)   += dct36_3dnow.c dct64_3dnow.c
SRCS-$(TARGET_3DNOWEX) += dct36_k7.c dct64_k7.c
SRCS-$(TARGET_SSE)     += dct64_sse.c
endif
ifeq ($(TARGET_ALTIVEC),yes)
SRCS += dct64_altivec.c
ifeq ($(TARGET_OS),Darwin)
CFLAGS += -faltivec
else
CFLAGS += -maltivec -mabi=altivec
endif
endif

include ../mpcommon.mak

decode_i586.o: decode_i586.c
	$(CC) -c $(CFLAGS) -fomit-frame-pointer -o $@ $<

%: %.c	$(LIBNAME) ../mp_msg-mencoder.o ../cpudetect.o
	$(CC) $(CFLAGS) -o $@ ../libvo/aclib.c $^ -lm

distclean::
	rm -f test test2
