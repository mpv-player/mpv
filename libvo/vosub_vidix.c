
/*
 *  vosub_vidix.c
 *
 *	Copyright (C) Nick Kurshev <nickols_k@mail.ru> - 2002
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *
 * This file contains vidix interface to any mplayer's VO plugin.
 * (Partly based on vesa_lvo.c from mplayer's package)
 */

#include <inttypes.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "vosub_vidix.h"
#include "../vidix/vidixlib.h"
#include "fastmemcpy.h"
#include "osd.h"
#include "video_out.h"

#define NUM_FRAMES 10 /* Temporary: driver will overwrite it */
#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

static uint32_t frames[NUM_FRAMES];

static VDL_HANDLE vidix_handler = NULL;
static uint8_t *vidix_mem = NULL;
static uint8_t next_frame;
static unsigned image_bpp,image_height,image_width,src_format;
extern int verbose;

static vidix_capability_t vidix_cap;
static vidix_playback_t   vidix_play;
static vidix_fourcc_t	  vidix_fourcc;

#define PIXEL_SIZE() ((video_mode_info.BitsPerPixel+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (video_mode_info.XResolution*(pixel_size) )
#define IMAGE_LINE_SIZE(pixel_size) (image_width*(pixel_size))

extern vo_functions_t video_out_vesa;

int vidix_preinit(const char *drvname)
{
  int err;
  if(verbose > 1) printf("vosub_vidix: vidix_preinit(%s) was called\n",drvname);
	if(vdlGetVersion() != VIDIX_VERSION)
	{
	  printf("vosub_vidix: You have wrong version of VIDIX library\n");
	  return -1;
	}
	vidix_handler = vdlOpen("/usr/lib/mplayer/vidix/",
				drvname ? drvname[0] == ':' ? &drvname[1] : NULL : NULL,
				TYPE_OUTPUT,
				verbose);
	if(vidix_handler == NULL)
	{
		printf("vosub_vidix: Couldn't find working VIDIX driver\n");
		return -1;
	}
	if((err=vdlGetCapability(vidix_handler,&vidix_cap)) != 0)
	{
		printf("vosub_vidix: Couldn't get capability: %s\n",strerror(err));
		return -1;
	}
	printf("vosub_vidix: Using: %s\n",vidix_cap.name);
	/* we are able to tune up this stuff depend on fourcc format */
	video_out_vesa.draw_slice=vidix_draw_slice;
	video_out_vesa.draw_frame=vidix_draw_frame;
	video_out_vesa.flip_page=vidix_flip_page;
	video_out_vesa.draw_osd=vidix_draw_osd;
	return 0;
}

int      vidix_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp,
		   unsigned vid_w,unsigned vid_h)
{
  size_t i,awidth;
  int err;
  if(verbose > 1) printf("vosub_vidix: vidix_init() was called\n");
	if(vid_w > vidix_cap.maxwidth || vid_w < vidix_cap.minwidth ||
	   vid_h > vidix_cap.maxheight || vid_h < vidix_cap.minheight)
	{
	  printf("vosub_vidix: video server has unsupported resolution by vidix\n");
	  return -1;
	}
	err = 0;
	switch(dest_bpp)
	{
	  case 1: err = ((vidix_fourcc.depth & VID_DEPTH_1BPP) != VID_DEPTH_1BPP); break;
	  case 2: err = ((vidix_fourcc.depth & VID_DEPTH_2BPP) != VID_DEPTH_2BPP); break;
	  case 4: err = ((vidix_fourcc.depth & VID_DEPTH_4BPP) != VID_DEPTH_4BPP); break;
	  case 8: err = ((vidix_fourcc.depth & VID_DEPTH_8BPP) != VID_DEPTH_8BPP); break;
	  case 12:err = ((vidix_fourcc.depth & VID_DEPTH_12BPP) != VID_DEPTH_12BPP); break;
	  case 16:err = ((vidix_fourcc.depth & VID_DEPTH_16BPP) != VID_DEPTH_16BPP); break;
	  case 24:err = ((vidix_fourcc.depth & VID_DEPTH_24BPP) != VID_DEPTH_24BPP); break;
	  case 32:err = ((vidix_fourcc.depth & VID_DEPTH_32BPP) != VID_DEPTH_32BPP); break;
	}
	if(err)
	{
	  printf("vosub_vidix: video server has unsupported color depth by vidix\n");
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER)
	{
	  printf("vosub_vidix: vidix driver can't upscale image\n");
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
	{
	  printf("vosub_vidix: vidix driver can't downscale image\n");
	  return -1;
	}
	image_width = src_width;
	image_height = src_height;
	src_format = format;
	memset(&vidix_play,0,sizeof(vidix_playback_t));
	vidix_play.fourcc = format;
	vidix_play.capability = vidix_cap.flags; /* every ;) */
	vidix_play.blend_factor = 0; /* for now */
	vidix_play.src.x = vidix_play.src.y = 0;
	vidix_play.src.w = src_width;
	vidix_play.src.h = src_height;
	vidix_play.dest.x = x_org;
	vidix_play.dest.y = y_org;
	vidix_play.dest.w = dst_width;
	vidix_play.dest.h = dst_height;
	vidix_play.num_frames=NUM_FRAMES;
	if((err=vdlConfigPlayback(vidix_handler,&vidix_play))!=0)
	{
		printf("vosub_vidix: Can't configure playback: %s\n",strerror(err));
		return -1;
	}
	if((err=vdlPlaybackOn(vidix_handler))!=0)
	{
                printf("vosub_vidix: Can't start playback: %s\n",strerror(err));
		return -1;
	}

	frames[0] = vidix_play.offsets[0];
	for(i=1;i<vidix_play.num_frames;i++) frames[i] = vidix_play.offsets[i];
	next_frame = 0;
	vidix_mem =vidix_play.dga_addr;

	/*clear the buffer*/
	memset(vidix_mem + frames[0],0x80,vidix_play.frame_size*vidix_play.num_frames);
	return 0;  
}

void vidix_term( void )
{
  if(verbose > 1) printf("vosub_vidix: vidix_term() was called\n");
	vdlPlaybackOff(vidix_handler);
	vdlClose(vidix_handler);
}

uint32_t vidix_draw_slice_420(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    unsigned bespitch,apitch;
    int i;
    apitch = vidix_play.dest.pitch.y-1;
    bespitch = (w + apitch) & ~apitch;

    dest = vidix_mem + frames[next_frame] + vidix_play.offset.y;
    dest += bespitch*y + x;
    src = image[0];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[0];
        dest += bespitch;
    }

    apitch = vidix_play.dest.pitch.v-1;
    bespitch = (w + apitch) & ~apitch;
    dest = vidix_mem + frames[next_frame] + vidix_play.offset.v;
    dest += bespitch*y/4 + x;
    src = image[1];
    for(i=0;i<h/2;i++){
        memcpy(dest,src,w/2);
        src+=stride[1];
        dest+=bespitch/2;
    }
    apitch = vidix_play.dest.pitch.u-1;
    bespitch = (w + apitch) & ~apitch;

    dest = vidix_mem + frames[next_frame] + vidix_play.offset.u;
    dest += bespitch*y/4 + x;
    src = image[2];
    for(i=0;i<h/2;i++){
        memcpy(dest,src,w/2);
        src+=stride[2];
        dest += bespitch/2;
    }
    return 0;
}

