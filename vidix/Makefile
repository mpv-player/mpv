LIBNAME = libvidix.a

include ../config.mak

SRCS    = vidixlib.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:	$(LIBNAME) do_drivers

do_drivers:
	$(MAKE) -C drivers

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

clean:
	rm -f *.o *.a *~
	$(MAKE) -C drivers clean

distclean: clean
	rm -f .depend test
	$(MAKE) -C drivers distclean

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend
	$(MAKE) -C drivers depend

install:
	$(MAKE) -C drivers install

uninstall:
	$(MAKE) -C drivers uninstall

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
