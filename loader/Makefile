include ../config.mak

LIBNAME_COMMON = libloader.a

CFLAGS= -DMPLAYER -D__WINE__ -DNOAVIFILE_HEADERS
#CFLAGS+=-Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT

SRCS_COMMON  = driver.c afl.c vfl.c
ifneq ($(TARGET_WIN32),yes)
SRCS_COMMON += ldt_keeper.c pe_image.c module.c ext.c win32.c \
               pe_resource.c resource.c registry.c elfdll.c
# QTX emulation is not supported in Darwin
ifneq ($(TARGET_OS),Darwin)
SRCS_COMMON += wrapper.S
endif
endif

SRCS_COMMON += dshow/DS_AudioDecoder.c \
               dshow/DS_Filter.c \
               dshow/DS_VideoDecoder.c \
               dshow/allocator.c \
               dshow/mediatype.c \
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

CFLAGS+=-fno-omit-frame-pointer -mno-omit-leaf-frame-pointer

dshow/test: dshow/test.c $(LIBNAME)
	$(CC) $(CFLAGS) -o $@ $^ -lstdc++

clean::
	rm -f dshow/*.o dshow/*.a dshow/*~
	rm -f dmo/*.o dmo/*.a dmo/*~

distclean::
	rm -f dshow/test dshow/test.raw