uint32_t vidix_draw_slice_422(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    unsigned bespitch,apitch;
    int i;
    apitch = vidix_play.dest.pitch.y-1;
    bespitch = (w*2 + apitch) & ~apitch;
    dest = vidix_mem + frames[next_frame] + vidix_play.offset.y;
    dest += bespitch*y + x;
    src = image[0];
    for(i=0;i<h;i++){
        memcpy(dest,src,w*2);
        src+=stride[0];
        dest += bespitch;
    }
}


uint32_t vidix_draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
 if(verbose > 1) printf("vosub_vidix: vidix_draw_slice() was called\n");
    if(src_format == IMGFMT_YV12 || src_format == IMGFMT_I420 || src_format == IMGFMT_IYUV)
	vidix_draw_slice_420(image,stride,w,h,x,y);
    else
	vidix_draw_slice_422(image,stride,w,h,x,y);
 return 0;
}

uint32_t vidix_draw_frame(uint8_t *image[])
{
  if(verbose > 1) printf("vosub_vidix: vidix_draw_frame() was called\n");
/* Note it's very strange but sometime for YUY2 draw_frame is called */
    if(src_format == IMGFMT_YV12 || src_format == IMGFMT_I420 || src_format == IMGFMT_IYUV)
    {
	printf("vosub_vidix: draw_frame for i420 is called\nExiting...\n");
	vidix_term();
	exit( EXIT_FAILURE );
    }
    else
    {
	int stride[1];
	stride[0] = vidix_play.src.w*2;
	vidix_draw_slice_422(image,stride,vidix_play.src.w,vidix_play.src.h,
			     vidix_play.src.x,vidix_play.src.y);
    }
  return 0;
}

void     vidix_flip_page(void)
{
  if(verbose > 1) printf("vosub_vidix: vidix_flip_page() was called\n");
  if(vo_doublebuffering)
  {
	next_frame=(next_frame+1)%vidix_play.num_frames;
	vdlPlaybackFrameSelect(vidix_handler,next_frame);
  }	
}

static void draw_alpha_null(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
  UNUSED(x0);
  UNUSED(y0);
  UNUSED(w);
  UNUSED(h);
  UNUSED(src);
  UNUSED(srca);
  UNUSED(stride);
}

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
    uint32_t bespitch = vidix_play.src.w + ((vidix_play.dest.pitch.y-1) & ~(vidix_play.dest.pitch.y-1));
    void *lvo_mem;
    lvo_mem = vidix_mem + frames[next_frame] + vidix_play.offset.y;
    switch(vidix_play.fourcc){
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
    case IMGFMT_I420:
        vo_draw_alpha_yv12(w,h,src,srca,stride,lvo_mem+bespitch*y0+x0,bespitch);
        break;
    case IMGFMT_YUY2:
        vo_draw_alpha_yuy2(w,h,src,srca,stride,lvo_mem+2*(bespitch*y0+x0),bespitch);
        break;
    case IMGFMT_UYVY:
        vo_draw_alpha_yuy2(w,h,src,srca,stride,lvo_mem+2*(bespitch*y0+x0)+1,bespitch);
        break;
    default:
        draw_alpha_null(x0,y0,w,h,src,srca,stride);
    }
}

void     vidix_draw_osd(void)
{
  if(verbose > 1) printf("vosub_vidix: vidix_draw_osd() was called\n");
  /* TODO: hw support */
#if 0
/* disable this stuff until new fbvid.h interface will be implemented
  because in different fourcc radeon_vid and rage128_vid have different
  width alignment */
  vo_draw_text(vidix_play.src.w,vidix_play.src.h,draw_alpha);
#endif
}

uint32_t vidix_query_fourcc(uint32_t format)
{
  if(verbose > 1) printf("vosub_vidix: query_format was called: %x (%s)\n",format,vo_format_name(format));
  vidix_fourcc.fourcc = format;
  vdlQueryFourcc(vidix_handler,&vidix_fourcc);
  return vidix_fourcc.depth != VID_DEPTH_NONE;
}
