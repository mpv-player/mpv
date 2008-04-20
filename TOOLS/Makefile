include ../config.mak

CFLAGS = -I..

OBJS = alaw-gen$(EXESUF) \
	asfinfo$(EXESUF) \
	avi-fix$(EXESUF) \
	avisubdump$(EXESUF) \
	compare$(EXESUF) \
	dump_mp4$(EXESUF) \
	modify_reg$(EXESUF) \
	movinfo$(EXESUF) \
	subrip$(EXESUF) \
#	vivodump$(EXESUF) \

COMMON_LIBS = ../mp_msg.o ../mp_fifo.o ../osdep/$(TIMER) ../osdep/$(GETCH) \
              -ltermcap -lm

all: $(OBJS)

bmovl-test$(EXESUF): bmovl-test.c -lSDL_image

subrip$(EXESUF): subrip.c ../vobsub.o ../spudec.o ../unrar_exec.o \
  ../libswscale/libswscale.a ../libavutil/libavutil.a $(COMMON_LIBS)

vfw2menc$(EXESUF): vfw2menc.c -lwinmm -lole32

#FIXME: Linking is broken, help welcome.
vivodump$(EXESUF): vivodump.c ../libmpdemux/libmpdemux.a $(COMMON_LIBS)

fastmemcpybench: fastmemcpybench.c
	$(CC) $(CFLAGS) $< -o fastmem-mmx$(EXESUF)  -DNAME=\"mmx\"      -DHAVE_MMX
	$(CC) $(CFLAGS) $< -o fastmem-k6$(EXESUF)   -DNAME=\"k6\ \"     -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) $< -o fastmem-k7$(EXESUF)   -DNAME=\"k7\ \"     -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) $< -o fastmem-sse$(EXESUF)  -DNAME=\"sse\"      -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2
	$(CC) $(CFLAGS) $< -o fastmem2-mmx$(EXESUF) -DNAME=\"mga-mmx\"  -DHAVE_MGA -DHAVE_MMX
	$(CC) $(CFLAGS) $< -o fastmem2-k6$(EXESUF)  -DNAME=\"mga-k6\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) $< -o fastmem2-k7$(EXESUF)  -DNAME=\"mga-k7\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) $< -o fastmem2-sse$(EXESUF) -DNAME=\"mga-sse\"  -DHAVE_MGA -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2

REAL_SRCS    = $(wildcard realcodecs/*.c)
REAL_TARGETS = $(REAL_SRCS:.c=.so.6.0)

realcodecs: $(REAL_TARGETS)

fastmemcpybench realcodecs: CFLAGS += -g

%.so.6.0: %.o
	ld -shared -o $@ $< -ldl -lc

# FIXME: netstream linking is a mess that should be fixed properly some day.
# It does not work with either GUI, LIVE555, libavformat, cdparanoia enabled.
NETSTREAM_DEPS = ../libmpdemux/libmpdemux.a \
                 ../stream/stream.a \
                 ../dvdread/libdvdread.a \
                 ../libdvdcss/libdvdcss.a \
                 ../libavutil/libavutil.a \
                 ../m_option.o \
                 ../m_struct.o \
                  $(COMMON_LIBS)

netstream: netstream.o $(NETSTREAM_DEPS)
	$(CC) $(CFLAGS) -o $@ $^

clean distclean:
	rm -f *.o *~ $(OBJS)
	rm -f fastmem*-* netstream
	rm -f bmovl-test$(EXESUF) vfw2menc$(EXESUF)
	rm -f $(REAL_TARGETS)

.PHONY: all fastmemcpybench realcodecs clean distclean
