LIBNAME = libvidix.a
SUBDIRS = drivers
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@; done

include ../config.mak

SRCS    = vidixlib.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:	$(SUBDIRS) $(LIBNAME)
	$(DO_MAKE)

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)


clean:
	rm -f *.o *.a *~
	$(DO_MAKE)

distclean:
	rm -f test *.o *.a *~ .depend
	$(DO_MAKE)

dep:    depend
	$(DO_MAKE)

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

install:
	$(DO_MAKE)

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
