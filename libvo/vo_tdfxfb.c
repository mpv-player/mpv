// uncomment this if you pached mplayer.c with mplayer_our_out_buffer_hack.diff
//#define VO_TDFXFB_METHOD 2
#define VO_TDFXFB_METHOD 1
// method: Host-to-Screen bitBLT-ing.
#define HWACCEL_OSD_M2
//#define YV12_CONV_METH
#define DONT_USE_FAST_MEMCPY

/* 
 *    video_out_tdfxfb.c
 *
 *  Copyright (C) Zeljko Stevanovic 2001, <zsteva@ptt.yu>
 *
 *  Most code rewrited, move from /dev/3dfx to /dev/fb0 (kernel 2.4.?)
 *  add support for YUY2 and BGR16 format, remove all X11 DGA code.
 *  - add support for hardware accelerated OSD (buggy for now).
 *    work on BGR16 and YUY2 (VO_3DFX_METHOD == 2 only)
 *  [oct2001]
 *  - added hardware acceleration for OSD (does not look nice, but is faster)
 *    (for YV12 don't fork.)
 *  - fixed YV12 support for ffdivx, but on my cpu this is sllower of yuv2rgb()
 *    try to uncommenting '#define YV12_CONV_METH'
 *  - fast_memcpy() is sllower of memcpy() (why, i don't know)
 *  
 *
 *	Copyright (C) Colin Cross Apr 2000
 *
 *  This file heavily based off of video_out_mga.c of Aaron Holtzman's
 *  mpeg2dec
 *	
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(tdfxfb)

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <wchar.h>
#include <signal.h>

#include <linux/fb.h>

extern int verbose;

//#define LOG(x) syslog(LOG_USER | LOG_DEBUG,x)
#define LOG(x)

#include "drivers/3dfx.h"

// fast_memcpy() is slower of memcpy(), why? i dont know...
#ifndef DONT_USE_FAST_MEMCPY
#include "fastmemcpy.h"
#endif

#ifdef YV12_CONV_METH
#include "../postproc/rgb2rgb.h"
#endif

static vo_info_t vo_info = 
{
	"tdfxfb (/dev/fb?)",
	"tdfxfb",
	"Zeljko Stevanovic <zsteva@ptt.yu>",
	""
};

static char *fb_devname = NULL;
static int fb_fd = -1;
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_vinfo;

static uint32_t in_width;
static uint32_t in_height;
static uint32_t in_format;
static uint32_t in_bytepp;

static uint32_t in_banshee_format,
				in_banshee_size;

static uint32_t screenwidth;
static uint32_t screenheight;
static uint32_t screendepth;
static uint32_t vidwidth, vidheight;	// resize on screen to ... for ration expect...
static uint32_t vidx=0, vidy=0;			// for centring on screen.

static uint32_t vid_banshee_xy,
				vid_banshee_format,
				vid_banshee_size;

static void (*draw_alpha_p)(int w, int h, unsigned char *src,
		unsigned char *srca, int stride, unsigned char *dst,
		int dstride);

static uint32_t *vidpage0;
static uint32_t *vidpage1;
static uint32_t *in_page0;

static uint32_t vidpage0offset;
static uint32_t vidpage1offset;
static uint32_t in_page0_offset;

// Current pointer into framebuffer where display is located
static uint32_t targetoffset;

static uint32_t page_space;

static uint32_t *tdfx_iobase;

static voodoo_io_reg *reg_IO;
static voodoo_2d_reg *reg_2d;
static voodoo_yuv_reg *reg_YUV;
static voodoo_yuv_fb *fb_YUV;

static uint32_t *memBase0, *memBase1;
//static uint32_t baseAddr0, baseAddr1;

//#define BANSHEE_SCREEN_MEMORY		(8*1024*1024)
static uint32_t tdfx_free_offset = 0;

/*- ----------------------------------------------------------------- -*/

/* code get from linux kernel tdfxfb.c by Hannu Mallat */

