/*
  Video driver for SVGAlib - alpha, slow
  by Zoltan Mark Vician <se7en@sch.bme.hu>
  Code started: Mon Apr  1 23:25:47 2001
  
  Uses HW acceleration if your card is supported by SVGAlib.
*/

#include <stdio.h>
#include <stdlib.h>

#include <vga.h>
#include <vgagl.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "yuv2rgb.h"
#include "mmx.h"

extern void rgb15to16_mmx(char* s0,char* d0,int count);

LIBVO_EXTERN(svga)

static vo_info_t vo_info = {
	"SVGAlib",
        "svga",
        "Zoltan Mark Vician <se7en@sch.bme.hu>",
        ""
};

// SVGAlib definitions

GraphicsContext *screen;
GraphicsContext *virt;

static uint8_t *scalebuf = NULL, *yuvbuf = NULL, *bppbuf = NULL;

static uint32_t orig_w, orig_h, maxw, maxh; // Width, height
static float scaling = 1.0;
static uint32_t x_pos, y_pos; // Position

// Order must not change!
#define _640x480x32K 	 0   // 17
#define _640x480x64K 	 1   // 18
#define _640x480x16M 	 2   // 19
#define _640x480x16M32   3   // 34
#define _800x600x32K 	 4   // 20
#define _800x600x64K 	 5   // 21
#define _800x600x16M 	 6   // 22 
#define _800x600x16M32   7   // 35
#define _1024x768x32K 	 8   // 23
#define _1024x768x64K 	 9   // 24
#define _1024x768x16M 	 10  // 25
#define _1024x768x16M32  11  // 36
#define VID_MODE_NUM	 12

static uint8_t vid_modes[VID_MODE_NUM];
static vid_mode_nums[VID_MODE_NUM] = {17,18,19,34,20,21,22,35,23,24,25,36};
static uint8_t vid_mode;
static uint8_t bpp;

static uint32_t pformat;

static uint8_t checked = 0;
static uint8_t bpp_conv = 0;

