
/* 
 *    video_out_tdfxfb.c
 *
 *  Copyright (C) Zeljko Stevanovic 2001, <zsteva@ptt.yu>
 *
 *  Most code rewrited, move from /dev/3dfx to /dev/fb0 (kernel 2.4.?)
 *  add support for YUY2 and BGR16 format, remove all X11 DGA code.
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

#include "fastmemcpy.h"

static vo_info_t vo_info = 
{
	"tdfxfb (/dev/fb?)",
	"tdfxfb",
	"Zeljko Stevanovic <zsteva@ptt.yu>, bassed on vo_3dfx of Colin Cross <colin@MIT.EDU>",
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
static uint32_t *osd_page;
static uint32_t *in_page0;

static uint32_t vidpage0offset;
static uint32_t vidpage1offset;
static uint32_t osd_page_offset;
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
//static uint32_t tdfx_free_scrmem = 0;

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
	//reg_IO->vidDesktopStartAddr = vidpage0offset;
	//XF86DGADirectVideo(display,0,0);
}

static void 
sighup(int foo) 
{
	//reg_IO->vidDesktopStartAddr = vidpage0offset;
	//XF86DGADirectVideo(display,0,0);
	exit(0);
}

#if 1
static void 
dump_yuv_planar(void *y, void *u, void *v,
		uint32_t to, uint32_t px, uint32_t py, uint32_t width, uint32_t height) 
{
	// YUV conversion works like this:
	//
	// We write the Y, U, and V planes separately into 3dfx YUV Planar memory
	// region.  The nice chip then takes these and packs them into the YUYV
	// format in the regular frame buffer, starting at yuvBaseAddr, page 2 here.
	// Then we tell the 3dfx to do a Screen to Screen Stretch BLT to copy all 
	// of the data on page 2 onto page 1, converting it to 16 bpp RGB as
	// it goes. The result is a nice image on page 1 ready for display. 

	uint32_t j;
	uint32_t y_imax, uv_imax, jmax;


	//printf("dump_yuv_planar(..., px=%d, py=%d, w=%d, h=%d\n",
	//				px, py, width, height);

	reg_YUV->yuvBaseAddr = to + in_width * 2 * py;
	reg_YUV->yuvStride = width*2;

	jmax = height >> 1;		// vidheight/2, height of U and V planes
	y_imax = width;			// Y plane is twice as wide as U and V planes
	uv_imax = width >> 1;	// in_width/2/4, width of U and V planes in 32-bit words

	for (j = 0; j < jmax; j++) 
	{
#if 0
		//XXX this should be hand-rolled 32 bit memcpy for safeness.
		memcpy(fb_YUV->U (uint32_t) VOODOO_YUV_STRIDE *  j,
				((uint8_t*)u) + uv_imax *  j       , uv_imax);
		
		memcpy(fb_YUV->V + (uint32_t) VOODOO_YUV_STRIDE *  j,
				((uint8_t*)v) + uv_imax *  j       , uv_imax);

		memcpy(fb_YUV->Y + (uint32_t) VOODOO_YUV_STRIDE* (j<<1),
				((uint8_t*)y) + y_imax * (j<<1)   , y_imax);
		memcpy(fb_YUV->Y + (uint32_t) VOODOO_YUV_STRIDE*((j<<1)+1),
				((uint8_t*)y) + y_imax *((j<<1)+1), y_imax);
#else
		memcpy(&fb_YUV->U[VOODOO_YUV_STRIDE *  j], u + uv_imax *  j       , uv_imax);
		memcpy(&fb_YUV->V[VOODOO_YUV_STRIDE *  j], v + uv_imax *  j       , uv_imax);


		memcpy(&fb_YUV->Y[VOODOO_YUV_STRIDE* (j<<1)], y + y_imax * (j<<1)   , y_imax);
		memcpy(&fb_YUV->Y[VOODOO_YUV_STRIDE*((j<<1)+1)], y + y_imax *((j<<1)+1), y_imax);
#endif
	}
}
#endif

#define S2S_BLT(cmd, to, dXY, dFmt, dSize, from, sXY, sFmt, sSize)	\
	do { 										\
		voodoo_2d_reg saved_regs = *reg_2d;		\
												\
		reg_2d->commandExtra = 0;				\
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
		restore_regs(&saved_regs);				\
	} while (0)


#define VOODOO_BLT_FORMAT_24			(4 << 16)

/*- ----------------------------------------------------------------- -*/