typedef uint32_t u32;

static inline u32 tdfx_inl(unsigned int reg) {
  return *((volatile uint32_t *)(tdfx_iobase + reg));
}

static inline void tdfx_outl(unsigned int reg, u32 val) {
  *((volatile uint32_t *)(tdfx_iobase + reg)) = val;
}

static inline void banshee_make_room(int size) {
  while((tdfx_inl(STATUS) & 0x1f) < size);
}
 
static inline void banshee_wait_idle(void)
{
  int i = 0;

  banshee_make_room(1);
  tdfx_outl(COMMAND_3D, COMMAND_3D_NOP);

  while(1) {
    i = (tdfx_inl(STATUS) & STATUS_BUSY) ? 0 : i + 1;
    if(i == 3) break;
  }
}


/*- ----------------------------------------------------------------- -*/

static void 
restore_regs(voodoo_2d_reg *regs) 
{
	reg_2d->commandExtra = regs->commandExtra;
	reg_2d->clip0Min = regs->clip0Min;
	reg_2d->clip0Max = regs->clip0Max;

	reg_2d->srcBaseAddr = regs->srcBaseAddr;
	reg_2d->srcXY = regs->srcXY;
	reg_2d->srcFormat = regs->srcFormat;
	reg_2d->srcSize = regs->srcSize;

	reg_2d->dstBaseAddr = regs->dstBaseAddr;
	reg_2d->dstXY = regs->dstXY;
	reg_2d->dstFormat = regs->dstFormat;
	reg_2d->dstSize = regs->dstSize;

	reg_2d->command = 0;
}


static void 
restore(void) 
{
	reg_IO->vidDesktopStartAddr = vidpage0offset;
	//XF86DGADirectVideo(display,0,0);
}

static void 
sighup(int foo) 
{
	reg_IO->vidDesktopStartAddr = vidpage0offset;
	//XF86DGADirectVideo(display,0,0);
	exit(0);
}

#if 0
static void 
dump_yuv_planar(void *y, void *u, void *v,
		uint32_t to, uint32_t px, uint32_t py, uint32_t width, uint32_t height) 
{
	uint32_t j;
	uint32_t *YUV_U, *YUV_V, *YUV_Y;
	uint32_t width2 = width >> 1;
	uint32_t height2 = height >> 1;

	reg_YUV->yuvBaseAddr = to + in_width * 2 * py;
	reg_YUV->yuvStride = width << 1;

	YUV_U = &fb_YUV->U[0];
	YUV_V = &fb_YUV->V[0];
	YUV_Y = &fb_YUV->Y[0];
	for (j = 0; j < height2; j++) 
	{
		memcpy(YUV_U, u, width2);
		memcpy(YUV_V, v, width2);
		memcpy(YUV_Y, y, width); YUV_Y += VOODOO_YUV_STRIDE; y += width;
		memcpy(YUV_Y, y, width); YUV_Y += VOODOO_YUV_STRIDE; y += width;
		YUV_U += VOODOO_YUV_STRIDE; u += width2;
		YUV_V += VOODOO_YUV_STRIDE; v += width2;
	}
}
#endif

#define S2S_BLT(cmd, to, dXY, dFmt, dSize, from, sXY, sFmt, sSize, extCmd)	\
	do { 										\
		voodoo_2d_reg saved_regs = *reg_2d;		\
												\
		reg_2d->commandExtra = (extCmd);		\
		reg_2d->clip0Min = 0;					\
		reg_2d->clip0Max = 0xffffffff;			\
												\
		reg_2d->srcBaseAddr = (from);			\
		reg_2d->srcXY = (sXY);					\
		reg_2d->srcFormat = (sFmt);				\
		reg_2d->srcSize = (sSize);				\
												\
		reg_2d->dstBaseAddr = (to);				\
		reg_2d->dstXY = (dXY);					\
		reg_2d->dstFormat = (dFmt);				\
		reg_2d->dstSize = (dSize);				\
												\
		reg_2d->command = (cmd);				\
												\
		banshee_wait_idle();					\
		restore_regs(&saved_regs);				\
	} while (0)


