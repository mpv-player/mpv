
LIBNAME = libmpeg2.a

include ../config.mak

SRCS	= decode.c header.c idct.c idct_mmx.c motion_comp.c motion_comp_mmx.c slice.c stats.c
OBJS	= decode.o header.o idct.o idct_mmx.o motion_comp.o motion_comp_mmx.o slice.o stats.o
CFLAGS  = $(OPTFLAGS) -DMPG12PLAY
INCLUDE = -I. -I../libvo -I..

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	makedepend
	rm -f Makefile.bak *.o *.a *~

dep:	depend

depend:
	makedepend -- $(CFLAGS) -- $(SRCS) &> /dev/null

# DO NOT DELETE
