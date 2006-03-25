include ../config.mak

CFLAGS = -I..

OBJS = alaw-gen \
	asfinfo \
	avi-fix \
	avisubdump \
	bios2dump \
	dump_mp4 \
	mem2dump \
	movinfo \
	png2raw \
	subrip \
	vivodump \
	302m_convert \
	360m_convert \

ifeq ($(TARGET_ARCH_X86),yes)
OBJS += cpuinfo fastmemcpybench
endif

all: $(OBJS)

png2raw: png2raw.c
	$(CC) $< -o $@ -lpng

subrip: subrip.c
	$(CC) $(CFLAGS) -g -o $@ $< ../vobsub.o ../spudec.o ../mp_msg.o \
          ../unrarlib.o ../postproc/swscale.o ../postproc/rgb2rgb.o \
          ../postproc/yuv2rgb.o ../libmpcodecs/img_format.o -lm

vivodump: vivodump.c
	$(CC) $(CFLAGS) -o $@ $< ../mp_msg.o ../libmpdemux/libmpdemux.a

fastmemcpybench: fastmemcpybench.c
	$(CC) $(CFLAGS) -g $< -o fastmem-mmx  ../libvo/aclib.o -DNAME=\"mmx\"      -DHAVE_MMX
	$(CC) $(CFLAGS) -g $< -o fastmem-k6   ../libvo/aclib.o -DNAME=\"k6\ \"     -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) -g $< -o fastmem-k7   ../libvo/aclib.o -DNAME=\"k7\ \"     -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem-sse  ../libvo/aclib.o -DNAME=\"sse\"      -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem2-mmx ../libvo/aclib.o -DNAME=\"mga-mmx\"  -DHAVE_MGA -DHAVE_MMX
	$(CC) $(CFLAGS) -g $< -o fastmem2-k6  ../libvo/aclib.o -DNAME=\"mga-k6\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW 
	$(CC) $(CFLAGS) -g $< -o fastmem2-k7  ../libvo/aclib.o -DNAME=\"mga-k7\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) -g $< -o fastmem2-sse ../libvo/aclib.o -DNAME=\"mga-sse\"  -DHAVE_MGA -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2

clean:
	rm -f $(OBJS)
	rm -f fastmem-* fastmem2-*