/*- ----------------------------------------------------------------- -*/

static uint32_t draw_slice_YV12(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_frame_YV12(uint8_t *src[]);
static void flip_page_YV12(void);
static void draw_osd_YV12(void);

static uint32_t draw_slice_YUY2_BGR16(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_frame_YUY2_BGR16(uint8_t *src[]);
static void flip_page_vidpage10(void);
static void draw_osd(void);

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride);
#ifdef HWACCEL_OSD_M2
static void my_draw_alpha_accel(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride);
#endif

static void 
update_target(void) 
{
}

#ifndef VO_TDFXFB_METHOD
#define VO_TDFXFB_METHOD		1
#endif


#if VO_TDFXFB_METHOD == 2
extern void **our_out_buffer;
#endif

static uint32_t 
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,
		uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info) 
{

	if (verbose) {
		printf("vo_3dfx->init( width = %d, height = %d, "
				"d_width = %d, d_height = %d, format = %d)\n",
				width, height, d_width, d_height, format);
		printf("vo_3dfx->init( format => %s )\n", vo_format_name(format));
		printf("vo_3dfx: vo_depthonscreen => %d, vo_screenwidth => %d, "
				"vo_screenhight => %d\n", vo_depthonscreen, vo_screenwidth, vo_screenheight);
		printf("vo_3dfx->init() vo_dwidth => %d, vo_dheight => %d, vo_dbpp => %d\n",
						vo_dwidth, vo_dheight, vo_dbpp);
	}

	if (!fb_devname && !(fb_devname = getenv("FRAMEBUFFER")))
		fb_devname = "/dev/fb0";

	if (verbose)
		printf("vo_3dfx->init(): fbdev ==> %s\n", fb_devname);

	if ((fb_fd = open(fb_devname, O_RDWR)) == -1) {
		printf("vo_3dfx->init(): can't open %s, %s\n", fb_devname, strerror(errno));
		return -1;
	}
	
	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
		printf("vo_3dfx->init(): problem with ioctl(fb_fd, FBITGET_FSCREENINFO.., %s\n",
				strerror(errno));
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		printf("vo_3dfx->init(): problem with ioctl(fb_fd, FBITGET_VSCREENINFO.., %s\n",
				strerror(errno));
		return -1;
	}

	if (verbose) {
		printf("fb_finfo:\n");
		printf("  id: %s\n", fb_finfo.id);
		printf("  frame bufer at %x len %x (%d)\n", fb_finfo.smem_start, fb_finfo.smem_len,
													fb_finfo.smem_len);
		printf("  mem io      at %x len %x\n", fb_finfo.mmio_start, fb_finfo.mmio_len);

		printf("fb_vinfo:\n");
		printf("  resolution:  %dx%d\n", fb_vinfo.xres, fb_vinfo.yres);
		printf("  virtual res: %dx%d\n", fb_vinfo.xres_virtual, fb_vinfo.yres_virtual);
		printf("  virt offset: %dx%d\n", fb_vinfo.xoffset, fb_vinfo.yoffset);
	}

	if (fb_finfo.accel != FB_ACCEL_3DFX_BANSHEE) {
		printf("vo_3dfx->init(): this driver made only for 3dfx banshee... sorry...\n");
		return -1;
	}
	if (fb_vinfo.bits_per_pixel != 16) {
		printf("vo_3dfx->init(): for now fork only in 16 bits mode. use fbset -depth 16 <mode>\n");
		return -1;
	}
	//return -1;


	screenwidth = fb_vinfo.xres;
	screenheight = fb_vinfo.yres;
	screendepth = 2;
	// Store sizes for later
	in_width = width;
	in_height = height;
	in_format = format;

	if (fullscreen) {
		double exrat;

		if (verbose)
			printf("vo_tdfxfb->init(): fullscreen mode...\n");

		vidwidth = screenwidth;
		vidheight = screenheight;
		
		exrat = (double)in_width / in_height;
		if (verbose)
			printf("vo_3dfx->init(): in_width / in_height => %f\n", exrat);
		if (screenwidth / exrat <= screenheight)
			vidheight = (double)screenwidth / exrat;
		else
			vidwidth = (double)screenheight * exrat;

		vidx = (screenwidth - vidwidth) / 2;
		vidy = (screenheight - vidheight) / 2;

		if (verbose) {
			printf("vo_3dfx->init(): vidwidth => %d\n", vidwidth);
			printf("vo_3dfx->init(): vidheight => %d\n", vidheight);
			printf("vo_3dfx->init(): vidx => %d\n", vidx);
			printf("vo_3dfx->init(): vidy => %d\n", vidy);
		}
	} else {
		if (in_width > screenwidth || in_height > screenheight) {
			printf("vo_tdfxfb->init(): your resolution is small for play move...\n");
			return -1;
		} else {
			vidwidth = in_width;
			vidheight = in_height;
			vidx = (screenwidth - in_width) / 2;
			vidy = (screenheight - in_height) / 2;
		}
	}

	signal(SIGALRM,sighup);
	//alarm(120);


	// access to 3dfx hardware.... 
	memBase1 = mmap(0, fb_finfo.smem_len,	 PROT_READ | PROT_WRITE,
									MAP_SHARED, fb_fd, 0);
	memBase0 = mmap(0, fb_finfo.mmio_len,    PROT_READ | PROT_WRITE,
									MAP_SHARED, fb_fd, fb_finfo.smem_len);

	if (memBase0 == (uint32_t *)0xFFFFFFFF ||
			memBase1 == (uint32_t *)0xFFFFFFFF) 
	{
		printf("Couldn't map 3dfx memory areas: %p, %p, %d\n", 
		 memBase0, memBase1, errno);
	}  


	tdfx_iobase = (void *)memBase0 + VOODOO_IO_REG_OFFSET;
	
	// Set up global pointers
	reg_IO  = (void *)memBase0 + VOODOO_IO_REG_OFFSET;
	reg_2d  = (void *)memBase0 + VOODOO_2D_REG_OFFSET;
	reg_YUV = (void *)memBase0 + VOODOO_YUV_REG_OFFSET;
	fb_YUV  = (void *)memBase0 + VOODOO_YUV_PLANE_OFFSET;

	vidpage0offset = 0;
	vidpage1offset = screenwidth * screenheight * screendepth;
	in_page0_offset = vidpage1offset + screenwidth * screenheight * screendepth;

	vidpage0 = (void *)memBase1 + (unsigned long int)vidpage0offset;
	vidpage1 = (void *)memBase1 + (unsigned long int)vidpage1offset;
	in_page0 = (void *)memBase1 + (unsigned long int)in_page0_offset;

	vid_banshee_xy = XYREG(vidx, vidy);
	vid_banshee_format = screenwidth*2 | VOODOO_BLT_FORMAT_16;
	vid_banshee_size = XYREG(vidwidth, vidheight);

	in_banshee_size = XYREG(in_width, in_height);

	//video_out_3dfx.flip_page = flip_page_all;
	draw_alpha_p = vo_draw_alpha_rgb16;

	switch (in_format) {
	case IMGFMT_YV12:
		video_out_tdfxfb.draw_slice = draw_slice_YV12;
		video_out_tdfxfb.draw_frame = draw_frame_YV12;
		video_out_tdfxfb.flip_page = flip_page_YV12;
		video_out_tdfxfb.draw_osd = draw_osd_YV12;
		draw_alpha_p = vo_draw_alpha_yuy2;
		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_YUYV;
#ifdef YV12_CONV_METH
		yuv2rgb_init(16, MODE_RGB);
		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_16;
		draw_alpha_p = vo_draw_alpha_rgb16;
#endif
		break;
	case IMGFMT_YUY2:
		video_out_tdfxfb.draw_slice = draw_slice_YUY2_BGR16;
		video_out_tdfxfb.draw_frame = draw_frame_YUY2_BGR16;
		video_out_tdfxfb.flip_page = flip_page_vidpage10;

		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_YUYV;
		in_bytepp = 2;
#if VO_TDFXFB_METHOD == 2
		*our_out_buffer = in_page0;
#endif

		break;
	case IMGFMT_BGR|16:
		video_out_tdfxfb.draw_slice = draw_slice_YUY2_BGR16;
		video_out_tdfxfb.draw_frame = draw_frame_YUY2_BGR16;
		video_out_tdfxfb.flip_page = flip_page_vidpage10;

		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_16;
		in_bytepp = 2;
#if VO_TDFXFB_METHOD == 2
		*our_out_buffer = in_page0;
#endif
		break;
	}


	// Clear pages 1,2,3 
	// leave page 0, that belongs to X.
	// So does part of 1.  Oops.
	memset(vidpage0, 0x00, screenwidth * screenheight * screendepth);
	memset(vidpage1, 0x00, screenwidth * screenheight * screendepth);
	memset(in_page0, 0x00, in_width * in_height * in_bytepp);

	// Show page 0 (unblanked)
	reg_IO->vidDesktopStartAddr = vidpage0offset;
	//banshee_make_room(1);
	//tdfx_outl(VIDDESKSTART, vidpage1offset);

	/* fd is deliberately not closed - if it were, mmaps might be released??? */

	atexit(restore);

	printf("(display) tdfxfb initialized %p/%p\n",memBase0,memBase1);
	return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

