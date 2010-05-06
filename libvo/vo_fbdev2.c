/*
 * video driver for framebuffer device
 * copyright (C) 2003 Joey Parrish <joey@nicewarrior.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#include "mp_msg.h"

static const vo_info_t info = {
	"Framebuffer Device",
	"fbdev2",
	"Joey Parrish <joey@nicewarrior.org>",
	""
};

const LIBVO_EXTERN(fbdev2)

static void set_bpp(struct fb_var_screeninfo *p, int bpp)
{
	p->bits_per_pixel = (bpp + 1) & ~1;
	p->red.msb_right = p->green.msb_right = p->blue.msb_right = p->transp.msb_right = 0;
	p->transp.offset = p->transp.length = 0;
	p->blue.offset = 0;
	switch (bpp) {
		case 32:
			p->transp.offset = 24;
			p->transp.length = 8;
		case 24:
			p->red.offset = 16;
			p->red.length = 8;
			p->green.offset = 8;
			p->green.length = 8;
			p->blue.length = 8;
			break;
		case 16:
			p->red.offset = 11;
			p->green.length = 6;
			p->red.length = 5;
			p->green.offset = 5;
			p->blue.length = 5;
			break;
		case 15:
			p->red.offset = 10;
			p->green.length = 5;
			p->red.length = 5;
			p->green.offset = 5;
			p->blue.length = 5;
			break;
		case 12:
			p->red.offset   = 8;
			p->green.length = 4;
			p->red.length   = 4;
			p->green.offset = 4;
			p->blue.length  = 4;
			break;
	}
}

static char *fb_dev_name = NULL; // such as /dev/fb0
static int fb_dev_fd; // handle for fb_dev_name
static uint8_t *frame_buffer = NULL; // mmap'd access to fbdev
static uint8_t *center = NULL; // where to begin writing our image (centered?)
static struct fb_fix_screeninfo fb_finfo; // fixed info
static struct fb_var_screeninfo fb_vinfo; // variable info
static struct fb_var_screeninfo fb_orig_vinfo; // variable info to restore later
static unsigned short fb_ored[256], fb_ogreen[256], fb_oblue[256];
static struct fb_cmap fb_oldcmap = { 0, 256, fb_ored, fb_ogreen, fb_oblue };
static int fb_cmap_changed = 0; //  to restore map
static int fb_pixel_size;	// 32:  4  24:  3  16:  2  15:  2
static int fb_bpp;		// 32: 32  24: 24  16: 16  15: 15
static size_t fb_size; // size of frame_buffer
static int fb_line_len; // length of one line in bytes
static void (*draw_alpha_p)(int w, int h, unsigned char *src,
		unsigned char *srca, int stride, unsigned char *dst,
		int dstride);

static uint8_t *next_frame = NULL; // for double buffering
static int in_width;
static int in_height;
static int out_width;
static int out_height;

static struct fb_cmap *make_directcolor_cmap(struct fb_var_screeninfo *var)
{
  int i, cols, rcols, gcols, bcols;
  uint16_t *red, *green, *blue;
  struct fb_cmap *cmap;

  rcols = 1 << var->red.length;
  gcols = 1 << var->green.length;
  bcols = 1 << var->blue.length;

  /* Make our palette the length of the deepest color */
  cols = (rcols > gcols ? rcols : gcols);
  cols = (cols > bcols ? cols : bcols);

  red = malloc(cols * sizeof(red[0]));
  if(!red) {
	  mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't allocate red palette with %d entries.\n", cols);
	  return NULL;
  }
  for(i=0; i< rcols; i++)
    red[i] = (65535/(rcols-1)) * i;

  green = malloc(cols * sizeof(green[0]));
  if(!green) {
	  mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't allocate green palette with %d entries.\n", cols);
	  free(red);
	  return NULL;
  }
  for(i=0; i< gcols; i++)
    green[i] = (65535/(gcols-1)) * i;

  blue = malloc(cols * sizeof(blue[0]));
  if(!blue) {
	  mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't allocate blue palette with %d entries.\n", cols);
	  free(red);
	  free(green);
	  return NULL;
  }
  for(i=0; i< bcols; i++)
    blue[i] = (65535/(bcols-1)) * i;

  cmap = malloc(sizeof(struct fb_cmap));
  if(!cmap) {
	  mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't allocate color map\n");
	  free(red);
	  free(green);
	  free(blue);
	  return NULL;
  }
  cmap->start = 0;
  cmap->transp = 0;
  cmap->len = cols;
  cmap->red = red;
  cmap->blue = blue;
  cmap->green = green;
  cmap->transp = NULL;

  return cmap;
}

