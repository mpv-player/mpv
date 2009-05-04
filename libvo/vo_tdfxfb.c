/*
 * copyright (C) 2002 Mark Zealey <mark@zealos.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * 30/03/02: An almost total rewrite, added DR support and support for modes
 * other than 16bpp. Fixed the crash when playing multiple files
 * 07/04/02: Fixed DR support, added YUY2 support, fixed OSD stuff.
 * 08/04/02: Fixed a wierd sound corruption problem caused by some optomizations
 * I made.
 * 09/04/02: Fixed a problem with changing the variables passed to draw_slice().
 * Fixed DR support for YV12 et al. Added BGR support. Removed lots of dud code.
 * 10/04/02: Changed the memcpy functions to mem2agpcpy.. should be a tad
 * faster.
 * 11/04/02: Added a compile option so you can watch the film with the console
 * as the background, or not.
 * 13/04/02: Fix rough OSD stuff by rendering it straight onto the output
 * buffer. Added double-buffering. Supports hardware zoom/reduce zoom modes.
 * 13/04/02: Misc cleanups of the code.
 * 22/10/02: Added geometry support to it
 *
 * Hints and tricks:
 * - Use -dr to get direct rendering
 * - Use -vf yuy2 to get yuy2 rendering, *MUCH* faster than yv12
 * - To get a black background and nice smooth OSD, use -double
 * - To get the console as a background, but with scaled OSD, use -nodouble
 * - The driver supports both scaling and shrinking the image using the -x and
 *   -y options on the mplayer commandline. Also repositioning via the -geometry
 *   option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "fastmemcpy.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "drivers/3dfx.h"
#include "aspect.h"
#include "sub.h"

static const vo_info_t info =
{
	"3Dfx Banshee/Voodoo3/Voodoo5",
	"tdfxfb",
	"Mark Zealey <mark@zealos.org>",
	""
};

const LIBVO_EXTERN(tdfxfb)

/* Some registers on the card */
#define S2S_STRECH_BLT		2			// BLT + Strech
#define S2S_IMMED		(1 << 8)		// Do it immediatly
#define S2S_ROP			(0xCC << 24)		// ???

/* Stepping between the different YUV plane registers */
#define YUV_STRIDE 1024
struct YUV_plane {
  char Y[0x0100000];
  char U[0x0100000];
  char V[0x0100000];
};

static int fd = -1;
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_vinfo;
static uint32_t in_width, in_height, in_format, in_depth, in_voodoo_format,
	screenwidth, screenheight, screendepth, vidwidth, vidheight, vidx, vidy,
	vid_voodoo_format, *vidpage, *hidpage, *inpage, vidpageoffset,
	hidpageoffset, inpageoffset, *memBase0 = NULL, *memBase1 = NULL, r_width, r_height;
static volatile voodoo_io_reg *reg_IO;
static volatile voodoo_2d_reg *reg_2d;
static voodoo_yuv_reg *reg_YUV;
static struct YUV_plane *YUV;
static void (*alpha_func)(), (*alpha_func_double)();

static int preinit(const char *arg)
{
	char *name;

	if(arg)
	  name = (char*)arg;
	else if(!(name = getenv("FRAMEBUFFER")))
		name = "/dev/fb0";

	if((fd = open(name, O_RDWR)) == -1) {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_CantOpen, name, strerror(errno));
		return -1;
	}

	if(ioctl(fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo,
				strerror(errno));
		close(fd);
		fd = -1;
		return -1;
	}

	if(ioctl(fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo,
				strerror(errno));
		close(fd);
		fd = -1;
		return -1;
	}

	/* BANSHEE means any of the series aparently */
	if (fb_finfo.accel != FB_ACCEL_3DFX_BANSHEE) {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports);
		close(fd);
		fd = -1;
		return -1;
	}

	// Check the depth now as config() musn't fail
	switch(fb_vinfo.bits_per_pixel) {
	case 16:
	case 24:
	case 32:
	  break; // Ok
	default:
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_OutputIsNotSupported, fb_vinfo.bits_per_pixel);
	  close(fd);
	  fd = -1;
	  return -1;
	}

	/* Open up a window to the hardware */
	memBase1 = mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, 0);
	memBase0 = mmap(0, fb_finfo.mmio_len, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, fb_finfo.smem_len);

	if((long)memBase0 == -1 || (long)memBase1 == -1) {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas, strerror(errno));
		if((long)memBase0 != -1)
		  munmap(memBase0, fb_finfo.smem_len);
		if((long)memBase1 != -1)
		  munmap(memBase1, fb_finfo.smem_len);
		memBase0 = memBase1 = NULL;
		return -1;
	}

	/* Set up global pointers to the voodoo's regs */
	reg_IO = (void *)memBase0 + VOODOO_IO_REG_OFFSET;
	reg_2d = (void *)memBase0 + VOODOO_2D_REG_OFFSET;
	reg_YUV = (void *)memBase0 + VOODOO_YUV_REG_OFFSET;
	YUV = (void *)memBase0 + VOODOO_YUV_PLANE_OFFSET;

	return 0;
}

