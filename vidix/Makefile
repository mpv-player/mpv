
LIBNAME = libvidix.a

include ../config.mak

SRCS    = vidixlib.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -W -Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