static uint32_t draw_slice_YV12(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_frame_YV12(uint8_t *src[]);
static void flip_page_YV12(void);
static void draw_osd_YV12(void);

static uint32_t draw_slice_YUY2_BGR16(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_frame_YUY2_BGR16(uint8_t *src[]);
static void flip_page_YUY2_BGR16(void);
static void draw_osd_YUY2_BGR16(void);

#if 0
static uint32_t draw_frame_YUY2_2(uint8_t *src[]);
static uint32_t draw_slice_YUY2(uint8_t *image[], int stride[], int w,int h,int x,int y);
static void flip_page_all(void);
static void flip_page_YUY2(void);
static void flip_page_YUY2_2(void);
#endif

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride);

static void 
update_target(void) 
{
}

#ifndef VO_3DFX_METHOD
#define VO_3DFX_METHOD		1
#endif


#if VO_3DFX_METHOD == 2
extern void **our_out_buffer;
#endif

static uint32_t 
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,
		uint32_t fullscreen, char *title, uint32_t format) 
{

	if (!fullscreen) return -1;
	if (1 || verbose) {
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

	if (1 || verbose)
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


	//screenwidth = 800;
	//screenheight = 600;
	//screendepth = 2;
	screenwidth = fb_vinfo.xres;
	screenheight = fb_vinfo.yres;
	screendepth = 2;
	// Store sizes for later
	in_width = width;
	in_height = height;
	in_format = format;

	vidwidth = screenwidth;
	vidheight = screenheight;
	//vidwidth = in_width;
	//vidheight = in_height;
	if (1) {
		double exrat;
		
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
	//osd_page_offset = vidpage1offset + screenwidth * screenheight * screendepth;
	//in_page0_offset = osd_page_offset + screenwidth * screenheight * screendepth;
	in_page0_offset = vidpage1offset + screenwidth * screenheight * screendepth;

	vidpage0 = (void *)memBase1 + (unsigned long int)vidpage0offset;
	vidpage1 = (void *)memBase1 + (unsigned long int)vidpage1offset;
	//osd_page = (void *)memBase1 + (unsigned long int)osd_page_offset;
	in_page0 = (void *)memBase1 + (unsigned long int)in_page0_offset;

	vid_banshee_xy = XYREG(vidx, vidy);
	vid_banshee_format = screenwidth*2 | VOODOO_BLT_FORMAT_16;
	vid_banshee_size = XYREG(vidwidth, vidheight);

	in_banshee_size = XYREG(in_width, in_height);

	//video_out_tdfxfb.flip_page = flip_page_all;
	draw_alpha_p = NULL;

	switch (in_format) {
	case IMGFMT_YV12:
		video_out_tdfxfb.draw_slice = draw_slice_YV12;
		video_out_tdfxfb.draw_frame = draw_frame_YV12;
		video_out_tdfxfb.flip_page = flip_page_YV12;
		video_out_tdfxfb.draw_osd = draw_osd_YV12;
		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_YUYV;
		break;
	case IMGFMT_YUY2:
		video_out_tdfxfb.draw_slice = draw_slice_YUY2_BGR16;
		video_out_tdfxfb.draw_frame = draw_frame_YUY2_BGR16;
		video_out_tdfxfb.flip_page = flip_page_YUY2_BGR16;
		video_out_tdfxfb.draw_osd = draw_osd_YUY2_BGR16;

#if VO_3DFX_METHOD == 1
		draw_alpha_p = vo_draw_alpha_yuy2;
#endif
#if VO_3DFX_METHOD == 2
		*our_out_buffer = in_page0;
		draw_alpha_p = vo_draw_alpha_rgb16;
#endif
		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_YUYV;
		in_bytepp = 2;
		break;
	case IMGFMT_BGR|16:
		video_out_tdfxfb.draw_slice = draw_slice_YUY2_BGR16;
		video_out_tdfxfb.draw_frame = draw_frame_YUY2_BGR16;
		video_out_tdfxfb.flip_page = flip_page_YUY2_BGR16;
		video_out_tdfxfb.draw_osd = draw_osd_YUY2_BGR16;
#if VO_3DFX_METHOD == 2
		*our_out_buffer = in_page0;
#endif
		draw_alpha_p = vo_draw_alpha_rgb16;
		in_banshee_format = in_width * 2 | VOODOO_BLT_FORMAT_16;
		in_bytepp = 2;
		break;
	case IMGFMT_BGR|24:
		// FIXME: !!!!
		//video_out_tdfxfb.draw_frame = draw_frame_BGR24;
		video_out_tdfxfb.draw_frame = draw_frame; // draw_frame_BGR24;
		//*our_out_buffer = vidpage1;

		in_banshee_format = in_width * 3 | VOODOO_BLT_FORMAT_24;
		in_bytepp = 3;
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

	printf("(display) 3dfx initialized %p/%p\n",memBase0,memBase1);
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
	//printf("vo_3dfx->draw_frame_YV12\n");
	return 0;
}

static uint32_t
draw_slice_YV12(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	dump_yuv_planar((uint32_t *)image[0], (uint32_t *)image[1],
			(uint32_t *)image[2], in_page0_offset, x, y, w, h);
	return 0;
}

static void
flip_page_YV12(void)
{
	S2S_BLT(2 | 1 << 8 | 0xcc << 24, // 2 | 1<<8 | 0xcc<<24,
			vidpage0offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page0_offset, 0,
			in_banshee_format, in_banshee_size);
}

static void draw_alpha_YV12(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst = (void *)in_page0 + (in_width * y0 + x0) * 2;	// 2 <= bpp
	uint32_t dstride = in_width * 2; // 2 <= bpp

	//printf("draw_alpha: x0,y0 = %d,%d; w,h = %d,%d;\n", x0, y0, w, h);
	//(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
	vo_draw_alpha_yuy2(w, h, src, srca, stride, dst, dstride);
}


static void draw_osd_YV12(void)
{
	vo_draw_text(in_width, in_height, draw_alpha_YV12);
}


// -------------------------------------------------------------------
// YUYV & BGR16 support

static uint32_t 
draw_frame_YUY2_BGR16(uint8_t *src[]) 
{
#if VO_3DFX_METHOD == 1
	memcpy(in_page0, src[0], in_width * in_height * in_bytepp);
#endif
#if VO_3DFX_METHOD == 2
	// blt to offscreen page.
	S2S_BLT(2 | 1 << 8 | 0xcc << 24, // 2 | 1<<8 | 0xcc<<24,
			vidpage1offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page0_offset, 0,
			in_banshee_format, in_banshee_size);
	banshee_wait_idle();
#endif
	return 0;
}

static uint32_t
draw_slice_YUY2_BGR16(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	return 0;
}

static void
flip_page_YUY2_BGR16(void)
{
#if VO_3DFX_METHOD == 1
	S2S_BLT(2 | 1 << 8 | 0xcc << 24, // 2 | 1<<8 | 0xcc<<24,
			vidpage0offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page0_offset, 0,
			in_banshee_format, in_banshee_size);
	banshee_wait_idle();
#endif
#if VO_3DFX_METHOD == 2
	uint32_t o;
	void *p;

	// flip screen pages.
	o = vidpage0offset; vidpage0offset = vidpage1offset; vidpage1offset = o;
	p = vidpage0; vidpage0 = vidpage1; vidpage1 = p;

	reg_IO->vidDesktopStartAddr = vidpage0offset;
#endif
}

static void draw_alpha_YUY2_BGR16(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
#if VO_3DFX_METHOD == 1
	unsigned char *dst = (void *)in_page0 + (in_width * y0 + x0) * 2;	// 2 <= bpp
	uint32_t dstride = in_width * 2; // 2 <= bpp
#endif
#if VO_3DFX_METHOD == 2
	unsigned char *dst = (void *)vidpage1 + (screenwidth * (vidy+y0) + vidx+x0) * 2;	// 2 <= bpp
	uint32_t dstride = screenwidth * 2; // 2 <= bpp
#endif
	//printf("draw_alpha: x0,y0 = %d,%d; w,h = %d,%d;\n", x0, y0, w, h);
	(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
}

static void draw_osd_YUY2_BGR16(void)
{
#if VO_3DFX_METHOD == 1
	vo_draw_text(in_width, in_height, draw_alpha_YUY2_BGR16);
#endif
#if VO_3DFX_METHOD == 2
	//vo_draw_text(screenwidth, screenheight, draw_alpha_YUY2_BGR16);
	//vo_draw_text(vidwidth, vidheight, draw_alpha_YUY2_BGR16);
	vo_draw_text(vidwidth, vidheight, draw_alpha_YUY2_BGR16);
#endif
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

#if 0
static void
flip_page_all(void)
{
	S2S_BLT(2 | 1 << 8 | 0xcc << 24, // 2 | 1<<8 | 0xcc<<24,
			vidpage0offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page0_offset, 0,
			in_banshee_format, in_banshee_size);
}


static void
flip_page_YUY2_2(void)
{
	void *p; 
	uint32_t o;

	/* flip screen buffer */
	p = vidpage0; vidpage0 = vidpage1; vidpage1 = p;
	o = vidpage0offset; vidpage0offset = vidpage1offset; vidpage1offset = o;
	reg_IO->vidDesktopStartAddr = vidpage0offset;


	//banshee_make_room(1);
	//tdfx_outl(VIDDESKSTART, vidpage0offset);
	//banshee_wait_idle();

#if 0
	S2S_BLT(2 | 1 << 8 | 0xCC << 24,
			vidpage0offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			in_page1_offset, 0,
			in_banshee_format, in_banshee_size);
	banshee_wait_idle();
#endif
#if 0
	S2S_BLT(2 | 1 << 8 | 0xCC << 24,
			vidpage0offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size,
			vidpage1offset, vid_banshee_xy,
			vid_banshee_format, vid_banshee_size);

	banshee_wait_idle();
#endif
#if 0
	banshee_make_room(3+9);

	tdfx_outl(COMMANDEXTRA_2D, 4);
	tdfx_outl(CLIP0MIN, 0);
	tdfx_outl(CLIP0MAX, 0xffffffff);
	tdfx_outl(SRCBASE, page_0_offset);
	tdfx_outl(SRCXY, 0);
	tdfx_outl(SRCFORMAT, in_banshee_format);
	tdfx_outl(SRCSIZE, in_banshee_size);

	tdfx_outl(DSTBASE, vidpage0offset);
	tdfx_outl(DSTXY, vid_banshee_xy);
	tdfx_outl(DSTFORMAT, vid_banshee_format);
	tdfx_outl(DSTSIZE, vid_banshee_size);

	tdfx_outl(COMMAND_2D, 2 | 1<<8 | 0xcc << 24);
	banshee_wait_idle();
#endif
}
#endif


static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
        return 4|2; // 4|2;
    case IMGFMT_YUY2:
		if (verbose) printf("query_format: IMGFMT_YUY2\n");
		return 0; //4|2;
    case IMGFMT_RGB|24:
		if (verbose) printf("query_format: IMGFMT_RGB|24\n");
		return 0;
    case IMGFMT_BGR|24:
		if (verbose) printf("query_format: IMGFMT_BGR|24\n");
		return 0;
	case IMGFMT_BGR|16:
		return 4|2; // 4|1;	/* osd + ????? */
    }
    return 0;
}

static void
uninit(void)
{
}


static void check_events(void)
{
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst = (void *)vidpage1 + (in_width * y0 + x0) * 2;	// 2 <= bpp
	uint32_t dstride = in_width * 2; // 2 <= bpp

	//printf("draw_alpha: x0,y0 = %d,%d; w,h = %d,%d;\n", x0, y0, w, h);
	//(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
	vo_draw_alpha_rgb16(w, h, src, srca, stride, dst, dstride);
}

static void draw_osd(void)
{
#if 1
	vo_draw_text(vidwidth, vidheight, draw_alpha);
#else
	zz_draw_text(in_width, in_height, draw_alpha);
	S2S_BLT(2 | 1 << 8 | 0xCC << 24,
			in_page1_offset, in_banshee_xy,
			in_banshee_format, in_banshee_size,
			in_pageT_offset, 0,
			in_banshee_format, in_banshee_size);
	banshee_wait_idle();
#endif
}


