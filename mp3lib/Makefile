
include ../config.mak

SRCS = sr1.c
OBJS = sr1.o
# OBJS = $(SRCS:.c,.s=.o)
ifeq ($(TARGET_ARCH_SGI_MIPS),yes)
OPTFLAGS := $(OPTFLAGS:-O4=-O0)
endif
CFLAGS  = $(OPTFLAGS) $(EXTRA_INC)
ifeq ($(TARGET_ARCH_X86),yes)
SRCS += d_cpu.s decode_i586.c
OBJS += d_cpu.o decode_i586.o
ifeq ($(TARGET_MMX),yes)
SRCS += decode_MMX.c dct64_MMX.s tabinit_MMX.c
OBJS += decode_MMX.o dct64_MMX.o tabinit_MMX.o
endif
#ifeq ($(TARGET_SSE),yes)
#SRCS += dct64_sse.s
#OBJS += dct64_sse.o
#endif
ifeq ($(TARGET_3DNOW),yes)
SRCS += dct36_3dnow.s dct64_3dnow.s
OBJS += dct36_3dnow.o dct64_3dnow.o
endif
ifeq ($(TARGET_3DNOWEX),yes)
SRCS += dct36_k7.s dct64_k7.s
OBJS += dct36_k7.o dct64_k7.o
endif
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c $(CFLAGS) -o $@ $<

libMP3.a:	$(OBJS)
	$(AR) r libMP3.a $(OBJS)

test1:	libMP3.a test.c
	$(CC) $(CFLAGS) test.c ../libvo/aclib.c -o test1 -I.. -L. -lMP3 -lm

test2:	libMP3.a test2.c
	$(CC) $(CFLAGS) test2.c ../libvo/aclib.c -o test2 -I.. -L. -lMP3 -lm

all:	libMP3.a

clean:
	rm -f *~ *.o *.a

distclean:
	rm -f *~ *.o *.a Makefile.bak .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
