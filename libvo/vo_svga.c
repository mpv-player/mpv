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

#include <limits.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "yuv2rgb.h"
#ifdef HAVE_MMX
#include "mmx.h"
#endif

#include "sub.h"

extern void rgb15to16_mmx(char* s0,char* d0,int count);
extern int vo_dbpp;
extern int verbose;

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

// SVGAlib - list of detected modes
typedef struct vga_modelist_s {
          uint16_t modenum;
          vga_modeinfo modeinfo;
	  struct vga_modelist_s *next;
        } vga_modelist_t;

vga_modelist_t *modelist = NULL;

static uint8_t bpp;
static uint8_t bpp_conv = 0;
static uint32_t pformat;

#define BPP_15 1
#define BPP_16 2
#define BPP_24 4
#define BPP_32 8
static uint8_t bpp_avail = 0;

static uint8_t checked = 0;

static uint32_t add_mode(uint16_t mode, vga_modeinfo minfo) {
  vga_modelist_t *list;

  if (modelist == NULL) {
    modelist = (vga_modelist_t *) malloc(sizeof(vga_modelist_t));
    if (modelist == NULL) {
      printf("vo_svga: add_mode() failed. Not enough memory for modelist.");
      return(1); // error
    }
    modelist->modenum = mode;
    modelist->modeinfo = minfo;
    modelist->next = NULL;
  } else {
      list = modelist;
      while (list->next != NULL)
        list = list->next;
      list->next = (vga_modelist_t *) malloc(sizeof(vga_modelist_t));
      if (list->next == NULL) {
        printf("vo_svga: add_mode() failed. Not enough memory for modelist.");
        return(1); // error
      }
      list = list->next;
      list->modenum = mode;
      list->modeinfo = minfo;
      list->next = NULL;
    }  
  return (0);
}

