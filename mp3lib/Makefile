
include config.mak

SRCS = sr1.c d_cpu.s decode_i586.s dct64_MMX.s decode_MMX.s tabinit_MMX.s\
dct36_3dnow.s dct64_3dnow.s dct36_k7.s dct64_k7.s
OBJS = sr1.o d_cpu.o decode_i586.o dct64_MMX.o decode_MMX.o tabinit_MMX.o\
dct36_3dnow.o dct64_3dnow.o dct36_k7.o dct64_k7.o
# OBJS = $(SRCS:.c,.s=.o)
CFLAGS  = $(OPTFLAGS) $(EXTRA_INC)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c $(CFLAGS) -o $@ $<

libMP3.a:	$(OBJS)
	$(AR) r libMP3.a $(OBJS)

test1:	libMP3.a test.c
	$(CC) $(CFLAGS) test.c -o test1 -I.. -L. -lMP3 -lm

test2:	libMP3.a test2.c
	$(CC) $(CFLAGS) test2.c -o test2 -I.. -L. -lMP3 -lm

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