// -------------------------------------------------------------------
// YV12 fork fine. but only on vcd, with ffmpeg codec for DivX don't given corect picture.

static uint32_t 
draw_frame_YV12(uint8_t *src[]) 
{
	return 0;
}

#ifndef YV12_CONV_METH

static uint32_t
draw_slice_YV12(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	void *img_y	= image[0];
	void *img_u = image[1];
	void *img_v = image[2];
	uint32_t j;
	uint32_t *YUV_U = &fb_YUV->U[0],
			*YUV_V = &fb_YUV->V[0],
			*YUV_Y = &fb_YUV->Y[0];
	uint32_t height2 = h >> 1;

#if 0
	printf("stride[0] => %d\n", stride[0]);
	printf("stride[1] => %d\n", stride[1]);
	printf("stride[2] => %d\n", stride[2]);
	printf("w => %d, h => %d, x => %d, y => %d\n", w, h, x, y);
#endif
#if 0
	dump_yuv_planar((uint32_t *)image[0], (uint32_t *)image[1],
			(uint32_t *)image[2], in_page0_offset, x, y, w, h);
#endif

	//reg_YUV->yuvBaseAddr = to + mystride * 2 * py;
	reg_YUV->yuvBaseAddr = in_page0_offset + w * 2 * y;
	reg_YUV->yuvStride = w << 1;

	for (j = 0; j < height2; j++) 
	{
		memcpy(YUV_U, img_u, stride[1]);
		memcpy(YUV_V, img_v, stride[2]);
		memcpy(YUV_Y, img_y, stride[0]); YUV_Y += VOODOO_YUV_STRIDE; img_y += stride[0];
		memcpy(YUV_Y, img_y, stride[0]); YUV_Y += VOODOO_YUV_STRIDE; img_y += stride[0];
		YUV_U += VOODOO_YUV_STRIDE; img_u += stride[1];
		YUV_V += VOODOO_YUV_STRIDE; img_v += stride[2];
	}

	return 0;
}

