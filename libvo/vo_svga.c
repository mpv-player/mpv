/*
  Video driver for SVGAlib 
  by Zoltan Mark Vician <se7en@sch.bme.hu>
  Code started: Mon Apr  1 23:25:47 2001

  Some changes by Matan Ziv-Av <matan@svgalib.org>
*/

#include <stdio.h>
#include <stdlib.h>

#include <vga.h>

#include <limits.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "sub.h"
#include "../postproc/rgb2rgb.h"

#include "../mp_msg.h"
//#include "../mp_image.h"

extern int vo_directrendering;
extern int vo_dbpp;
extern int verbose;

static uint32_t query_format(uint32_t format);
static int checksupportedmodes();
static void putbox(int x, int y, int w, int h, uint8_t *buf,int prog);
static void fillbox(int x, int y, int w, int h, uint32_t c);
static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride);
static uint32_t get_image(mp_image_t *mpi);

static uint8_t *yuvbuf = NULL, *bppbuf = NULL;
static uint8_t *GRAPH_MEM;

static int BYTESPERPIXEL, WIDTH, HEIGHT, LINEWIDTH;
static int frame, maxframes, oldmethod=0;

static uint32_t pformat;
static uint32_t orig_w, orig_h, maxw, maxh; // Width, height
static uint8_t buf0[8192];
static uint8_t *buffer;

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
#define BPP_8 16
#define BPP_4 32
#define BPP_1 64
static uint8_t bpp_avail = 0;

static uint8_t checked = 0;

static uint32_t x_pos, y_pos;

LIBVO_EXTERN(svga)

static vo_info_t vo_info = {
	"SVGAlib",
        "svga",
        "Zoltan Mark Vician <se7en@sch.bme.hu>",
        ""
};