static void uninit(void)
{
	if(reg_IO) {
		/* Restore the screen (Linux lives at 0) */
		reg_IO->vidDesktopStartAddr = 0;
		reg_IO = NULL;
	}

	/* And close our mess */
	if(memBase1) {
		munmap(memBase1, fb_finfo.smem_len);
		memBase1 = NULL;
	}

	if(memBase0) {
		munmap(memBase0, fb_finfo.mmio_len);
		memBase0 = NULL;
	}

	if(fd != -1) {
		close(fd);
		fd = -1;
	}
}

static void clear_screen(void)
{
	/* There needs to be some sort of delay here or else things seriously
	 * screw up.  Causes the image to not be the right size on screen if
	 * this isn't like this.  A printf before the memset call also seems to
	 * work, but this made more sense since it actually checks the status of
	 * the card.
	 */
	if(vo_doublebuffering) {
		/* first wait for the card to be ready, do not try to write
		 * every time - alex */
		do {} while((reg_IO->status & 0x1f) < 1);
		memset(vidpage, 0, screenwidth * screenheight * screendepth);
		memset(hidpage, 0, screenwidth * screenheight * screendepth);
	}
}

/* Setup output screen dimensions etc */
static void setup_screen(uint32_t full)
{
	aspect(&vidwidth, &vidheight, full ? A_ZOOM : A_NOZOOM);

	geometry(&vidx, &vidy, &vidwidth, &vidheight, screenwidth, screenheight);
	vo_fs = full;
	clear_screen();
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,
		uint32_t flags, char *title, uint32_t format)
{
	screenwidth = fb_vinfo.xres;
	screenheight = fb_vinfo.yres;
	aspect_save_screenres(fb_vinfo.xres,fb_vinfo.yres);

	in_width = width;
	in_height = height;
	in_format = format;
	aspect_save_orig(width,height);

	r_width = d_width;
	r_height = d_height;
	aspect_save_prescale(d_width,d_height);

	/* Setup the screen for rendering to */
	switch(fb_vinfo.bits_per_pixel) {
	case 16:
		screendepth = 2;
		vid_voodoo_format = VOODOO_BLT_FORMAT_16;
		alpha_func_double = vo_draw_alpha_rgb16;
		break;

	case 24:
		screendepth = 3;
		vid_voodoo_format = VOODOO_BLT_FORMAT_24;
		alpha_func_double = vo_draw_alpha_rgb24;
		break;

	case 32:
		screendepth = 4;
		vid_voodoo_format = VOODOO_BLT_FORMAT_32;
		alpha_func_double = vo_draw_alpha_rgb32;
		break;

	default:
		mp_msg(MSGT_VO, MSGL_ERR, MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported, fb_vinfo.bits_per_pixel);
		return -1;
	}

	vid_voodoo_format |= screenwidth * screendepth;

	/* Some defaults here */
	in_voodoo_format = VOODOO_BLT_FORMAT_YUYV;
	in_depth = 2;
	alpha_func = vo_draw_alpha_yuy2;

	switch(in_format) {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YUY2:
		break;
	case IMGFMT_UYVY:
		in_voodoo_format = VOODOO_BLT_FORMAT_UYVY;
		break;
	case IMGFMT_BGR16:
		in_voodoo_format = VOODOO_BLT_FORMAT_16;
		alpha_func = vo_draw_alpha_rgb16;
		break;

	case IMGFMT_BGR24:
		in_depth = 3;
		in_voodoo_format = VOODOO_BLT_FORMAT_24;
		alpha_func = vo_draw_alpha_rgb24;
		break;

	case IMGFMT_BGR32:
		in_depth = 4;
		in_voodoo_format = VOODOO_BLT_FORMAT_32;
		alpha_func = vo_draw_alpha_rgb32;
		break;

	default:
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl);
		return -1;
	}

	in_voodoo_format |= in_width * in_depth;

	/* Linux lives in the first frame */
	if(vo_doublebuffering) {
		vidpageoffset = screenwidth * screenheight * screendepth;
		hidpageoffset = vidpageoffset + screenwidth * screenheight * screendepth;
	} else
		vidpageoffset = hidpageoffset = 0;              /* Console background */


	inpageoffset = hidpageoffset + screenwidth * screenheight * screendepth;

	if(inpageoffset + in_width * in_depth * in_height > fb_finfo.smem_len) {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay);
		return -1;
	}

	vidpage = (void *)memBase1 + (unsigned long)vidpageoffset;
	hidpage = (void *)memBase1 + (unsigned long)hidpageoffset;
	inpage = (void *)memBase1 + (unsigned long)inpageoffset;

	setup_screen(flags & VOFLAG_FULLSCREEN);

	memset(inpage, 0, in_width * in_height * in_depth);

	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_TDFXFB_ScreenIs,
			screenwidth, screenheight, screendepth * 8,
			in_width, in_height, in_depth * 8,
			d_width, d_height);

	return 0;
}