static int checksupportedmodes() {
  uint16_t i, max;
  vga_modeinfo *minfo;
  
  checked = 1;
  vga_init();
  vga_disabledriverreport();
  max = vga_lastmodenumber();
  for (i = 1; i < max; i++)
    if (vga_hasmode(i) > 0) {
      minfo = vga_getmodeinfo(i);
      switch (minfo->colors) {
        case 32768: bpp_avail |= BPP_15; break;
        case 65536: bpp_avail |= BPP_16; break;
      }
      switch (minfo->bytesperpixel) {
        case 3: bpp_avail |= BPP_24; break;
        case 4: bpp_avail |= BPP_32; break;
      }
      if (verbose >= 2)
        printf("vo_svga: Mode found: %s\n",vga_getmodename(i));
      if (add_mode(i, *minfo))
        return(1);
    }
  return(0);
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
                     uint32_t d_height, uint32_t fullscreen, char *title, 
		     uint32_t format) {
  uint32_t req_w = (d_width > 0 ? d_width : width);
  uint32_t req_h = (d_height > 0 ? d_height : height);
  uint16_t vid_mode = 0;
  uint8_t res_widescr, vid_widescr = (((req_w*1.0)/req_h) > (4.0/3)) ? 1 : 0;
  uint16_t buf_w = USHRT_MAX, buf_h = USHRT_MAX;
  vga_modelist_t *list = modelist;
  
  if (!checked) {
    if (checksupportedmodes()) // Looking for available video modes 
      return(1);
  }

  bpp_avail = 0;
  while (list != NULL) {
    if ((list->modeinfo.width >= req_w) && (list->modeinfo.height >= req_h)) {
      switch (list->modeinfo.colors) {
        case 32768: bpp_avail |= BPP_15; break;
        case 65536: bpp_avail |= BPP_16; break;
      }
      switch (list->modeinfo.bytesperpixel) {
        case 3: bpp_avail |= BPP_24; break;
        case 4: bpp_avail |= BPP_32; break;
      }
    }
    list = list->next;
  }
  
  pformat = format;
  
  // bpp check
  bpp_conv = 0;
  if (!vo_dbpp) {
    if (format == IMGFMT_YV12) bpp = 32;
    else bpp = format & 255;
    if (verbose)
      printf("vo_svga: vo_dbpp == 0, bpp: %d\n",bpp);
    switch (bpp) {
      case 32: if (!(bpp_avail & BPP_32)) {
	         printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
                 printf("vo_svga: Maybe you should try -bpp\n");
		 return(1);
	       } 
               break;
      case 24: if (!(bpp_avail & BPP_24)) {
                 if (!(bpp_avail & BPP_32)) {
	           printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
                   printf("vo_svga: Maybe you should try -bpp\n");
		   return(1);
		 } else {
		     bpp = 32;
		     bpp_conv = 1;
                     printf("vo_svga: BPP conversion 24->32\n");
		   }
	       }
               break;
      case 16: if (!(bpp_avail & BPP_16)) {
	         printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
		 printf("vo_svga: Maybe you should try -bpp\n");
		 return(1);
	       } 
               break;
      case 15: if (!(bpp_avail & BPP_15)) {
                 if (!(bpp_avail & BPP_16)) {
	           printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
		   printf("vo_svga: Maybe you should try -bpp\n");
		   return(1);
		 } else {
		     bpp = 16;
		     bpp_conv = 1;
                     printf("vo_svga: BPP conversion 15->16\n");
		   }
	       }
               break;
    }
  } else {
      bpp = vo_dbpp;
      if (verbose)
        printf("vo_svga: vo_dbpp == %d\n",bpp);
      switch (bpp) {
        case 32: if (!(bpp_avail & BPP_32)) {
	           printf("vo_svga: %dbpp not supported in %dx%d (or larger resoltuion) by HW or SVGAlib\n",bpp,req_w,req_h);
		   return(1);
                 }
		 break;
        case 24: if (!(bpp_avail & BPP_24)) {
	           printf("vo_svga: %dbpp not supported in %dx%d (or larger resoltuion) by HW or SVGAlib\n",bpp,req_w,req_h);
		   return(1);
                 }
		 break;
        case 16: if (!(bpp_avail & BPP_16)) {
	           printf("vo_svga: %dbpp not supported in %dx%d (or larger resoltuion) by HW or SVGAlib\n",bpp,req_w,req_h);
		   return(1);
                 }
		 break;
        case 15: if (!(bpp_avail & BPP_15)) {
	           printf("vo_svga: %dbpp not supported in %dx%d (or larger resoltuion) by HW or SVGAlib\n",bpp,req_w,req_h);
		   return(1);
                 }
		 break;
      }
    }

  list = modelist;
  if (verbose) {
    printf("vo_svga: Looking for the best resolution...\n");
    printf("vo_svga: req_w: %d, req_h: %d, bpp: %d\n",req_w,req_h,bpp);
  }
  while (list != NULL) {
    if ((list->modeinfo.width >= req_w) && (list->modeinfo.height >= req_h)) {
      if (verbose) {
        switch (list->modeinfo.colors) {
          case 32768: printf("vo_svga: vid_mode: %d, %dx%d 15bpp\n",list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
          case 65536: printf("vo_svga: vid_mode: %d, %dx%d 16bpp\n",list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
        }
        switch (list->modeinfo.bytesperpixel) {
          case 3: printf("vo_svga: vid_mode: %d, %dx%d 24bpp\n",list->modenum,list->modeinfo.width,list->modeinfo.height);
	          break;
          case 4: printf("vo_svga: vid_mode: %d, %dx%d 32bpp\n",list->modenum,list->modeinfo.width,list->modeinfo.height);
	          break;
        }
      }
      switch (bpp) {
        case 32: if (list->modeinfo.bytesperpixel == 4)
                   if ((list->modeinfo.width < buf_w) || (list->modeinfo.height < buf_h)) {
                     vid_mode = list->modenum;
                     buf_w = list->modeinfo.width;
                     buf_h = list->modeinfo.height;
		     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
		   }
		 break;
        case 24: if (list->modeinfo.bytesperpixel == 3)
                   if ((list->modeinfo.width < buf_w) || (list->modeinfo.height < buf_h)) {
                     vid_mode = list->modenum;
                     buf_w = list->modeinfo.width;
                     buf_h = list->modeinfo.height;
		     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
		   }
		 break;
        case 16: if (list->modeinfo.colors == 65536)
                   if ((list->modeinfo.width < buf_w) || (list->modeinfo.height < buf_h)) {
                     vid_mode = list->modenum;
                     buf_w = list->modeinfo.width;
                     buf_h = list->modeinfo.height;
		     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
		   }
		 break;
        case 15: if (list->modeinfo.colors == 32768)
                   if ((list->modeinfo.width < buf_w) || (list->modeinfo.height < buf_h)) {
                     vid_mode = list->modenum;
                     buf_w = list->modeinfo.width;
                     buf_h = list->modeinfo.height;
		     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
		   }
		 break;
      }
    }
    list = list->next;
  }

  if (verbose)
    printf("vo_svga: vid_mode: %d\n",vid_mode);
  vga_setlinearaddressing();
  if (vga_setmode(vid_mode) == -1) {
    printf("vo_svga: vga_setmode(%d) failed.\n",vid_mode);
    uninit();
    return(1); // error
  } 
  if (gl_setcontextvga(vid_mode)) {
    printf("vo_svga: gl_setcontextvga(%d) failed.\n",vid_mode);
    uninit();
    return(1); // error
  }
  screen = gl_allocatecontext();
  gl_getcontext(screen);
  if (gl_setcontextvgavirtual(vid_mode)) {
    printf("vo_svga: gl_setcontextvgavirtual(%d) failed.\n",vid_mode);
    uninit();
    return(1); // error
  }
  virt = gl_allocatecontext();
  gl_getcontext(virt);
  gl_setcontext(virt);
  gl_clearscreen(0);
  
  if (bpp_conv) {
    bppbuf = malloc(maxw * maxh * BYTESPERPIXEL);
    if (bppbuf == NULL) {
      printf("vo_svga: bppbuf -> Not enough memory for buffering!\n");
      uninit();
      return (1);
    }  
  }

  orig_w = width;
  orig_h = height;
  if ((fullscreen & 0x04) && (WIDTH != orig_w) && (HEIGHT != orig_h)) {
    if (!vid_widescr || !res_widescr) {
      maxh = HEIGHT;
      scaling = maxh / (orig_h * 1.0);
      maxw = (uint32_t) (orig_w * scaling);
      scalebuf = malloc(maxw * maxh * BYTESPERPIXEL);
      if (scalebuf == NULL) {
        printf("vo_svga: scalebuf -> Not enough memory for buffering!\n");
	uninit();
	return (1);
      }
    } else {
        maxw = WIDTH;
        scaling = maxw / (orig_w * 1.0);
        maxh = (uint32_t) (orig_h * scaling);
        scalebuf = malloc(maxw * maxh * BYTESPERPIXEL);
        if (scalebuf == NULL) {
          printf("vo_svga: scalebuf -> Not enough memory for buffering!\n");
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
    if (yuvbuf == NULL) {
      printf("vo_svga: yuvbuf -> Not enough memory for buffering!\n");
      uninit();
      return (1);
    }
  }

  printf("vo_svga: SVGAlib resolution: %dx%d %dbpp - ", WIDTH, HEIGHT, bpp);
  if (maxw != orig_w || maxh != orig_h) printf("Video scaled to: %dx%d\n",maxw,maxh);
  else printf("No video scaling\n");

  return (0);
}

static uint32_t query_format(uint32_t format) {
  uint32_t res = 0;

  if (!checked) {
    if (checksupportedmodes()) // Looking for available video modes
      return(0);
  }

  // if (vo_dbpp) => There is NO conversion!!!
  if (vo_dbpp) {
    if (format == IMGFMT_YV12) return (1);
    switch (vo_dbpp) {
      case 32: if ((format == IMGFMT_RGB32) || (format == IMGFMT_BGR32))
                 return ((bpp_avail & BPP_32) ? 1 : 0);
	       break;
      case 24: if ((format == IMGFMT_RGB24) || (format == IMGFMT_BGR24))
                 return ((bpp_avail & BPP_24) ? 1 : 0);
	       break;
      case 16: if ((format == IMGFMT_RGB16) || (format == IMGFMT_BGR16))
                 return ((bpp_avail & BPP_16) ? 1 : 0);
	       break;
      case 15: if ((format == IMGFMT_RGB15) || (format == IMGFMT_BGR15))
                 return ((bpp_avail & BPP_15) ? 1 : 0);
	       break;
    }
  } else {
      switch (format) {
        case IMGFMT_RGB32: 
        case IMGFMT_BGR32: return ((bpp_avail & BPP_32) ? 1 : 0); break;
        case IMGFMT_RGB24: 
        case IMGFMT_BGR24: {
          res = (bpp_avail & BPP_24) ? 1 : 0;
          if (!res)
            res = (bpp_avail & BPP_32) ? 1 : 0;
          return (res);
        } break;
        case IMGFMT_RGB16: 
        case IMGFMT_BGR16: return ((bpp_avail & BPP_16) ? 1 : 0); break;
        case IMGFMT_RGB15: 
        case IMGFMT_BGR15: {
          res = (bpp_avail & BPP_15) ? 1 : 0;
          if (!res)
            res = (bpp_avail & BPP_16) ? 1 : 0;
          return (res);
        } break;
        case IMGFMT_YV12: return (1); break;
      }
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
  if (scalebuf != NULL) {
    gl_scalebox(orig_w, orig_h, src[0], maxw, maxh, scalebuf);
    src[0] = scalebuf;
  }
  if (bpp_conv) {
    switch(bpp) {
      case 32: {
        uint8_t *source = src[0];
        uint8_t *dest = bppbuf;
	register uint32_t i = 0;
    
	while (i < (maxw * maxh * 4)) {
	  dest[i] = source[i];
	  dest[i+1] = source[i+1];
	  dest[i+2] = source[i+2];
	  dest[i+3] = 0;
	  i += 4;
	}
      } break;
      case 16: {
#ifdef HAVE_MMX
        rgb15to16_mmx(src[0],bppbuf,maxw * maxh * 2);
#else
        uint16_t *source = (uint16_t *) src[0];
        uint16_t *dest = (uint16_t *) bppbuf;
	register uint32_t i = 0;
	register uint16_t srcdata;
	
	while (i < (maxw * maxh)) {
	  srcdata = source[i];
	  dest[i++] = (srcdata & 0x1f) | ((srcdata & 0x7fe0) << 1);
	}
#endif
      } break;
    }
    src[0] = bppbuf;
  }
  gl_putbox(x_pos, y_pos, maxw, maxh, src[0]);
  
  return (0);
}

static uint32_t draw_slice(uint8_t *image[], int stride[], 
                           int w, int h, int x, int y) {
  uint8_t *src = yuvbuf;
  uint32_t sw, sh;
  
  yuv2rgb(yuvbuf, image[0], image[1], image[2], w, h, orig_w * BYTESPERPIXEL, stride[0], stride[1]);
  sw = (uint32_t) (w * scaling);
  sh = (uint32_t) (h * scaling);
  if (scalebuf != NULL) {
    gl_scalebox(w, h, yuvbuf, sw, sh, scalebuf);
    src = scalebuf;
  }
  gl_putbox((int)(x * scaling) + x_pos, (int)(y * scaling) + y_pos, sw, sh, src);

  return (0);
}

static void draw_osd(void)
{
  if (y_pos) {
    gl_fillbox(0, 0, WIDTH, y_pos, 0);
    gl_fillbox(0, HEIGHT - y_pos, WIDTH, y_pos, 0);
  }
  if (x_pos) {
    gl_fillbox(0, 0, x_pos, HEIGHT, 0);
    gl_fillbox(WIDTH - x_pos, 0, x_pos, HEIGHT, 0);
  }

  vo_draw_text(WIDTH, HEIGHT, draw_alpha);
}

static void flip_page(void) {
  gl_copyscreen(screen);
}

static void check_events(void) {
}

static void uninit(void) {
  vga_modelist_t *list = modelist;

  gl_freecontext(screen);
  gl_freecontext(virt);
  vga_setmode(TEXT);
  if (bppbuf != NULL)
    free(bppbuf);
  if (scalebuf != NULL)
    free(scalebuf);
  if (yuvbuf != NULL)
    free(yuvbuf);
  while (modelist != NULL) {
       list=modelist;
       modelist=modelist->next;
       free(list);
  }
}
