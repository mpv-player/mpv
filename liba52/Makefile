
LIBNAME = liba52.a

include ../config.mak

SRCS    = crc.c resample.c bit_allocate.c bitstream.c downmix.c imdct.c imdct_mlib.c parse.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = $(MLIB_INC) $(OPTFLAGS) 

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:        $(LIBNAME) test.c
	$(CC) $(CFLAGS)  test.c ../cpudetect.c -o test ./liba52.a -lm

test2:        $(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c -o test2 ../libac3/libac3.a ./liba52.a -lm

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
