/*
  Video driver for SVGAlib - alpha version
  by Zoltan Mark Vician <se7en@sch.bme.hu>
  Code started: Mon Apr  1 23:25:47 2000
*/

#include <stdio.h>
#include <stdlib.h>

#include <vga.h>
#include <vgagl.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "yuv2rgb.h"

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

static uint8_t *scalebuf = NULL, *yuvbuf = NULL;

static uint32_t orig_w, orig_h, maxw, maxh; // Width, height
static float scaling = 0;
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
static uint8_t vid_mode;

static uint32_t pformat;

static uint8_t checked = 0;

static void checksupportedmodes() {
  int i;
  
  checked = 1;
  vga_init();
  vga_disabledriverreport();
  for (i = 0; i < VID_MODE_NUM; i++) {
    if (vga_hasmode(i) > 0)
      vid_modes[i] = 1;
    else vid_modes[i] = 0;
  }
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
                     uint32_t d_height, uint32_t fullscreen, char *title, 
		     uint32_t format) {
static uint8_t bpp;
  if (!checked) {
    checksupportedmodes(); // Looking for available video modes
  }
  pformat = format;
  if(format==IMGFMT_YV12) bpp=32; else bpp=format&255;
  if (d_width > 800)
    switch (bpp) {
      case 32: vid_mode = 36; break;
      case 24: vid_mode = 25; break;
      case 16: vid_mode = 24; break;
      case 15: vid_mode = 23; break;
    }
  else
    if (d_width > 640)
      switch (bpp) {
        case 32: vid_mode = 35; break;
        case 24: vid_mode = 22; break;
        case 16: vid_mode = 21; break;
        case 15: vid_mode = 20; break;
      }
    else
      switch (bpp) {
        case 32: vid_mode = 34; break;
        case 24: vid_mode = 19; break;
        case 16: vid_mode = 18; break;
        case 15: vid_mode = 17; break;
      }
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
  if (fullscreen && (WIDTH != orig_w)) {
    maxw = WIDTH;
    scaling = maxw / (orig_w*1.0);
    maxh = (uint32_t) (orig_h * scaling);
    scalebuf = malloc(maxw * maxh * BYTESPERPIXEL);
  } else {
      maxw = orig_w;
      maxh = orig_h;
    }
  
  x_pos = (WIDTH - maxw) / 2;
  y_pos = (HEIGHT - maxh) / 2;
  
  if (pformat == IMGFMT_YV12) {
    yuv2rgb_init(bpp, MODE_RGB);
    yuvbuf = malloc(maxw * maxh * BYTESPERPIXEL);
  }
  
  printf("SVGAlib resolution: %dx%d %dbpp - ",WIDTH,HEIGHT,bpp);
  if (maxw != orig_w || maxh != orig_h) printf("Video scaled to: %dx%d\n",maxw,maxh);
  else printf("No video scaling\n");

  return (0);
}

static uint32_t query_format(uint32_t format) {
    if (!checked)
      checksupportedmodes(); // Looking for available video modes
    switch (format) {
      case IMGFMT_RGB32: 
      case IMGFMT_BGR|32: {
	return (vid_modes[_640x480x16M32] | vid_modes[_800x600x16M32] | vid_modes[_1024x768x16M32]);
      }
      case IMGFMT_RGB24: 
      case IMGFMT_BGR|24: {
	return (vid_modes[_640x480x16M] | vid_modes[_800x600x16M] | vid_modes[_1024x768x16M]);
      }
      case IMGFMT_RGB16: 
      case IMGFMT_BGR|16: {
	return (vid_modes[_640x480x64K] | vid_modes[_800x600x64K] | vid_modes[_1024x768x64K]);
      }
      case IMGFMT_RGB15: 
      case IMGFMT_BGR|15: {
	return (vid_modes[_640x480x32K] | vid_modes[_800x600x32K] | vid_modes[_1024x768x32K]);
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
  int x, y, i;
  uint8_t *dest, buf;
  
//  if (pformat == IMGFMT_YV12) {
    for (y = 0; y < h; y++) {
      dest = virt->vbuf + ((WIDTH * (y0 + y) + x0) * BYTESPERPIXEL);
      for (x = 0; x < w; x++) {
        if (srca[x]) {
	  for (i = 0; i < BYTESPERPIXEL; i++)
            dest[i] = /*((dest[i] * srca[x]) >> 8) +*/ src[x] >> 6;
        }
        dest += BYTESPERPIXEL;
      }
      src += stride;
      srca += stride;
    }
//  }
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
  gl_putbox(x_pos, y_pos, maxw, maxh, src[0]);
}

static uint32_t draw_slice(uint8_t *image[], int stride[], 
                           int w, int h, int x, int y) {
  uint8_t *src = yuvbuf;

  yuv2rgb(yuvbuf, image[0], image[1], image[2], w, h, orig_w * BYTESPERPIXEL, stride[0], stride[1]);
  if (scalebuf) {
    gl_scalebox(w, h, yuvbuf,(int) (w * scaling), (int) (h * scaling), scalebuf);
    src = scalebuf;
  }
  gl_putbox(x + x_pos, y + y_pos, (int) (w * scaling), (int) (h * scaling), src);
}


static void flip_page(void) {
  gl_fillbox(0, 0, WIDTH, y_pos, 0);
  gl_fillbox(0, HEIGHT - y_pos, WIDTH, y_pos, 0);
  vo_draw_text(WIDTH, HEIGHT, draw_alpha);
  gl_copyscreen(screen);
}

static void check_events(void) {
}

static void uninit(void) {
  gl_freecontext(screen);
  gl_freecontext(virt);
  vga_setmode(TEXT);
  if (scalebuf)
    free(scalebuf);
  if (yuvbuf)
    free(yuvbuf);
}
	