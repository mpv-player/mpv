
include ../config.mak

LIBNAME = libao.a

SRCS=audio_out.c ao_oss.c ao_null.c
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I..
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     .depend $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

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
