include ../config.mak

LIBNAME_COMMON = loader.a

#CFLAGS+=-Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT

SRCS_COMMON  = driver.c afl.c vfl.c
SRCS_COMMON-$(QTX_EMULATION)   += wrapper.S
SRCS_COMMON-$(WIN32_EMULATION) += elfdll.c \
                                  ext.c \
                                  ldt_keeper.c \
                                  module.c \
                                  pe_image.c \
                                  pe_resource.c \
                                  registry.c \
                                  resource.c \
                                  win32.c \

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

CFLAGS+=-fno-omit-frame-pointer $(CFLAG_NO_OMIT_LEAF_FRAME_POINTER)
win32.o: CFLAGS += $(CFLAG_STACKREALIGN)

TEST_OBJS    = $(LIBNAME_COMMON) ../cpudetect.o ../mp_msg.o ../mp_fifo.o ../osdep/mmap_anon.o ../osdep/$(GETCH) ../osdep/$(TIMER)
TEST_LDFLAGS = -ltermcap -lm -g

qtx/qtxload: qtx/qtxload.c $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

qtx/list: qtx/list.c  $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

clean::
	rm -f dshow/*.o dshow/*.a dshow/*~
	rm -f dmo/*.o dmo/*.a dmo/*~
	rm -f dshow/test dshow/test.raw
	rm -f qtx/list qtx/qtxload