/* Double-buffering draw_alpha */
static void draw_alpha_double(int x, int y, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	char *dst = (char *)vidpage + ((y + vidy) * screenwidth + x + vidx) * screendepth;
	alpha_func_double(w, h, src, srca, stride, dst, screenwidth * screendepth);
}

/* Single-buffering draw_alpha */
static void draw_alpha(int x, int y, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	char *dst = (char *)inpage + (y * in_width + x) * in_depth;
	alpha_func(w, h, src, srca, stride, dst, in_width * in_depth);
}

static void draw_osd(void)
{
	if(!vo_doublebuffering)
		vo_draw_text(in_width, in_height, draw_alpha);
}

/* Render onto the screen */
static void flip_page(void)
{
	voodoo_2d_reg regs = *reg_2d;		/* Copy the regs */
	int i = 0;

	if(vo_doublebuffering) {
		/* Flip to an offscreen buffer for rendering */
		uint32_t t = vidpageoffset;
		void *j = vidpage;

		vidpage = hidpage;
		hidpage = j;
		vidpageoffset = hidpageoffset;
		hidpageoffset = t;
	}

	reg_2d->commandExtra = 0;
	reg_2d->clip0Min = 0;
	reg_2d->clip0Max = 0xffffffff;

	reg_2d->srcBaseAddr = inpageoffset;
	reg_2d->srcXY = 0;
	reg_2d->srcFormat = in_voodoo_format;
	reg_2d->srcSize = XYREG(in_width, in_height);

	reg_2d->dstBaseAddr = vidpageoffset;
	reg_2d->dstXY = XYREG(vidx, vidy);
	reg_2d->dstFormat = vid_voodoo_format;
	reg_2d->dstSize = XYREG(vidwidth, vidheight);
	reg_2d->command = S2S_STRECH_BLT | S2S_IMMED | S2S_ROP;

	/* Wait for the command to finish (If we don't do this, we get wierd
	 * sound corruption... */
	while((reg_IO->status & 0x1f) < 1)
		/* Wait */;

	*((volatile uint32_t *)((uint32_t *)reg_IO + COMMAND_3D)) = COMMAND_3D_NOP;

	while(i < 3)
		if(!(reg_IO->status & STATUS_BUSY))
			i++;

	/* Restore the old regs now */
	reg_2d->commandExtra = regs.commandExtra;
	reg_2d->clip0Min = regs.clip0Min;
	reg_2d->clip0Max = regs.clip0Max;

	reg_2d->srcBaseAddr = regs.srcBaseAddr;
	reg_2d->srcXY = regs.srcXY;
	reg_2d->srcFormat = regs.srcFormat;
	reg_2d->srcSize = regs.srcSize;

	reg_2d->dstBaseAddr = regs.dstBaseAddr;
	reg_2d->dstXY = regs.dstXY;
	reg_2d->dstFormat = regs.dstFormat;
	reg_2d->dstSize = regs.dstSize;

	reg_2d->command = 0;

	/* Render any text onto this buffer */
	if(vo_doublebuffering)
		vo_draw_text(vidwidth, vidheight, draw_alpha_double);

	/* And flip to the new buffer! */
	reg_IO->vidDesktopStartAddr = vidpageoffset;
}

