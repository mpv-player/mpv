
LIBNAME = libDMO_Filter.a

include ../../config.mak

# DS_AudioDec.c 
SRCS	= DMO_AudioDecoder.c \
	  DMO_VideoDecoder.c   \
	  buffer.c   \
	  dmo.c  \
	  dmo_guids.c \

OBJS = $(SRCS:.c=.o)

INCLUDE = -I. -I../dshow -I..
CFLAGS  = $(INCLUDE) $(OPTFLAGS) -DNOAVIFILE_HEADERS

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:   test.c $(LIBNAME)
	$(CC) test.c $(CFLAGS) -o test ./libDMO_Filter.a ../libloader.a -lstdc++

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

