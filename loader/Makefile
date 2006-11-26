include ../config.mak

LIBNAME = libloader.a

CFLAGS= -Idshow -DMPLAYER -D__WINE__ -DNOAVIFILE_HEADERS
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

include ../mpcommon.mak

dshow/test: libloader.a
	$(CC) dshow/test.c $(CFLAGS) -o $@ $^ -lstdc++

clean::
	rm -f dshow/*.o dshow/*.a dshow/*~
	rm -f dmo/*.o dmo/*.a dmo/*~

distclean::
	rm -f dshow/test dshow/test.raw
