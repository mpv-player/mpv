
LIBNAME = libDS_Filter.a

include ../../config.mak

# DS_AudioDec.c 
SRCS	= DS_AudioDecoder.c \
	  DS_Filter.c \
	  DS_VideoDecoder.c \
	  allocator.c \
	  cmediasample.c \
	  guids.c \
	  inputpin.c \
	  outputpin.c \

OBJS = $(SRCS:.c=.o)

# OBJS	= DS_AudioDec.o DS_VideoDec.o DS_Filter.o allocator.o cmediasample.o guids.o inputpin.o outputpin.o

INCLUDE = -I. -I..
CFLAGS  = $(INCLUDE) $(OPTFLAGS) -DNOAVIFILE_HEADERS

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:   test.c $(LIBNAME)
	$(CC) test.c $(CFLAGS) -o test ./libDS_Filter.a ../libloader.a -lstdc++

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend test test.raw

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