static uint32_t preinit(const char *arg)
{
  int i;
  
  for(i=0;i<8192;i++) buf0[i]=0;
  
  if(vo_directrendering) {
      maxframes=0;
  } else {
      maxframes=1;
  }

printf("vo_svga: preinit - maxframes=%i\n",maxframes);
  
  return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  }
  return VO_NOTIMPL;
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
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
        case 256  : bpp_avail |= BPP_8; break;
        case 16   : bpp_avail |= BPP_4; break;
        case 2    : bpp_avail |= BPP_1; break;
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
      case 8: if (!(bpp_avail & BPP_8)) {
	           printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
		   printf("vo_svga: Maybe you should try -bpp\n");
		   return(1);
	       }
               break;
      case 4: if (!(bpp_avail & BPP_4)) {
	           printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
		   printf("vo_svga: Maybe you should try -bpp\n");
		   return(1);
	       }
               break;
      case 1: if (!(bpp_avail & BPP_1)) {
	           printf("vo_svga: Haven't found video mode which fit to: %dx%d %dbpp\n",req_w,req_h,bpp);
		   printf("vo_svga: Maybe you should try -bpp\n");
		   return(1);
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
        case 8: if (!(bpp_avail & BPP_8)) {
	           printf("vo_svga: %dbpp not supported in %dx%d (or larger resoltuion) by HW or SVGAlib\n",bpp,req_w,req_h);
		   return(1);
                 }
		 break;
        case 4: if (!(bpp_avail & BPP_4)) {
	           printf("vo_svga: %dbpp not supported in %dx%d (or larger resoltuion) by HW or SVGAlib\n",bpp,req_w,req_h);
		   return(1);
                 }
		 break;
        case 1: if (!(bpp_avail & BPP_1)) {
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
          case 2    : printf("vo_svga: vid_mode: %d, %dx%d 1bpp\n",
                             list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
          case 16   : printf("vo_svga: vid_mode: %d, %dx%d 4bpp\n",
                             list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
          case 256  : printf("vo_svga: vid_mode: %d, %dx%d 8bpp\n",
                             list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
          case 32768: printf("vo_svga: vid_mode: %d, %dx%d 15bpp\n",
                             list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
          case 65536: printf("vo_svga: vid_mode: %d, %dx%d 16bpp\n",
                             list->modenum,list->modeinfo.width,list->modeinfo.height);
	              break;
        }
        switch (list->modeinfo.bytesperpixel) {
          case 3: printf("vo_svga: vid_mode: %d, %dx%d 24bpp\n",
                         list->modenum,list->modeinfo.width,list->modeinfo.height);
	          break;
          case 4: printf("vo_svga: vid_mode: %d, %dx%d 32bpp\n",
                         list->modenum,list->modeinfo.width,list->modeinfo.height);
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
        case 8: if (list->modeinfo.colors == 256)
                   if ((list->modeinfo.width < buf_w) || (list->modeinfo.height < buf_h)) {
                     vid_mode = list->modenum;
                     buf_w = list->modeinfo.width;
                     buf_h = list->modeinfo.height;
		     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
		   }
		 break;
        case 4: if (list->modeinfo.colors ==16)
                   if ((list->modeinfo.width < buf_w) || (list->modeinfo.height < buf_h)) {
                     vid_mode = list->modenum;
                     buf_w = list->modeinfo.width;
                     buf_h = list->modeinfo.height;
		     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
		   }
		 break;
        case 1: if (list->modeinfo.colors == 2)
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

  if((vo_subdevice) && (strlen(vo_subdevice)>2)) {
     if(!strncmp(vo_subdevice,"old",3)) {
        oldmethod=1;
        vo_subdevice+=3;
        if( *vo_subdevice == ',' ) vo_subdevice++;
     }
  }

  if((vo_subdevice)  && *vo_subdevice) {
      int vm;
      vm=vga_getmodenumber(vo_subdevice);
      list=modelist;
      while(list) {
          if(list->modenum == vm) {
             buf_w = list->modeinfo.width;
             buf_h = list->modeinfo.height;
	     res_widescr = (((buf_w*1.0)/buf_h) > (4.0/3)) ? 1 : 0;
             switch(list->modeinfo.colors) {
                 case 2:
                     bpp=1;
                     bpp_conv=0;
                     break;
                 case 16:
                     bpp=4;
                     bpp_conv=0;
                     break;
                 case 256:
                     bpp=8;
                     bpp_conv=0;
                     break;
                 case 32768:
                     bpp=16;
                     bpp_conv=1;
                     break;
                 case 65536:
                     bpp=16;
                     bpp_conv=0;
                     break;
                 case (1<<24):
                     if(list->modeinfo.bytesperpixel == 3) {
                         bpp=32;
                         bpp_conv=1;
                     } else {
                         bpp=32;
                         bpp_conv=0;
                     }
                     break;
             }
             vid_mode=vm;
             list=NULL;
      	  } else list=list->next;
      }
  }

  if (verbose)
    printf("vo_svga: vid_mode: %d\n",vid_mode);
  if (vga_setmode(vid_mode) == -1) {
    printf("vo_svga: vga_setmode(%d) failed.\n",vid_mode);
    uninit();
    return(1); // error
  }
  
  /* set 332 palette for 8 bpp */
  if(bpp==8){
    int i;
    for(i=0; i<256; i++)
      vga_setpalette(i, ((i>>5)&7)*9, ((i>>2)&7)*9, (i&3)*21);
  }
  /* set 121 palette for 4 bpp */
  else if(bpp==4){
    int i;
    for(i=0; i<16; i++)
      vga_setpalette(i, ((i>>3)&1)*63, ((i>>1)&3)*21, (i&1)*63);
  }

  WIDTH=vga_getxdim();
  HEIGHT=vga_getydim();
  BYTESPERPIXEL=(bpp+4)>>3;
  if(bpp==1)
    LINEWIDTH=(WIDTH+7)/8;
  else
    LINEWIDTH=WIDTH*BYTESPERPIXEL;

  vga_setlinearaddressing();
  if(oldmethod) {
     buffer=malloc(HEIGHT*LINEWIDTH);
     maxframes=0;
  }
  vga_claimvideomemory((maxframes+1)*HEIGHT*LINEWIDTH);
  GRAPH_MEM=vga_getgraphmem();
  frame=0;
  fillbox(0,0,WIDTH,HEIGHT*(maxframes+1),0);
  
  orig_w = width;
  orig_h = height;
  maxw = orig_w;
  maxh = orig_h;

  if (bpp_conv) {
    bppbuf = malloc(maxw * maxh * BYTESPERPIXEL);
    if (bppbuf == NULL) {
      printf("vo_svga: bppbuf -> Not enough memory for buffering!\n");
      uninit();
      return (1);
    }  
  }
  
  x_pos = (WIDTH - maxw) / 2;
  y_pos = (HEIGHT - maxh) / 2;
  
  if (pformat == IMGFMT_YV12) {
    yuv2rgb_init(bpp, MODE_RGB);
    if(bpp==1)
      yuvbuf = malloc((maxw+7)/8 * maxh);
    else
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

  vga_setdisplaystart(0);

  return (0);
}

static const vo_info_t* get_info(void) {
  return (&vo_info);
}

static uint32_t draw_frame(uint8_t *src[]) {
  if (pformat == IMGFMT_YV12) {
    if(bpp==1)
      yuv2rgb(yuvbuf, src[0], src[1], src[2], orig_w, orig_h, (orig_w+7)/8, orig_w, orig_w / 2);
    else
      yuv2rgb(yuvbuf, src[0], src[1], src[2], orig_w, orig_h, orig_w * BYTESPERPIXEL, orig_w, orig_w / 2);
    src[0] = yuvbuf;
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
        rgb15to16(src[0],bppbuf,maxw * maxh * 2);
      } break;
    }
    src[0] = bppbuf;
  }
  putbox(x_pos, y_pos, maxw, maxh, src[0], 1);
  
  return (0);
}

static uint32_t draw_slice(uint8_t *image[], int stride[], 
                           int w, int h, int x, int y) {
  uint8_t *src = yuvbuf;
  uint32_t sw, sh;
  if(bpp==1)
    yuv2rgb(yuvbuf, image[0], image[1], image[2], w, h, (orig_w+7)/8, stride[0], stride[1]);
  else
    yuv2rgb(yuvbuf, image[0], image[1], image[2], w, h, orig_w * BYTESPERPIXEL, stride[0], stride[1]);

  putbox(x + x_pos, y + y_pos, w, h, src, 1);

  return (0);
}

static void draw_osd(void)
{
  if(oldmethod) {
      if (y_pos) {
        fillbox(0, 0, WIDTH, y_pos, 0);
        fillbox(0, HEIGHT - y_pos, WIDTH, y_pos, 0);
        if (x_pos) {
           int hmy=HEIGHT - (y_pos<<1); 
           fillbox(0, y_pos, x_pos, hmy, 0);
           fillbox(WIDTH - x_pos, y_pos, x_pos, hmy, 0);
        }
      } else if (x_pos) {
        fillbox(0, y_pos, x_pos, HEIGHT, 0);
        fillbox(WIDTH - x_pos, y_pos, x_pos, HEIGHT, 0);
      }
      vo_draw_text(WIDTH, HEIGHT, draw_alpha);
  } else 
  vo_draw_text(maxw, maxh, draw_alpha);
}

static void flip_page(void) {
    if(oldmethod)  {
       int i;
       uint8_t *b;
       b=buffer;
       for(i=0;i<HEIGHT;i++){ 
          vga_drawscansegment(b,0,i,LINEWIDTH);
          b+=LINEWIDTH;
       }
    } else {
        if(maxframes) {
            vga_setdisplaystart(frame*HEIGHT*LINEWIDTH);
            frame++;
            if(frame>maxframes)frame=0;
        }
    }
}

static void check_events(void) {
}

static void uninit(void) {
  vga_modelist_t *list = modelist;

  vga_setmode(TEXT);

  if (bppbuf != NULL)
    free(bppbuf);
  if (yuvbuf != NULL)
    free(yuvbuf);
  while (modelist != NULL) {
       list=modelist;
       modelist=modelist->next;
       free(list);
  }
  checked = 0;
}


/* --------------------------------------------------------------------- */

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
  getch2_disable();
  vga_init();
  vga_disabledriverreport();
  max = vga_lastmodenumber();
  if (verbose >= 2)
    printf("vo_svga: Max mode : %i\n",max);
  for (i = 1; i <= max; i++)
    if (vga_hasmode(i) > 0) {
      minfo = vga_getmodeinfo(i);
      switch (minfo->colors) {
        case 2    : bpp_avail |= BPP_1 ; break;
        case 16   : bpp_avail |= BPP_4 ; break;
        case 256  : bpp_avail |= BPP_8 ; break;
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
      case 8 : if ((format == IMGFMT_RGB8 ) || (format == IMGFMT_BGR8))
                 return ((bpp_avail & BPP_8 ) ? 1 : 0);
	       break;
      case 4 : if ((format == IMGFMT_RGB4 ) || (format == IMGFMT_BGR4))
                 return ((bpp_avail & BPP_4 ) ? 1 : 0);
	       break;
      case 1 : if ((format == IMGFMT_RGB1 ) || (format == IMGFMT_BGR1))
                 return ((bpp_avail & BPP_1 ) ? 1 : 0);
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
        case IMGFMT_RGB8:
        case IMGFMT_BGR8: return ((bpp_avail & BPP_8) ? 1 : 0); break;
        case IMGFMT_RGB4:
        case IMGFMT_BGR4: return ((bpp_avail & BPP_4) ? 1 : 0); break;
        case IMGFMT_RGB1:
        case IMGFMT_BGR1: return ((bpp_avail & BPP_1) ? 1 : 0); break;
      }
    }
  return (0);
}

static void putbox(int x, int y, int w, int h, uint8_t *buf, int prog) {
    int base, add, wid;
    if(bpp==1)
      wid=(w+7)/8;
    else
      wid=w*BYTESPERPIXEL;

    if(oldmethod) {
        add=wid*prog;
        while( (h--) > 0 ) {
            if(bpp==1)
              memcpy(buffer+(x+7)/8+(y++)*LINEWIDTH, buf, wid);
            else
              memcpy(buffer+x*BYTESPERPIXEL+(y++)*LINEWIDTH, buf, wid);
            buf+=add;
        }
    } else {
        add=wid*prog;
        base=frame*HEIGHT;
        while( (h--) > 0 ) {
            vga_drawscansegment(buf, x, (y++)+base, wid);
            buf+=add;
        }
    }
}

static void fillbox(int x, int y, int w, int h, uint32_t c) {
    putbox(x,y,w,h,buf0,0);
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride) {
  int base;
  
  if(oldmethod) {
     base=buffer;
  } else 
     base=((frame*HEIGHT+y_pos)*WIDTH+x_pos)*BYTESPERPIXEL + GRAPH_MEM ;
     
  switch (bpp) {
    case 32: 
      vo_draw_alpha_rgb32(w, h, src, srca, stride, base+4*(y0*WIDTH+x0), 4*WIDTH);
      break;
    case 24: 
      vo_draw_alpha_rgb24(w, h, src, srca, stride, base+3*(y0*WIDTH+x0), 3*WIDTH);
      break;
    case 16:
      vo_draw_alpha_rgb16(w, h, src, srca, stride, base+2*(y0*WIDTH+x0), 2*WIDTH);
      break;
    case 15:
      vo_draw_alpha_rgb15(w, h, src, srca, stride, base+2*(y0*WIDTH+x0), 2*WIDTH);
      break;
  }
}

static uint32_t get_image(mp_image_t *mpi)
{
    if (/* zoomFlag || */
	!IMGFMT_IS_BGR(mpi->imgfmt) ||
	((mpi->type != MP_IMGTYPE_STATIC) && (mpi->type != MP_IMGTYPE_TEMP)) ||
	(mpi->flags & MP_IMGFLAG_PLANAR) ||
	(mpi->flags & MP_IMGFLAG_YUV) /*
	(mpi->width != image_width) ||
	(mpi->height != image_height) */
    )
	return(VO_FALSE);

/*
    if (Flip_Flag)
    {
	mpi->stride[0] = -image_width*((bpp+7)/8);
	mpi->planes[0] = ImageData - mpi->stride[0]*(image_height-1);
    }
    else
*/
    {
	mpi->stride[0] = LINEWIDTH;
        if(oldmethod) {
	       mpi->planes[0] = buffer;
        } else
	mpi->planes[0] = GRAPH_MEM;
    }
    mpi->flags |= MP_IMGFLAG_DIRECT;
    
    return(VO_TRUE);
}

