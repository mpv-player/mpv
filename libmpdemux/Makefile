
LIBNAME = libmpdemux.a

include ../config.mak

SRCS = cache2.c asfheader.c aviheader.c aviprint.c aviwrite.c demux_asf.c demux_avi.c demux_mov.c demux_mpg.c demuxer.c dvdauth.c open.c parse_es.c stream.c
ifeq ($(STREAMING),yes)
SRCS += asf_streaming.c url.c http.c network.c
endif

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I../loader $(EXTRA_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

.SUFFIXES: .c .o

# .PHONY: all clean

all:	$(LIBNAME)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../mp_msg.c ../linux/shmem.c -o test -L. -lmpdemux -ldvdread -lz -lpthread

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
