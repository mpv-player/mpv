
include config.mak

SRCS = sr1.c d_cpu.s decode_i586.s $(OPTIONAL_SRCS)
OBJS = sr1.o d_cpu.o decode_i586.o $(OPTIONAL_OBJS)
CFLAGS  = $(OPTFLAGS)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c $(CFLAGS) -o $@ $<

libMP3.a:	.depend $(OBJS)
	$(AR) r libMP3.a $(OBJS)

test:	libMP3.a test.c
	$(CC) $(CFLAGS) test.c -o test -I.. -L. -lMP3

all:	libMP3.a

clean:
	rm -f *~ *.o *.a

distclean:
	rm -f *~ *.o *.a Makefile.bak .depend

dep:    depend
depend: .depend

.depend: Makefile config.mak ../config.mak ../config.h
	makedepend -f- -- $(CFLAGS) -- $(SRCS) 1>.depend 2>/dev/null

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
