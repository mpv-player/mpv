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
	vivodump$(EXESUF) \
	302m_convert$(EXESUF) \
	360m_convert$(EXESUF) \

ifeq ($(TARGET_ARCH_X86),yes)
OBJS += cpuinfo$(EXESUF) fastmemcpybench
endif

all: $(OBJS)

png2raw: png2raw.c
	$(CC) $< -o $@ -lpng

subrip: subrip.c
	$(CC) $(CFLAGS) -g -o $@ $< ../vobsub.o ../spudec.o ../mp_msg.o \
          ../unrarlib.o ../libswscale/swscale.o ../libswscale/rgb2rgb.o \
          ../libswscale/yuv2rgb.o ../libmpcodecs/img_format.o -lm

vivodump: vivodump.c
	$(CC) $(CFLAGS) -o $@ $< ../mp_msg.o ../libmpdemux/libmpdemux.a ../stream/stream.a

fastmemcpybench: fastmemcpybench.c
	$(CC) $(CFLAGS) -g $< -o fastmem-mmx$(EXESUF)  ../libvo/aclib.o -DNAME=\"mmx\"      -DHAVE_MMX
	$(CC) $(CFLAGS) -g $< -o fastmem-k6$(EXESUF)   ../libvo/aclib.o -DNAME=\"k6\ \"     -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) -g $< -o fastmem-k7$(EXESUF)   ../libvo/aclib.o -DNAME=\"k7\ \"     -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem-sse$(EXESUF)  ../libvo/aclib.o -DNAME=\"sse\"      -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem2-mmx$(EXESUF) ../libvo/aclib.o -DNAME=\"mga-mmx\"  -DHAVE_MGA -DHAVE_MMX
	$(CC) $(CFLAGS) -g $< -o fastmem2-k6$(EXESUF)  ../libvo/aclib.o -DNAME=\"mga-k6\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) -g $< -o fastmem2-k7$(EXESUF)  ../libvo/aclib.o -DNAME=\"mga-k7\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem2-sse$(EXESUF) ../libvo/aclib.o -DNAME=\"mga-sse\"  -DHAVE_MGA -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2

clean:
	rm -f $(OBJS)
	rm -f fastmem-* fastmem2-*
