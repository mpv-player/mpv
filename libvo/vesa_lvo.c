/*
 *  vesa_lvo.c
 *
 *	Copyright (C) Nick Kurshev <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *
 * This file contains vo_vesa interface to Linux Video Overlay.
 * (Partly based on vo_mga.c from mplayer's package)
 */

#include <inttypes.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vesa_lvo.h"
#include "img_format.h"
#include "../drivers/mga_vid.h" /* <- should be changed to "linux/'something'.h" */
#include "fastmemcpy.h"
#include "../mmx_defs.h"
#include "../postproc/rgb2rgb.h"

#define WIDTH_ALIGN 32 /* should be 16 for radeons */
#define NUM_FRAMES 2
static uint8_t *frames[NUM_FRAMES];

static int lvo_handler = -1;
static uint8_t *lvo_mem = NULL;
static uint8_t next_frame;
static mga_vid_config_t mga_vid_config;
static unsigned image_bpp,image_height,image_width,src_format;
extern int verbose;

#define HAVE_RADEON 1

#define PIXEL_SIZE() ((video_mode_info.BitsPerPixel+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (video_mode_info.XResolution*(pixel_size) )
#define IMAGE_LINE_SIZE(pixel_size) (image_width*(pixel_size))

int vlvo_preinit(const char *drvname)
{
  if(verbose > 1) printf("vesa_lvo: vlvo_preinit(%s) was called\n",drvname);
	lvo_handler = open(drvname,O_RDWR);
	if(lvo_handler == -1)
	{
		printf("vesa_lvo: Couldn't open '%s'\n",drvname);
		return -1;
	}
	return 0;
}

int      vlvo_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp)
{
  size_t i,awidth;
  if(verbose > 1) printf("vesa_lvo: vlvo_init() was called\n");
	image_width = src_width;
	image_height = src_height;
	mga_vid_config.version=MGA_VID_VERSION;
        src_format = mga_vid_config.format=format;
        awidth = (src_width + (WIDTH_ALIGN-1)) & ~(WIDTH_ALIGN-1);
        switch(format){
#ifdef HAVE_RADEON
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    image_bpp=16;
	    mga_vid_config.format = IMGFMT_YUY2;
	    mga_vid_config.frame_size = awidth*src_height*2;
	    break;
#else
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    image_bpp=16;
	    mga_vid_config.frame_size = awidth*src_height+(awidth*src_height)/2;
	    break;
#endif	    
        case IMGFMT_YUY2:
        case IMGFMT_UYVY:
	    image_bpp=16;
	    mga_vid_config.frame_size = awidth*src_height*2;
	    break;
        case IMGFMT_RGB15:
        case IMGFMT_BGR15:
        case IMGFMT_RGB16:
        case IMGFMT_BGR16:
	    image_bpp=16;
	    mga_vid_config.frame_size = awidth*src_height*2;
	    break;
        case IMGFMT_RGB24:
        case IMGFMT_BGR24:
	    image_bpp=24;
	    mga_vid_config.frame_size = awidth*src_height*3;
	    break;
        case IMGFMT_RGB32:
        case IMGFMT_BGR32:
	    image_bpp=32;
	    mga_vid_config.frame_size = awidth*src_height*4;
	    break;
        default:
            printf("vesa_lvo: invalid output format %s(%0X)\n",vo_format_name(format),format);
            return -1;
        }
        mga_vid_config.colkey_on=0;
	mga_vid_config.src_width = src_width;
	mga_vid_config.src_height= src_height;
	mga_vid_config.dest_width = dst_width;
	mga_vid_config.dest_height= dst_height;
	mga_vid_config.x_org=x_org;
	mga_vid_config.y_org=y_org;
	mga_vid_config.num_frames=NUM_FRAMES;
	if (ioctl(lvo_handler,MGA_VID_CONFIG,&mga_vid_config))
	{
		perror("vesa_lvo: Error in mga_vid_config ioctl()");
                printf("vesa_lvo: Your fb_vid driver version is incompatible with this MPlayer version!\n");
		return -1;
	}
	ioctl(lvo_handler,MGA_VID_ON,0);

	frames[0] = (char*)mmap(0,mga_vid_config.frame_size*mga_vid_config.num_frames,PROT_WRITE,MAP_SHARED,lvo_handler,0);
	for(i=1;i<NUM_FRAMES;i++)
		frames[i] = frames[i-1] + mga_vid_config.frame_size;
	next_frame = 0;
	lvo_mem = frames[next_frame];

	/*clear the buffer*/
	memset(frames[0],0x80,mga_vid_config.frame_size*mga_vid_config.num_frames);
	return 0;  
}

