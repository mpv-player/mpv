include ../config.mak

CFLAGS=-I. -I.. -DMPLAYER -D__WINE__ $(OPTFLAGS)
#CFLAGS+=-Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT

SRCS= driver.c afl.c vfl.c
ifneq ($(TARGET_WIN32),yes)
SRCS+= ldt_keeper.c pe_image.c module.c ext.c win32.c \
       pe_resource.c resource.c registry.c elfdll.c wrapper.S stubs.s
endif

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
OBJS := $(OBJS:.s=.o)

all: libloader.a

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend

libloader.a:  $(OBJS)
	$(AR) -r libloader.a $(OBJS)
	$(RANLIB) libloader.a

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