static int fb_preinit(int reset)
{
	static int fb_preinit_done = 0;
	static int fb_err = -1;

	if (reset) {
		fb_preinit_done = 0;
		return 0;
	}

	if (fb_preinit_done)
		return fb_err;
	fb_preinit_done = 1;

	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name = strdup("/dev/fb0");

	mp_msg(MSGT_VO, MSGL_V, "[fbdev2] Using device %s\n", fb_dev_name);

	if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
		mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't open %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out;
	}
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out;
	}
	fb_orig_vinfo = fb_vinfo;

	fb_bpp = fb_vinfo.bits_per_pixel;

	/* 16 and 15 bpp is reported as 16 bpp */
	if (fb_bpp == 16)
		fb_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
			fb_vinfo.blue.length;

	fb_err = 0;
	return 0;
err_out:
	if (fb_dev_fd >= 0) close(fb_dev_fd);
	fb_dev_fd = -1;
	fb_err = -1;
	return -1;
}

static int preinit(const char *subdevice)
{
	if (subdevice)
	{
	    if (fb_dev_name) free(fb_dev_name);
	    fb_dev_name = strdup(subdevice);
	}
	return fb_preinit(0);
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t flags, char *title,
		uint32_t format)
{
	struct fb_cmap *cmap;
	int fs = flags & VOFLAG_FULLSCREEN;

	out_width = width;
	out_height = height;
	in_width = width;
	in_height = height;

	if (fs) {
		out_width = fb_vinfo.xres;
		out_height = fb_vinfo.yres;
	}

	if (out_width < in_width || out_height < in_height) {
		mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Screensize is smaller than video size (%dx%d < %dx%d)\n",
		    out_width, out_height, in_width, in_height);
		return 1;
	}

	switch (fb_bpp) {
		case 32: draw_alpha_p = vo_draw_alpha_rgb32; break;
		case 24: draw_alpha_p = vo_draw_alpha_rgb24; break;
		case 16: draw_alpha_p = vo_draw_alpha_rgb16; break;
		case 15: draw_alpha_p = vo_draw_alpha_rgb15; break;
		case 12: draw_alpha_p = vo_draw_alpha_rgb12; break;
		default: return 1;
	}

	if (vo_config_count == 0) {
		if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
			mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't get FSCREENINFO: %s\n", strerror(errno));
			return 1;
		}

		if (fb_finfo.type != FB_TYPE_PACKED_PIXELS) {
			mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] type %d not supported\n", fb_finfo.type);
			return 1;
		}

		switch (fb_finfo.visual) {
			case FB_VISUAL_TRUECOLOR:
				break;
			case FB_VISUAL_DIRECTCOLOR:
				mp_msg(MSGT_VO, MSGL_V, "[fbdev2] creating cmap for directcolor\n");
				if (ioctl(fb_dev_fd, FBIOGETCMAP, &fb_oldcmap)) {
					mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] can't get cmap: %s\n", strerror(errno));
					return 1;
				}
				if (!(cmap = make_directcolor_cmap(&fb_vinfo)))
					return 1;
				if (ioctl(fb_dev_fd, FBIOPUTCMAP, cmap)) {
					mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] can't put cmap: %s\n", strerror(errno));
					return 1;
				}
				fb_cmap_changed = 1;
				free(cmap->red);
				free(cmap->green);
				free(cmap->blue);
				free(cmap);
				break;
			default:
				mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] visual: %d not yet supported\n", fb_finfo.visual);
				return 1;
		}

		fb_size = fb_finfo.smem_len;
		fb_line_len = fb_finfo.line_length;
		if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
			mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
			return 1;
		}
	}

	center = frame_buffer +
	         ( (out_width - in_width) / 2 ) * fb_pixel_size +
		 ( (out_height - in_height) / 2 ) * fb_line_len;

