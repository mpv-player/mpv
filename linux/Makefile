
include ../config.mak

LIBNAME = libosdep.a

SRCS=getch2.c timer-lx.c shmem.c strsep.c # timer.c
OBJS=$(SRCS:.c=.o)

ifeq ($(TARGET_ARCH_X86),yes)
ifeq ($(TARGET_OS),Linux)
SRCS += lrmi.c vbelib.c
endif
endif

CFLAGS  = $(OPTFLAGS) -I. -I..
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

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
