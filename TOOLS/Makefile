include ../config.mak

CFLAGS = -I..

OBJS = alaw-gen$(EXESUF) \
	asfinfo$(EXESUF) \
	avi-fix$(EXESUF) \
	avisubdump$(EXESUF) \
	bios2dump$(EXESUF) \
	dump_mp4$(EXESUF) \
	mem2dump$(EXESUF) \
	movinfo$(EXESUF) \
	png2raw$(EXESUF) \
	subrip$(EXESUF) \
#	vivodump$(EXESUF) \
	302m_convert$(EXESUF) \
	360m_convert$(EXESUF) \

ifeq ($(TARGET_ARCH_X86),yes)
OBJS += cpuinfo$(EXESUF) fastmemcpybench
endif

all: $(OBJS)

png2raw$(EXESUF): png2raw.c
	$(CC) $< -o $@ -lpng

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

gltest: gltest.c ../osdep/timer-lx.o
	$(CC) -O4 -g -o $@ $^ -lglut

vfw2menc$(EXESUF): vfw2menc.c
	$(CC) $< -o $@ -lwinmm -lole32

clean distclean:
	rm -f $(OBJS)
	rm -f fastmem-* fastmem2-* fastmemcpybench gltest
	rm -f cpuinfo$(EXESUF) bmovl-test$(EXESUF) vfw2menc$(EXESUF)
