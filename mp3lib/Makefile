include ../config.mak

LIBNAME_COMMON = mp3lib.a

SRCS_COMMON = sr1.c
SRCS_COMMON-$(HAVE_MMX)       += decode_mmx.c
SRCS_COMMON-$(HAVE_SSE)       += dct64_sse.c
SRCS_COMMON                   += decode_i586.c
SRCS_COMMON-$(ARCH_X86_32)-$(HAVE_MMX)       += dct64_mmx.c
SRCS_COMMON-$(ARCH_X86_32)-$(HAVE_3DNOW)     += dct36_3dnow.c dct64_3dnow.c
SRCS_COMMON-$(ARCH_X86_32)-$(HAVE_3DNOWEX)   += dct36_k7.c dct64_k7.c
SRCS_COMMON-$(HAVE_ALTIVEC)   += dct64_altivec.c

include ../mpcommon.mak

decode_i586.o: CFLAGS += -fomit-frame-pointer

%: %.c $(LIBNAME_COMMON) ../libvo/aclib.o ../mp_msg-mencoder.o ../cpudetect.o ../osdep/getch2.o
	$(CC) $(CFLAGS) -o $@ $^ -ltermcap -lm
