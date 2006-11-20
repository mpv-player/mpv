
LIBNAME = libvorbisidec.a

include ../config.mak

SRCS	= bitwise.c \
          block.c \
          codebook.c \
          floor0.c \
          floor1.c \
          framing.c \
          info.c \
          mapping0.c \
          mdct.c \
          registry.c \
          res012.c \
          sharedbook.c \
          synthesis.c \
          window.c \

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I. -I..
CFLAGS  = $(OPTFLAGS) $(INCLUDE) $(TREMOR_FLAGS)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
