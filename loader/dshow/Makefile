
LIBNAME = libDS_Filter.a

include ../../config.mak

# DS_AudioDec.c 
SRCS	= DS_AudioDecoder.c DS_Filter.c DS_VideoDecoder.c allocator.c cmediasample.c guids.c inputpin.c outputpin.c
OBJS = $(SRCS:.c=.o)

# OBJS	= DS_AudioDec.o DS_VideoDec.o DS_Filter.o allocator.o cmediasample.o guids.o inputpin.o outputpin.o

INCLUDE = -I. -I.. $(EXTRA_INC) -DNOAVIFILE_HEADERS
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:   test.c $(LIBNAME)
	$(CC) test.c $(CFLAGS) -o test ./libDS_Filter.a ../libloader.a $(ARCH_LIBS) -lstdc++

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend test test.raw

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

