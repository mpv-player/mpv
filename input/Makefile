
include ../config.mak

LIBNAME = libinput.a

SRCS=input.c joystick.c lirc.c 
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o


.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
