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

#define WIDTH_ALIGN 32 /* should be 16 for radeons */
#define NUM_FRAMES 2
static uint8_t *frames[NUM_FRAMES];

static int lvo_handler = -1;
static uint8_t *lvo_mem = NULL;
static uint8_t next_frame;
static mga_vid_config_t mga_vid_config;
static unsigned image_bpp,image_height,image_width;
extern int verbose;

#define PIXEL_SIZE() ((video_mode_info.BitsPerPixel+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (video_mode_info.XResolution*(pixel_size) )
#define IMAGE_LINE_SIZE(pixel_size) (image_width*(pixel_size))

int      vlvo_init(const char *drvname,unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp)
{
  size_t i,awidth;
	lvo_handler = open(drvname,O_RDWR);
	if(lvo_handler == -1)
	{
		printf("Couldn't open %s\n",drvname);
		return -1;
	}
	image_width = src_width;
	image_height = src_height;
	mga_vid_config.version=MGA_VID_VERSION;
        mga_vid_config.format=format;
        awidth = (src_width + (WIDTH_ALIGN-1)) & ~(WIDTH_ALIGN-1);
        switch(format){
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    image_bpp=16;
	    mga_vid_config.frame_size = awidth*src_height+(awidth*src_height)/2;
	    break;
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
		perror("Error in mga_vid_config ioctl()");
                printf("Your mga_vid driver version is incompatible with this MPlayer version!\n");
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
	ioctl( lvo_handler,MGA_VID_OFF,0 );
	munmap(frames[0],mga_vid_config.frame_size*mga_vid_config.num_frames);
	if(lvo_handler != -1) close(lvo_handler);
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
 uint8_t *src;
 uint8_t *dst;
    dst = lvo_mem + image_width * y + x;
    src = image[0];
    w *= (image_bpp+7)/8;
    memcpy(dst,src,w*h);
#endif
 return 0;
}

uint32_t vlvo_draw_frame(uint8_t *src[])
{
	size_t i, ssize;
	uint8_t *dest;
	const uint8_t *sptr;
	ssize = IMAGE_LINE_SIZE((image_bpp+7)/8);
	dest = lvo_mem;
	sptr = src[0];
	for(i=0;i<image_height;i++)
	{
	   memcpy(dest,sptr,ssize);
	   sptr += ssize;
	   dest += ssize;
	}
}

void     vlvo_flip_page(void)
{
	ioctl(lvo_handler,MGA_VID_FSEL,&next_frame);
	next_frame=(next_frame+1)%mga_vid_config.num_frames;
	lvo_mem=frames[next_frame];
}

void     vlvo_draw_osd(void)
{
  /* TODO: hw support */
}

uint32_t vlvo_query_info(uint32_t format)
{
  if(verbose) printf("vesa_lvo: query_format was called: %x (%s)\n",format,vo_format_name(format));
  return 1;
}
