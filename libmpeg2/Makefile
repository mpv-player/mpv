
LIBNAME = libmpeg2.a

include ../config.mak

SRCS	= header.c idct.c idct_mmx.c motion_comp.c motion_comp_mmx.c slice.c stats.c decode.c
OBJS	= $(SRCS:.c=.o)
INCLUDE = -I. -I../libvo -I..
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -DMPG12PLAY

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	.depend $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend
depend: .depend

.depend: Makefile ../config.mak ../config.h
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
