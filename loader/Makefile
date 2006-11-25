include ../config.mak

CFLAGS=-I. -I.. -Idshow -DMPLAYER -D__WINE__ -DNOAVIFILE_HEADERS $(OPTFLAGS)
#CFLAGS+=-Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT

SRCS= driver.c afl.c vfl.c
ifneq ($(TARGET_WIN32),yes)
SRCS+= ldt_keeper.c pe_image.c module.c ext.c win32.c \
       pe_resource.c resource.c registry.c elfdll.c wrapper.S stubs.s
endif

SRCS+= dshow/DS_AudioDecoder.c \
       dshow/DS_Filter.c \
       dshow/DS_VideoDecoder.c \
       dshow/allocator.c \
       dshow/cmediasample.c \
       dshow/guids.c \
       dshow/inputpin.c \
       dshow/outputpin.c \
       dmo/DMO_AudioDecoder.c \
       dmo/DMO_VideoDecoder.c   \
       dmo/buffer.c   \
       dmo/dmo.c  \
       dmo/dmo_guids.c \

OBJS  = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
OBJS := $(OBJS:.s=.o)

all: libloader.a

libloader.a:  $(OBJS)
	$(AR) -r $@ $^
	$(RANLIB) $@

dshow/test: libloader.a
	$(CC) dshow/test.c $(CFLAGS) -o $@ $^ -lstdc++

clean:
	rm -f *.o *.a *~
	rm -f dshow/*.o dshow/*.a dshow/*~
	rm -f dmo/*.o dmo/*.a dmo/*~

distclean: clean
	rm -f .depend
	rm -f dshow/.depend dshow/test dshow/test.raw
	rm -f dmo/.depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

ifneq ($(wildcard .depend),)
include .depend
endif