#ifndef USE_CONVERT2FB
	if (!(next_frame = realloc(next_frame, in_width * in_height * fb_pixel_size))) {
		mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't malloc next_frame: %s\n", strerror(errno));
		return 1;
	}
#endif
	if (fs) memset(frame_buffer, '\0', fb_line_len * fb_vinfo.yres);

	return 0;
}

static int query_format(uint32_t format)
{
	// open the device, etc.
	if (fb_preinit(0)) return 0;
	if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		int fb_target_bpp = format & 0xff;
		set_bpp(&fb_vinfo, fb_target_bpp);
		fb_vinfo.xres_virtual = fb_vinfo.xres;
		fb_vinfo.yres_virtual = fb_vinfo.yres;
		if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo))
			// Needed for Intel framebuffer with 32 bpp
			fb_vinfo.transp.length = fb_vinfo.transp.offset = 0;
		if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo)) {
			mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't put VSCREENINFO: %s\n", strerror(errno));
			return 0;
		}
		fb_pixel_size = fb_vinfo.bits_per_pixel / 8;
		fb_bpp = fb_vinfo.bits_per_pixel;
		if (fb_bpp == 16)
			fb_bpp = fb_vinfo.red.length + fb_vinfo.green.length + fb_vinfo.blue.length;
		if (fb_bpp == fb_target_bpp)
			return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_ACCEPT_STRIDE;
	}
	return 0;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst;
	int dstride;

#ifdef USE_CONVERT2FB
	dst = center + (fb_line_len * y0) + (x0 * fb_pixel_size);
	dstride = fb_line_len;
#else
	dst = next_frame + (in_width * y0 + x0) * fb_pixel_size;
	dstride = in_width * fb_pixel_size;
#endif
	(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
}

static void draw_osd(void)
{
	vo_draw_text(in_width, in_height, draw_alpha);
}

// all csp support stride
static int draw_frame(uint8_t *src[]) { return 1; }

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
	uint8_t *in = src[0];
#ifdef USE_CONVERT2FB
	uint8_t *dest = center + (fb_line_len * y) + (x * fb_pixel_size);
	int next = fb_line_len;
#else
	uint8_t *dest = next_frame + (in_width * y + x) * fb_pixel_size;
	int next = in_width * fb_pixel_size;
#endif
	int i;

	for (i = 0; i < h; i++) {
		fast_memcpy(dest, in, w * fb_pixel_size);
		dest += next;
		in += stride[0];
	}
	return 0;
}

static void check_events(void)
{
}

static void flip_page(void)
{
#ifndef USE_CONVERT2FB
	int i, out_offset = 0, in_offset = 0;

	for (i = 0; i < in_height; i++) {
		fast_memcpy(center + out_offset, next_frame + in_offset,
				in_width * fb_pixel_size);
		out_offset += fb_line_len;
		in_offset += in_width * fb_pixel_size;
	}
#endif
}

static void uninit(void)
{
	if (fb_cmap_changed) {
		if (ioctl(fb_dev_fd, FBIOPUTCMAP, &fb_oldcmap))
			mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't restore original cmap\n");
		fb_cmap_changed = 0;
	}
	if(next_frame) free(next_frame);
	if (fb_dev_fd >= 0) {
		if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_orig_vinfo))
			mp_msg(MSGT_VO, MSGL_ERR, "[fbdev2] Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
		close(fb_dev_fd);
		fb_dev_fd = -1;
	}
	if(frame_buffer) munmap(frame_buffer, fb_size);
	next_frame = frame_buffer = NULL;
	fb_preinit(1); // so that later calls to preinit don't fail
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
