
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

libMP3.a:	$(OBJS)
	$(AR) r libMP3.a $(OBJS)

all:	libMP3.a

clean:
	rm -f *~ *.o *.a

distclean:
	makedepend
	rm -f *~ *.o *.a Makefile.bak

dep:	depend

depend:
	makedepend -- $(CFLAGS) -- $(SRCS) &>/dev/null
# DO NOT DELETE