#else /* !YV12_CONV_METH */
// -------------------------------------------------------------------
// YV12 with converting support

static uint32_t
draw_slice_YV12(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	uint8_t *dest = (uint8_t *)(in_page0) + (in_width * y + x) * 2;
	//dump_yuv_planar((uint32_t *)image[0], (uint32_t *)image[1],
	//		(uint32_t *)image[2], in_page0_offset, x, y, w, h);
	yuv2rgb(dest, image[0], image[1], image[2], w, h, in_width * 2,
			stride[0], stride[1]);
	return 0;
}

#endif /* else ! YV12_CONV_METH */

static void
flip_page_YV12(void)
{
	S2S_BLT(2 | 1 << 8 | 0xcc << 24, // 2 | 1<<8 | 0xcc<<24,
			vidpage0offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page0_offset, 0,
			in_banshee_format, in_banshee_size, 0);

}

static void draw_alpha_YV12(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst = (void *)in_page0 + (in_width * (0+y0) + 0+x0) * 2;	// 2 <= bpp
	uint32_t dstride = in_width * 2; // 2 <= bpp
	//printf("draw_alpha: x0,y0 = %d,%d; w,h = %d,%d; stride=%d;\n", x0, y0, w, h, stride);
	(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
}


static void draw_osd_YV12(void)
{
#ifndef HWACCEL_OSD_M2
	//vo_draw_text(vidwidth, vidheight, draw_alpha);
#else
	//vo_draw_text(vidwidth, vidheight, my_draw_alpha_accel);
#endif /* else ! HWACCEL_OSD_M2 */
	vo_draw_text(in_width, in_height, draw_alpha_YV12);
}



// -------------------------------------------------------------------
// YUYV & BGR16 support

static uint32_t 
draw_frame_YUY2_BGR16(uint8_t *src[]) 
{
#if VO_TDFXFB_METHOD == 1
	memcpy(in_page0, src[0], in_width * in_height * in_bytepp);
#endif
	// blt to offscreen page.
	S2S_BLT(2 | 1 << 8 | 0xcc << 24, // 2 | 1<<8 | 0xcc<<24,
			vidpage1offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page0_offset, 0,
			in_banshee_format, in_banshee_size, 0);
	banshee_wait_idle();
	return 0;
}

static uint32_t
draw_frame_YUY2_BGR16_h2s_bitblt(uint8_t *src[]) 
{
	uint32_t i, len;
	uint32_t *launch = (uint32_t *)&reg_2d->launchArea[0];
	uint32_t *src32 = (uint32_t *)src[0];
	voodoo_2d_reg saved_regs = *reg_2d;

	reg_2d->commandExtra = 0;
	reg_2d->clip0Min = 0;
	reg_2d->clip0Max = 0xffffffff;

	reg_2d->colorFore = 0;
	reg_2d->colorBack = 0;

	reg_2d->srcXY = 0;
	//reg_2d->srcBaseAddr = (from);

//	reg_2d->srcFormat = 0x00400000 | BIT(20); // byte allignment + byte swizzle...
	// YUYV + dword packet
	reg_2d->srcFormat = in_width*2 | VOODOO_BLT_FORMAT_YUYV; // | (2 << 22);
	reg_2d->dstXY = vid_banshee_xy;
	reg_2d->dstSize = vid_banshee_size;
	reg_2d->dstBaseAddr = vidpage1offset;
	reg_2d->dstFormat = vid_banshee_format;

// host-to-screen blting + tranpasparent
	//reg_2d->command = 3 | (1 << 16)| (ROP_COPY << 24);
	reg_2d->command = 3 | (ROP_COPY << 24);

	i = 0;
	len = in_width * in_height * 2;	/* 2 => 16 bit */
	len >>= 2;	/* / 4 */
	for (;;) {
		if (i == len) break; launch[0] = src32[i]; i++;
		if (i == len) break; launch[1] = src32[i]; i++;
		if (i == len) break; launch[2] = src32[i]; i++;
		if (i == len) break; launch[3] = src32[i]; i++;
	}
	banshee_wait_idle();
	restore_regs(&saved_regs);
	return;
}

static uint32_t
draw_slice_YUY2_BGR16(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	return 0;
}

static void
flip_page_vidpage10(void)
{
	uint32_t o;
	void *p;

	// flip screen pages.
	o = vidpage0offset; vidpage0offset = vidpage1offset; vidpage1offset = o;
	p = vidpage0; vidpage0 = vidpage1; vidpage1 = p;

	reg_IO->vidDesktopStartAddr = vidpage0offset;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst = (void *)vidpage1 + (screenwidth * (vidy+y0) + vidx+x0) * 2;	// 2 <= bpp
	uint32_t dstride = screenwidth * 2; // 2 <= bpp
	//printf("draw_alpha: x0,y0 = %d,%d; w,h = %d,%d; stride=%d;\n", x0, y0, w, h, stride);
	(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
}

static void draw_osd(void)
{
#ifndef HWACCEL_OSD_M2
	vo_draw_text(vidwidth, vidheight, draw_alpha);
#else
	vo_draw_text(vidwidth, vidheight, my_draw_alpha_accel);
#endif /* else ! HWACCEL_OSD_M2 */
}

// -------------------------------------------------------------------

static uint32_t
draw_frame(uint8_t *src[])
{
	/* dummy */
	return 0;
}

static uint32_t
draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	/* dummy */
	return 0;
}

static void
flip_page(void)
{
	/* dummy */
}

/*- ----------------------------------------------------------------- -*/

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
        return 4|2; // 4|2;
    case IMGFMT_YUY2:
		if (verbose) printf("query_format: IMGFMT_YUY2\n");
		return 4|2; //4|2;
	case IMGFMT_BGR|16:
		if (verbose) printf("query_format: IMGFMT_BGR|16\n");
		return 4|2; // 4|2;	/* osd + ????? */
    }
    return 0;
}