static int draw_frame(uint8_t *src[])
{
	mem2agpcpy(inpage, src[0], in_width * in_depth * in_height);
	return 0;
}

static int draw_slice(uint8_t *i[], int s[], int w, int h, int x, int y)
{
	/* We want to render to the YUV to the input page + the location
	 * of the stripes we're doing */
	reg_YUV->yuvBaseAddr = inpageoffset + in_width * in_depth * y + x;
	reg_YUV->yuvStride = in_width * in_depth;

	/* Put the YUV channels into the voodoos internal combiner unit
	 * thingie */
	mem2agpcpy_pic(YUV->Y, i[0], s[0], h    , YUV_STRIDE, s[0]);
	mem2agpcpy_pic(YUV->U, i[1], s[1], h / 2, YUV_STRIDE, s[1]);
	mem2agpcpy_pic(YUV->V, i[2], s[2], h / 2, YUV_STRIDE, s[2]);
	return 0;
}

/* Attempt to start doing DR */
static uint32_t get_image(mp_image_t *mpi)
{

	if(mpi->flags & MP_IMGFLAG_READABLE)
		return VO_FALSE;
	if(mpi->type == MP_IMGTYPE_STATIC && vo_doublebuffering)
		return VO_FALSE;
	if(mpi->type > MP_IMGTYPE_TEMP)
		return VO_FALSE; // TODO ??

	switch(in_format) {
	case IMGFMT_YUY2:
	case IMGFMT_BGR16:
	case IMGFMT_BGR24:
	case IMGFMT_BGR32:
	case IMGFMT_UYVY:
		mpi->planes[0] = (char *)inpage;
		mpi->stride[0] = in_width * in_depth;
		break;

	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		if(!(mpi->flags & MP_IMGFLAG_ACCEPT_STRIDE) && mpi->w != YUV_STRIDE)
			return VO_FALSE;
		mpi->planes[0] = YUV->Y;
		mpi->planes[1] = YUV->U;
		mpi->planes[2] = YUV->V;
		mpi->stride[0] = mpi->stride[1] = mpi->stride[2] = YUV_STRIDE;
		reg_YUV->yuvBaseAddr = inpageoffset;
		reg_YUV->yuvStride = in_width * in_depth;
		break;

	default:
		return VO_FALSE;
	}

	mpi->width = in_width;
	mpi->flags |= MP_IMGFLAG_DIRECT;

	return VO_TRUE;
}

static int control(uint32_t request, void *data, ...)
{
	switch(request) {
	case VOCTRL_GET_IMAGE:
		return get_image(data);

	case VOCTRL_QUERY_FORMAT:
		switch(*((uint32_t*)data)) {
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
		case IMGFMT_YUY2:
		case IMGFMT_UYVY:
		case IMGFMT_BGR16:
		case IMGFMT_BGR24:
		case IMGFMT_BGR32:
			return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
				VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
		}

		return 0;		/* Not supported */

	case VOCTRL_FULLSCREEN:
		setup_screen(!vo_fs);
		return 0;
	}

	return VO_NOTIMPL;
}

/* Dummy funcs */
static void check_events(void) {}
