include ../config.mak

CFLAGS = -I..

OBJS = alaw-gen$(EXESUF) \
	asfinfo$(EXESUF) \
	avi-fix$(EXESUF) \
	avisubdump$(EXESUF) \
	dump_mp4$(EXESUF) \
	movinfo$(EXESUF) \
	subrip$(EXESUF) \
#	vivodump$(EXESUF) \

all: $(OBJS)

subrip$(EXESUF): subrip.c
	$(CC) $(CFLAGS) -g -o $@ $< ../vobsub.o ../spudec.o ../mp_msg.o \
          ../unrarlib.o ../libswscale/libswscale.a ../libavutil/libavutil.a \
          ../libmpcodecs/img_format.o ../osdep/getch2.o -ltermcap -lm

#FIXME: Linking is broken, help welcome.
vivodump$(EXESUF): vivodump.c
	$(CC) $(CFLAGS) -o $@ $< ../mp_msg.o ../libmpdemux/libmpdemux.a \
           ../stream/stream.a ../osdep/getch2.o -ltermcap -lm

fastmemcpybench: fastmemcpybench.c
	$(CC) $(CFLAGS) -g $< -o fastmem-mmx$(EXESUF)  ../libvo/aclib.o -DNAME=\"mmx\"      -DHAVE_MMX
	$(CC) $(CFLAGS) -g $< -o fastmem-k6$(EXESUF)   ../libvo/aclib.o -DNAME=\"k6\ \"     -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) -g $< -o fastmem-k7$(EXESUF)   ../libvo/aclib.o -DNAME=\"k7\ \"     -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem-sse$(EXESUF)  ../libvo/aclib.o -DNAME=\"sse\"      -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem2-mmx$(EXESUF) ../libvo/aclib.o -DNAME=\"mga-mmx\"  -DHAVE_MGA -DHAVE_MMX
	$(CC) $(CFLAGS) -g $< -o fastmem2-k6$(EXESUF)  ../libvo/aclib.o -DNAME=\"mga-k6\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) -g $< -o fastmem2-k7$(EXESUF)  ../libvo/aclib.o -DNAME=\"mga-k7\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem2-sse$(EXESUF) ../libvo/aclib.o -DNAME=\"mga-sse\"  -DHAVE_MGA -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2

bmovl-test$(EXESUF): bmovl-test.c
	$(CC) -O3 $(EXTRA_INC) -o $@ $< -lSDL_image

vfw2menc$(EXESUF): vfw2menc.c
	$(CC) $< -o $@ -lwinmm -lole32

REAL_SRCS    = $(wildcard realcodecs/*.c)
REAL_TARGETS = $(REAL_SRCS:.c=.so.6.0)

realcodecs: $(REAL_TARGETS)
realcodecs: CFLAGS += -g

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
                 ../mp_msg.o \
                 ../osdep/libosdep.a \
                 -ltermcap -lm \

ifeq ($(TARGET_WIN32),yes)
NETSTREAM_DEPS += -lwsock32
endif

netstream: netstream.o $(NETSTREAM_DEPS)
	$(CC) -o $@ $^

clean distclean:
	rm -f *.o *~ $(OBJS)
	rm -f fastmem-* fastmem2-* fastmemcpybench netstream
	rm -f cpuinfo$(EXESUF) bmovl-test$(EXESUF) vfw2menc$(EXESUF)
	rm -f $(REAL_TARGETS)