static void checksupportedmodes() {
  int i;
  
  checked = 1;
  vga_init();
  vga_disabledriverreport();
  for (i = 0; i < VID_MODE_NUM; i++) {
    if (vga_hasmode(vid_mode_nums[i]) > 0)
      vid_modes[i] = 1;
    else vid_modes[i] = 0;
  }
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
                     uint32_t d_height, uint32_t fullscreen, char *title, 
		     uint32_t format) {
  uint32_t wid = (d_width > 0 ? d_width : width);
  
  if (!checked) {
    checksupportedmodes(); // Looking for available video modes
  }

  pformat = format;
  
  // -bpp check
  if (!vo_dbpp) {
    if (format == IMGFMT_YV12) bpp = 32;
    else bpp = format & 255;
  } else {
      bpp = vo_dbpp;
      switch (bpp) {
        case 32: if (!(vid_modes[_640x480x16M32] | vid_modes[_800x600x16M32] | vid_modes[_1024x768x16M32])) {
	           printf("vo_svga: %dbpp not supported by HW or SVGAlib",bpp);
		   return(1);
                 }
        case 24: if (!(vid_modes[_640x480x16M] | vid_modes[_800x600x16M] | vid_modes[_1024x768x16M])) {
	           printf("vo_svga: %dbpp not supported by HW or SVGAlib",bpp);
		   return(1);
                 }
        case 16: if (!(vid_modes[_640x480x64K] | vid_modes[_800x600x64K] | vid_modes[_1024x768x64K])) {
	           printf("vo_svga: %dbpp not supported by HW or SVGAlib",bpp);
		   return(1);
                 }
        case 15: if (!(vid_modes[_640x480x32K] | vid_modes[_800x600x32K] | vid_modes[_1024x768x32K])) {
	           printf("vo_svga: %dbpp not supported by HW or SVGAlib",bpp);
		   return(1);
                 }
      }
    }
  
  if (wid > 800)
    switch (bpp) {
      case 32: vid_mode = 36; break;
      case 24: vid_mode = bpp_conv ? 36 : 25; bpp = 32; break;
      case 16: vid_mode = 24; break;
      case 15: vid_mode = bpp_conv ? 24 : 23; bpp = 16; break;
    }
  else
    if (wid > 640)
      switch (bpp) {
        case 32: vid_mode = 35; break;
        case 24: vid_mode = bpp_conv ? 35 : 22; bpp = 32; break;
        case 16: vid_mode = 21; break;
        case 15: vid_mode = bpp_conv ? 21 : 20; bpp = 16; break;
      }
    else
      switch (bpp) {
        case 32: vid_mode = 34; break;
        case 24: vid_mode = bpp_conv ? 34 : 19; bpp = 32; break;
        case 16: vid_mode = 18; break;
        case 15: vid_mode = bpp_conv ? 18 : 17; bpp = 16; break;
      }
  if (bpp_conv)
    bppbuf = malloc(maxw * maxh * BYTESPERPIXEL);
  if (!bppbuf) {
    printf("vo_svga: Not enough memory for buffering!");
    uninit();
    return (1);
  }

  vga_setlinearaddressing();
  if (vga_setmode(vid_mode) == -1){
    printf("vo_svga: vga_setmode(%d) failed.\n",vid_mode);
    return(1); // error
  }
  if (gl_setcontextvga(vid_mode)){
    printf("vo_svga: gl_setcontextvga(%d) failed.\n",vid_mode);
    return(1); // error
  }
  screen = gl_allocatecontext();
  gl_getcontext(screen);
  if (gl_setcontextvgavirtual(vid_mode)){
    printf("vo_svga: gl_setcontextvgavirtual(%d) failed.\n",vid_mode);
    return(1); // error
  }
  virt = gl_allocatecontext();
  gl_getcontext(virt);
  gl_setcontext(virt);
  gl_clearscreen(0);
  
  orig_w = width;
  orig_h = height;
  if ((fullscreen & 0x04) && (WIDTH != orig_w)) {
    if (((orig_w*1.0) / orig_h) < (4.0/3)) {
      maxh = HEIGHT;
      scaling = maxh / (orig_h * 1.0);
      maxw = (uint32_t) (orig_w * scaling);
      scalebuf = malloc(maxw * maxh * BYTESPERPIXEL);
      if (!scalebuf) {
        printf("vo_svga: Not enough memory for buffering!");
	uninit();
	return (1);
      }
    } else {
        maxw = WIDTH;
        scaling = maxw / (orig_w * 1.0);
        maxh = (uint32_t) (orig_h * scaling);
        scalebuf = malloc(maxw * maxh * BYTESPERPIXEL);
        if (!scalebuf) {
          printf("vo_svga: Not enough memory for buffering!");
	  uninit();
	  return (1);
        }
      }
  } else {
      maxw = orig_w;
      maxh = orig_h;
    }
  x_pos = (WIDTH - maxw) / 2;
  y_pos = (HEIGHT - maxh) / 2;
  
  if (pformat == IMGFMT_YV12) {
    yuv2rgb_init(bpp, MODE_RGB);
    yuvbuf = malloc(maxw * maxh * BYTESPERPIXEL);
    if (!yuvbuf) {
      printf("vo_svga: Not enough memory for buffering!");
      uninit();
      return (1);
    }
  }

  printf("SVGAlib resolution: %dx%d %dbpp - ", WIDTH, HEIGHT, bpp);
  if (maxw != orig_w || maxh != orig_h) printf("Video scaled to: %dx%d\n",maxw,maxh);
  else printf("No video scaling\n");

  return (0);
}

static uint32_t query_format(uint32_t format) {
  uint8_t res = 0;

  if (!checked)
    checksupportedmodes(); // Looking for available video modes
  switch (format) {
    case IMGFMT_RGB32: 
    case IMGFMT_BGR|32: {
      return (vid_modes[_640x480x16M32] | vid_modes[_800x600x16M32] | vid_modes[_1024x768x16M32]);
    }
    case IMGFMT_RGB24: 
    case IMGFMT_BGR|24: {
      res = vid_modes[_640x480x16M] | vid_modes[_800x600x16M] | vid_modes[_1024x768x16M];
      if (!res) {
        res = vid_modes[_640x480x16M32] | vid_modes[_800x600x16M32] | vid_modes[_1024x768x16M32];
	bpp_conv = 1;
      }
      return (res);
    }
    case IMGFMT_RGB16: 
    case IMGFMT_BGR|16: {
      return (vid_modes[_640x480x64K] | vid_modes[_800x600x64K] | vid_modes[_1024x768x64K]);
    }
    case IMGFMT_RGB15: 
    case IMGFMT_BGR|15: {
      res = vid_modes[_640x480x32K] | vid_modes[_800x600x32K] | vid_modes[_1024x768x32K];
      if (!res) {
        res = vid_modes[_640x480x64K] | vid_modes[_800x600x64K] | vid_modes[_1024x768x64K];
        bpp_conv = 1;
      }
      return (res);
    }
    case IMGFMT_YV12: return (1);
  }
  return (0);
}

