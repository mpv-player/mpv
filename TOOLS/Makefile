OBJS = asfinfo avi-fix bios2dump cpuinfo dump_mp4 mem2dump movinfo png2raw subrip fastmemcpybench

all: $(OBJS)

png2raw: png2raw.c
	cc png2raw.c -o png2raw -lpng

subrip: subrip.c
	cc -g -I.. -o subrip subrip.c ../vobsub.o ../spudec.o ../mp_msg.o ../unrarlib.o ../postproc/swscale.o ../postproc/rgb2rgb.o ../postproc/yuv2rgb.o ../libmpcodecs/img_format.o -lm

vivodump: vivodump.c
	cc -I.. -o vivodump vivodump.c ../mp_msg.o ../libmpdemux/libmpdemux.a

fastmemcpybench: fastmemcpybench.c
	cc -g ../libvo/aclib.o -DNAME=\"mmx\"      -DHAVE_MMX fastmemcpybench.c -o fastmem-mmx
	cc -g ../libvo/aclib.o -DNAME=\"k6\ \"     -DHAVE_3DNOW -DHAVE_MMX fastmemcpybench.c -o fastmem-k6
	cc -g ../libvo/aclib.o -DNAME=\"k7\ \"     -DHAVE_MMX2  -DHAVE_3DNOW -DHAVE_MMX fastmemcpybench.c -o fastmem-k7
	cc -g ../libvo/aclib.o -DNAME=\"sse\"      -DHAVE_MMX2  -DHAVE_SSE   -DHAVE_MMX fastmemcpybench.c -o fastmem-sse
	cc -g ../libvo/aclib.o -DNAME=\"mga-mmx\"  -DHAVE_MGA   -DHAVE_MMX fastmemcpybench.c -o fastmem2-mmx
	cc -g ../libvo/aclib.o -DNAME=\"mga-k6\ \" -DHAVE_MGA   -DHAVE_3DNOW -DHAVE_MMX fastmemcpybench.c -o fastmem2-k6
	cc -g ../libvo/aclib.o -DNAME=\"mga-k7\ \" -DHAVE_MGA   -DHAVE_MMX2  -DHAVE_3DNOW -DHAVE_MMX fastmemcpybench.c -o fastmem2-k7
	cc -g ../libvo/aclib.o -DNAME=\"mga-sse\"  -DHAVE_MGA   -DHAVE_MMX2  -DHAVE_SSE   -DHAVE_MMX fastmemcpybench.c -o fastmem2-sse

clean:
	rm -f $(OBJS)
	rm -f fastmem-mmx fastmem-k6 fastmem-k7 fastmem-sse fastmem2-mmx fastmem2-k6 fastmem2-k7 fastmem2-sse
