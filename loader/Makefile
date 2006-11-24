include ../config.mak

CFLAGS=-I. -I.. -DMPLAYER -D__WINE__ $(OPTFLAGS)
#CFLAGS+=-Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT

SRCS= driver.c afl.c vfl.c
ifneq ($(TARGET_WIN32),yes)
SRCS+= ldt_keeper.c pe_image.c module.c ext.c win32.c \
       pe_resource.c resource.c registry.c elfdll.c wrapper.S
endif

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

all: libloader.a

clean:
	rm -f *.o *.a *~

distclean: clean

.c.o:
	$(CC) $(CFLAGS) -c $<

libloader.a:  $(OBJS) stubs.s
	$(CC) -c ./stubs.s -o stubs.o
	$(AR) -r libloader.a $(OBJS) stubs.o
	$(RANLIB) libloader.a

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