static const vo_info_t* get_info(void) {
  return (&vo_info);
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride) {
  switch (bpp) {
    case 32: 
      vo_draw_alpha_rgb32(w, h, src, srca, stride, virt->vbuf+4*(y0*WIDTH+x0), 4*WIDTH);
      break;
    case 24: 
      vo_draw_alpha_rgb24(w, h, src, srca, stride, virt->vbuf+3*(y0*WIDTH+x0), 3*WIDTH);
      break;
    case 16:
      vo_draw_alpha_rgb16(w, h, src, srca, stride, virt->vbuf+2*(y0*WIDTH+x0), 2*WIDTH);
      break;
    case 15:
      vo_draw_alpha_rgb15(w, h, src, srca, stride, virt->vbuf+2*(y0*WIDTH+x0), 2*WIDTH);
      break;
  }
}		

static uint32_t draw_frame(uint8_t *src[]) {
  if (pformat == IMGFMT_YV12) {
    yuv2rgb(yuvbuf, src[0], src[1], src[2], orig_w, orig_h, orig_w * BYTESPERPIXEL, orig_w, orig_w / 2);
    src[0] = yuvbuf;
  }
  if (scalebuf) {
    gl_scalebox(orig_w, orig_h, src[0], maxw, maxh, scalebuf);
    src[0] = scalebuf;
  }
  if (bpp_conv) {
    uint16_t *src = (uint16_t *) src[0];
    uint16_t *dest = (uint16_t *) bppbuf;
    uint16_t *end;
    
    switch(bpp) {
      case 32:
	end = src + (maxw * maxh * 2);
        while (src < end) {
	  *dest++ = *src++;
	  (uint8_t *)dest = (uint8_t *)src;
	  *(((uint8_t *)dest)+1) = 0;
	  dest++;
	  src++;
	}
      case 16:
#ifdef HAVE_MMX
        rgb15to16_mmx(src[0],bppbuf,maxw * maxh * 2);
#else
	register uint16_t srcdata;
	
	end = src + (maxw * maxh);
	while (src < end) {
	  srcdata = *src++;
	  *dest++ = (srcdata & 0x1f) | ((srcdata & 0x7fe0) << 1);
	}
#endif
    }
    src[0] = bppbuf;
  }
  gl_putbox(x_pos, y_pos, maxw, maxh, src[0]);
}

static uint32_t draw_slice(uint8_t *image[], int stride[], 
                           int w, int h, int x, int y) {
  uint8_t *src = yuvbuf;
  uint32_t sw, sh;
  
  emms();
  sw = (uint32_t) (w * scaling);
  sh = (uint32_t) (h * scaling);
  yuv2rgb(yuvbuf, image[0], image[1], image[2], w, h, orig_w * BYTESPERPIXEL, stride[0], stride[1]);
  if (scalebuf) {
    gl_scalebox(w, h, yuvbuf, sw, sh, scalebuf);
    src = scalebuf;
  }
  gl_putbox((int)(x * scaling) + x_pos, (int)(y * scaling) + y_pos, sw, sh, src);
}

static void flip_page(void) {
  if (y_pos) {
    gl_fillbox(0, 0, WIDTH, y_pos, 0);
    gl_fillbox(0, HEIGHT - y_pos, WIDTH, y_pos, 0);
  } else {
      gl_fillbox(0, 0, x_pos, HEIGHT, 0);
      gl_fillbox(WIDTH - x_pos, 0, x_pos, HEIGHT, 0);
    }
  vo_draw_text(WIDTH, HEIGHT, draw_alpha);
  gl_copyscreen(screen);
}

static void check_events(void) {
}

static void uninit(void) {
  gl_freecontext(screen);
  gl_freecontext(virt);
  vga_setmode(TEXT);
  if (bppbuf)
    free(bppbuf);
  if (scalebuf)
    free(scalebuf);
  if (yuvbuf)
    free(yuvbuf);
}
	