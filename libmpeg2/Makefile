
LIBNAME = libmpeg2.a

include ../config.mak

SRCS	= header.c idct.c idct_mmx.c motion_comp.c motion_comp_mmx.c slice.c stats.c decode.c
OBJS	= header.o idct.o idct_mmx.o motion_comp.o motion_comp_mmx.o slice.o stats.o decode.o
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
	makedepend -f- -- $(CFLAGS) -- $(SRCS) 1>.depend 2>/dev/null

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
