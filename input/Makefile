
include ../config.mak

LIBNAME = libinput.a

SRCS=input.c
ifeq ($(JOYSTICK),yes)
SRCS += joystick.c
endif
ifeq ($(LIRC),yes)
SRCS += lirc.c
endif

OBJS=$(SRCS:.c=.o)

CFLAGS  = -I. -I.. $(OPTFLAGS)

.SUFFIXES: .c .o


.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

all:    $(LIBNAME)

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