void vlvo_term( void )
{
  if(verbose > 1) printf("vesa_lvo: vlvo_term() was called\n");
	ioctl( lvo_handler,MGA_VID_OFF,0 );
	munmap(frames[0],mga_vid_config.frame_size*mga_vid_config.num_frames);
	if(lvo_handler != -1) close(lvo_handler);
}

static void
CopyData420(
   unsigned char *src1,
   unsigned char *src2,
   unsigned char *src3,
   unsigned char *dst1,
   unsigned char *dst2,
   unsigned char *dst3,
   int srcPitch,
   int srcPitch2,
   int dstPitch,
   int h,
   int w
){
   int count;

       count = h;
       while(count--) {
	   memcpy(dst1, src1, w);
	   src1 += srcPitch;
	   dst1 += dstPitch;
       }

   w >>= 1;
   h >>= 1;
   dstPitch >>= 1;

       count = h;
       while(count--) {
	   memcpy(dst2, src2, w);
	   src2 += srcPitch2;
	   dst2 += dstPitch;
       }

       count = h;
       while(count--) {
	   memcpy(dst3, src3, w);
	   src3 += srcPitch2;
	   dst3 += dstPitch;
       }
}


uint32_t vlvo_draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
#if 0
/* original vo_mga stuff */
    uint8_t *src;
    uint8_t *dest;
    uint32_t bespitch,bespitch2,srcpitch;
    int i;

    bespitch = (mga_vid_config.src_width + (WIDTH_ALIGN-1)) & ~(WIDTH_ALIGN-1);
    bespitch2 = bespitch/2;

    dest = lvo_mem + bespitch * y + x;
    src = image[0];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[0];
        dest += bespitch;
    }

    w/=2;h/=2;x/=2;y/=2;

    dest = lvo_mem + bespitch*mga_vid_config.src_height + bespitch2 * y + x;
    src = image[1];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[1];
        dest += bespitch2;
    }

    dest = lvo_mem + bespitch*mga_vid_config.src_height
                   + bespitch*mga_vid_config.src_height / 4
                   + bespitch2 * y + x;
    src = image[2];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[2];
        dest += bespitch2;
    }
#else
 uint8_t *dst;
 uint8_t bytpp;
 bytpp = (image_bpp+7)/8;
 dst = lvo_mem + (image_width * y + x)*bytpp;
#ifdef HAVE_RADEON
    if(src_format == IMGFMT_YV12)
    {
      yv12toyuy2(image[0],image[1],image[2],dst
                 ,w,h,stride[0],stride[1],w*2);
    }
    else
#else
    if(src_format == IMGFMT_YV12)
    {
        uint32_t dstPitch,d1line,d2line,d3line,d1offset,d2offset,d3offset;
	dstPitch = (mga_vid_config.src_width + 15) & ~15;  /* of luma */
	d1line = y * dstPitch;
	d2line = (mga_vid_config.src_height * dstPitch) + ((y >> 1) * (dstPitch >> 1));
	d3line = d2line + ((mga_vid_config.src_height >> 1) * (dstPitch >> 1));

	y &= ~1;

	d1offset = (y * dstPitch) + x;
	d2offset = d2line + (x >> 1);
	d3offset = d3line + (x >> 1);
      CopyData420(image[0],image[1],image[2],
    		  dst+d1offset,dst+d2offset,dst+d3offset,
		  stride[0],stride[1],dstPitch,h,w);
    }
    else
#endif
      memcpy(dst,image[0],mga_vid_config.frame_size);
#endif
 if(verbose > 1) printf("vesa_lvo: vlvo_draw_slice() was called\n");
 return 0;
}

uint32_t vlvo_draw_frame(uint8_t *image[])
{
/* Note it's very strange but sometime for YUY2 draw_frame is called */
  memcpy(lvo_mem,image[0],mga_vid_config.frame_size);
  if(verbose > 1) printf("vesa_lvo: vlvo_draw_frame() was called\n");
  return 0;
}

void     vlvo_flip_page(void)
{
  if(verbose > 1) printf("vesa_lvo: vlvo_flip_page() was called\n");
	ioctl(lvo_handler,MGA_VID_FSEL,&next_frame);
	next_frame=(next_frame+1)%mga_vid_config.num_frames;
	lvo_mem=frames[next_frame];
}

void     vlvo_draw_osd(void)
{
  if(verbose > 1) printf("vesa_lvo: vlvo_draw_osd() was called\n");
  /* TODO: hw support */
}

uint32_t vlvo_query_info(uint32_t format)
{
  if(verbose > 1) printf("vesa_lvo: query_format was called: %x (%s)\n",format,vo_format_name(format));
  return 1;
}