static void
uninit(void)
{
	reg_IO->vidDesktopStartAddr = vidpage0offset;
}


static void check_events(void)
{
}

#ifdef HWACCEL_OSD_M2

static void my_draw_alpha_accel(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	int y, x;
	uint32_t pbuf, pcnt;
	uint32_t *launch = (uint32_t *)&reg_2d->launchArea[0];
	voodoo_2d_reg saved_regs = *reg_2d;

	reg_2d->commandExtra = 0;
	reg_2d->clip0Min = 0;
	reg_2d->clip0Max = 0xffffffff;

	reg_2d->colorFore = 0xffff;
	reg_2d->colorBack = 0;

	reg_2d->srcXY = 0;
	//reg_2d->srcBaseAddr = (from);

	reg_2d->srcFormat = 0x00400000 | BIT(20); // byte allignment + byte swizzle...
	//reg_2d->srcSize = XYREG(w, h);
	reg_2d->dstSize = XYREG(w, h);

	reg_2d->dstBaseAddr = vidpage1offset;
	reg_2d->dstXY = XYREG(vidx+x0, vidy+y0);
	reg_2d->dstFormat = vid_banshee_format;

// host-to-screen blting + tranpasparent
	reg_2d->command = 3 | (1 << 16)| (ROP_COPY << 24);

	pcnt = 0;
	pbuf = 0;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			pbuf = (pbuf << 1) | (((src[x] > 150) ? 1 : 0));
			if (++pcnt == 32) { launch[0] = pbuf; pcnt = 0; pbuf = 0; }
		}

		if ((pcnt % 8) != 0) { 
			pbuf <<= 8 - (pcnt % 8);
			pcnt += 8 - (pcnt % 8);
			if (pcnt == 32) { launch[0] = pbuf; pcnt = 0; pbuf = 0; }
		}
			
		src += stride;
		srca += stride;
	}
	if (pcnt != 0) launch[0] = pbuf;

	banshee_wait_idle();
	restore_regs(&saved_regs);
	return;
}
#endif /* ! HWACCEL_OSD_M2 */

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_tdfxfb: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
