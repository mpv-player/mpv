/*
 *  vosub_vidix.c
 *
 *	Copyright (C) Nick Kurshev <nickols_k@mail.ru> - 2002
 *	Copyright (C) Alex Beregszaszi
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
#include <errno.h>

#include "config.h"

#include "vosub_vidix.h"
#include "../vidix/vidixlib.h"
#include "fastmemcpy.h"
#include "osd.h"
#include "video_out.h"
#include "../mp_image.h"


#define NUM_FRAMES VID_PLAY_MAXFRAMES /* Temporary: driver will overwrite it */
#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

static VDL_HANDLE vidix_handler = NULL;
static uint8_t *vidix_mem = NULL;
static uint8_t next_frame;
static unsigned image_Bpp,image_height,image_width,src_format;
extern int verbose;
static int video_on=0;

static vidix_capability_t vidix_cap;
static vidix_playback_t   vidix_play;
static vidix_fourcc_t	  vidix_fourcc;
static vo_functions_t *   vo_server;
static vidix_yuv_t	  dstrides;
static uint32_t (*server_control)(uint32_t request, void *data, ...);


static int  vidix_get_bes_da(bes_da_t *);
static int  vidix_get_video_eq(vidix_video_eq_t *info);
static int  vidix_set_video_eq(const vidix_video_eq_t *info);
static int  vidix_get_num_fx(unsigned *info);
static int  vidix_get_oem_fx(vidix_oem_fx_t *info);
static int  vidix_set_oem_fx(const vidix_oem_fx_t *info);
static int  vidix_set_deint(const vidix_deinterlace_t *info);

static void vidix_query_vaa(vo_vaa_t *vaa)
{
  memset(vaa,0,sizeof(vo_vaa_t));
  vaa->query_bes_da=vidix_get_bes_da;
  vaa->get_video_eq=vidix_get_video_eq;
  vaa->set_video_eq=vidix_set_video_eq;
  vaa->get_num_fx=vidix_get_num_fx;
  vaa->get_oem_fx=vidix_get_oem_fx;
  vaa->set_oem_fx=vidix_set_oem_fx;
  vaa->set_deint=vidix_set_deint;
}

extern int vo_gamma_brightness;
extern int vo_gamma_saturation;
extern int vo_gamma_contrast;
extern int vo_gamma_hue;
extern int vo_gamma_red_intensity;
extern int vo_gamma_green_intensity;
extern int vo_gamma_blue_intensity;

static vidix_video_eq_t vid_eq;

int vidix_start(void)
{
    int err;
    if((err=vdlPlaybackOn(vidix_handler))!=0)
    {
	printf("vosub_vidix: Can't start playback: %s\n",strerror(err));
	return -1;
    }
    video_on=1;
    if (vidix_cap.flags & FLAG_EQUALIZER)
    {
	if(verbose > 1)
	{
	    printf("vosub_vidix: vo_gamma_brightness=%i\n"
	       "vosub_vidix: vo_gamma_saturation=%i\n"
	       "vosub_vidix: vo_gamma_contrast=%i\n"
	       "vosub_vidix: vo_gamma_hue=%i\n"
	       "vosub_vidix: vo_gamma_red_intensity=%i\n"
	       "vosub_vidix: vo_gamma_green_intensity=%i\n"
	       "vosub_vidix: vo_gamma_blue_intensity=%i\n"
	       ,vo_gamma_brightness
	       ,vo_gamma_saturation
	       ,vo_gamma_contrast
	       ,vo_gamma_hue
	       ,vo_gamma_red_intensity
	       ,vo_gamma_green_intensity
	       ,vo_gamma_blue_intensity);
	}
        /* To use full set of vid_eq.cap */
	if(vidix_get_video_eq(&vid_eq) == 0)
	{
		vid_eq.brightness = vo_gamma_brightness;
		vid_eq.saturation = vo_gamma_saturation;
		vid_eq.contrast = vo_gamma_contrast;
		vid_eq.hue = vo_gamma_hue;
		vid_eq.red_intensity = vo_gamma_red_intensity;
		vid_eq.green_intensity = vo_gamma_green_intensity;
		vid_eq.blue_intensity = vo_gamma_blue_intensity;
		vid_eq.flags = VEQ_FLG_ITU_R_BT_601;
		vidix_set_video_eq(&vid_eq);
	}
    }
    return 0;
}

int vidix_stop(void)
{
    int err;
    if((err=vdlPlaybackOff(vidix_handler))!=0)
    {
	printf("vosub_vidix: Can't stop playback: %s\n",strerror(err));
	return -1;
    }
    video_on=0;
    return 0;
}

void vidix_term( void )
{
  if(verbose > 1) printf("vosub_vidix: vidix_term() was called\n");
	vidix_stop();
	vdlClose(vidix_handler);
}

static uint32_t vidix_draw_slice_420(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    int i;

    /* Plane Y */
    dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.y;
    dest += dstrides.y*y + x;
    src = image[0];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[0];
        dest += dstrides.y;
    }

    if (vidix_play.flags & VID_PLAY_INTERLEAVED_UV)
    {
        int hi,wi;
        uint8_t *src2;
        dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.v;
        dest += dstrides.y*y/2 + x; // <- is this correct ?
        h/=2;
        w/=2;
        src = image[1];
        src2 = image[2];
        for(hi = 0; hi < h; hi++)
        {
            for(wi = 0; wi < w; wi++)
            {
                dest[2*wi+0] = src[wi];
                dest[2*wi+1] = src2[wi];
            }
            dest += dstrides.y;
            src += stride[1];
	    src2+= stride[2];
	}
    }
    else 
    {
		/* Plane V */
		dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.v;
		dest += dstrides.v*y/4 + x;
		src = image[1];
		for(i=0;i<h/2;i++){
			memcpy(dest,src,w/2);
			src+=stride[1];
			dest+=dstrides.v/2;
		}

		/* Plane U */
		dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.u;
		dest += dstrides.u*y/4 + x;
		src = image[2];
		for(i=0;i<h/2;i++){
			memcpy(dest,src,w/2);
			src+=stride[2];
			dest += dstrides.u/2;
		}
		return 0;
    }
}

static uint32_t vidix_draw_slice_packed(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    int i;
    dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.y;
    dest += dstrides.y*y + x;
    src = image[0];
    for(i=0;i<h;i++){
        memcpy(dest,src,w*image_Bpp);
        src+=stride[0];
        dest += dstrides.y;
    }
    return 0;
}

static uint32_t vidix_draw_slice_packed_fast(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    int i;
    dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.y;
    dest += dstrides.y*y + x;
    src = image[0];
    memcpy(dest,src,h*dstrides.y);
    return 0;
}

uint32_t vidix_draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	printf("vosub_vidix: Error unoptimized draw_slice was called\nExiting...");
	vidix_term();
	exit( EXIT_FAILURE );
	return 0;
}

uint32_t vidix_draw_frame(uint8_t *image[])
{
  int stride[1];
  if(verbose > 1) printf("vosub_vidix: vidix_draw_frame() was called\n");
/* Note it's very strange but sometime for YUY2 draw_frame is called */
    if(src_format == IMGFMT_YV12 || src_format == IMGFMT_I420 || src_format == IMGFMT_IYUV)
	printf("vosub_vidix: draw_frame for YUV420 called, frame cannot be written\n");
    else
    if(src_format == IMGFMT_RGB32 || src_format == IMGFMT_BGR32)
	stride[0] = vidix_play.src.w*4;
    else
    if(src_format == IMGFMT_RGB24 || src_format == IMGFMT_BGR24)
	stride[0] = vidix_play.src.w*3;
    else
	stride[0] = vidix_play.src.w*2;
    return vo_server->draw_slice(image,stride,vidix_play.src.w,vidix_play.src.h,
				 vidix_play.src.x,vidix_play.src.y);
}

void     vidix_flip_page(void)
{
  if(verbose > 1) printf("vosub_vidix: vidix_flip_page() was called\n");
  if(vo_doublebuffering)
  {
	vdlPlaybackFrameSelect(vidix_handler,next_frame);
	next_frame=(next_frame+1)%vidix_play.num_frames;
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
    uint32_t apitch,bespitch;
    void *lvo_mem;
    lvo_mem = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.y;
    apitch = vidix_play.dest.pitch.y-1;
    switch(vidix_play.fourcc){
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
    case IMGFMT_I420:
	bespitch = (vidix_play.src.w + apitch) & (~apitch);
        vo_draw_alpha_yv12(w,h,src,srca,stride,lvo_mem+bespitch*y0+x0,bespitch);
        break;
    case IMGFMT_YUY2:
	bespitch = (vidix_play.src.w*2 + apitch) & (~apitch);
        vo_draw_alpha_yuy2(w,h,src,srca,stride,lvo_mem+bespitch*y0+2*x0,bespitch);
        break;
    case IMGFMT_UYVY:
	bespitch = (vidix_play.src.w*2 + apitch) & (~apitch);
        vo_draw_alpha_yuy2(w,h,src,srca,stride,lvo_mem+bespitch*y0+2*x0+1,bespitch);
        break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
	bespitch = (vidix_play.src.w*4 + apitch) & (~apitch);
	vo_draw_alpha_rgb32(w,h,src,srca,stride,lvo_mem+y0*bespitch+4*x0,bespitch);
        break;
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
	bespitch = (vidix_play.src.w*3 + apitch) & (~apitch);
	vo_draw_alpha_rgb24(w,h,src,srca,stride,lvo_mem+y0*bespitch+3*x0,bespitch);
        break;
    case IMGFMT_RGB16:
    case IMGFMT_BGR16:
	bespitch = (vidix_play.src.w*2 + apitch) & (~apitch);
	vo_draw_alpha_rgb16(w,h,src,srca,stride,lvo_mem+y0*bespitch+2*x0,bespitch);
        break;
    case IMGFMT_RGB15:
    case IMGFMT_BGR15:
	bespitch = (vidix_play.src.w*2 + apitch) & (~apitch);
	vo_draw_alpha_rgb15(w,h,src,srca,stride,lvo_mem+y0*bespitch+2*x0,bespitch);
        break;
    default:
        draw_alpha_null(x0,y0,w,h,src,srca,stride);
    }
}

void     vidix_draw_osd(void)
{
  if(verbose > 1) printf("vosub_vidix: vidix_draw_osd() was called\n");
  /* TODO: hw support */
  vo_draw_text(vidix_play.src.w,vidix_play.src.h,draw_alpha);
}

uint32_t vidix_query_fourcc(uint32_t format)
{
  if(verbose > 1) printf("vosub_vidix: query_format was called: %x (%s)\n",format,vo_format_name(format));
  vidix_fourcc.fourcc = format;
  vdlQueryFourcc(vidix_handler,&vidix_fourcc);
  if (vidix_fourcc.depth == VID_DEPTH_NONE) return(0);
  return(0x2); /* hw support without conversion */
}

int vidix_grkey_support(void)
{
    return(vidix_fourcc.flags & VID_CAP_COLORKEY);
}

int vidix_grkey_get(vidix_grkey_t *gr_key)
{
    return(vdlGetGrKeys(vidix_handler, gr_key));
}

int vidix_grkey_set(const vidix_grkey_t *gr_key)
{
    return(vdlSetGrKeys(vidix_handler, gr_key));
}

static int  vidix_get_bes_da(bes_da_t *info)
{
  if(!video_on) return EPERM;
  info->dest.x = vidix_play.src.x;
  info->dest.y = vidix_play.src.y;
  info->dest.w = vidix_play.src.w;
  info->dest.h = vidix_play.src.h;
  info->dest.pitch.y = vidix_play.dest.pitch.y;
  info->dest.pitch.u = vidix_play.dest.pitch.u;
  info->dest.pitch.v = vidix_play.dest.pitch.v;
  info->flags = vidix_play.flags;
  info->frame_size = vidix_play.frame_size;
  info->num_frames = vidix_play.num_frames;
  memcpy(info->offsets,vidix_play.offsets,sizeof(unsigned)*vidix_play.num_frames);
  memcpy(&info->offset,&vidix_play.offset,sizeof(vidix_yuv_t));
  info->dga_addr = vidix_play.dga_addr;
  return 0;
}

static int  vidix_get_video_eq(vidix_video_eq_t *info)
{
  if(!video_on) return EPERM;
  return vdlPlaybackGetEq(vidix_handler, info);
}

static int  vidix_set_video_eq(const vidix_video_eq_t *info)
{
  if(!video_on) return EPERM;
  return vdlPlaybackSetEq(vidix_handler, info);
}

static int  vidix_get_num_fx(unsigned *info)
{
  if(!video_on) return EPERM;
  return vdlQueryNumOemEffects(vidix_handler, info);
}

static int  vidix_get_oem_fx(vidix_oem_fx_t *info)
{
  if(!video_on) return EPERM;
  return vdlGetOemEffect(vidix_handler, info);
}

static int  vidix_set_oem_fx(const vidix_oem_fx_t *info)
{
  if(!video_on) return EPERM;
  return vdlSetOemEffect(vidix_handler, info);
}

static int  vidix_set_deint(const vidix_deinterlace_t *info)
{
  if(!video_on) return EPERM;
  return vdlPlaybackSetDeint(vidix_handler, info);
}

int      vidix_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp,
		   unsigned vid_w,unsigned vid_h,const void *info)
{
  size_t i,awidth;
  int err,is_422_planes_eq;
  uint32_t sstride,apitch;
  if(verbose > 1)
     printf("vosub_vidix: vidix_init() was called\n"
    	    "src_w=%u src_h=%u dest_x_y_w_h = %u %u %u %u\n"
	    "format=%s dest_bpp=%u vid_w=%u vid_h=%u\n"
	    ,src_width,src_height,x_org,y_org,dst_width,dst_height
	    ,vo_format_name(format),dest_bpp,vid_w,vid_h);

	if(((vidix_cap.maxwidth != -1) && (vid_w > vidix_cap.maxwidth)) ||
	    ((vidix_cap.minwidth != -1) && (vid_w < vidix_cap.minwidth)) ||
	    ((vidix_cap.maxheight != -1) && (vid_h > vidix_cap.maxheight)) ||
	    ((vidix_cap.minwidth != -1 ) && (vid_h < vidix_cap.minheight)))
	{
	  printf("vosub_vidix: video server has unsupported resolution (%dx%d), supported: %dx%d-%dx%d\n",
	    vid_w, vid_h, vidix_cap.minwidth, vidix_cap.minheight,
	    vidix_cap.maxwidth, vidix_cap.maxheight);
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
	  case 15:err = ((vidix_fourcc.depth & VID_DEPTH_15BPP) != VID_DEPTH_15BPP); break;
	  case 16:err = ((vidix_fourcc.depth & VID_DEPTH_16BPP) != VID_DEPTH_16BPP); break;
	  case 24:err = ((vidix_fourcc.depth & VID_DEPTH_24BPP) != VID_DEPTH_24BPP); break;
	  case 32:err = ((vidix_fourcc.depth & VID_DEPTH_32BPP) != VID_DEPTH_32BPP); break;
	  default: err=1; break;
	}
	if(err)
	{
	  printf("vosub_vidix: video server has unsupported color depth by vidix (%d)\n"
	  ,vidix_fourcc.depth);
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER)
	{
	  printf("vosub_vidix: vidix driver can't upscale image (%d%d -> %d%d)\n",
	  src_width, src_height, dst_width, dst_height);
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
	{
	  printf("vosub_vidix: vidix driver can't downscale image (%d%d -> %d%d)\n",
	  src_width, src_height, dst_width, dst_height);
	  return -1;
	}
	image_width = src_width;
	image_height = src_height;
	src_format = format;
	memset(&vidix_play,0,sizeof(vidix_playback_t));
	vidix_play.fourcc = format;
	vidix_play.capability = vidix_cap.flags; /* every ;) */
	vidix_play.blend_factor = 0; /* for now */
	/* display the full picture.
	   Nick: we could implement here zooming to a specified area -- alex */
	vidix_play.src.x = vidix_play.src.y = 0;
	vidix_play.src.w = src_width;
	vidix_play.src.h = src_height;
	vidix_play.dest.x = x_org;
	vidix_play.dest.y = y_org;
	vidix_play.dest.w = dst_width;
	vidix_play.dest.h = dst_height;
	vidix_play.num_frames=vo_doublebuffering?NUM_FRAMES-1:1;
	vidix_play.src.pitch.y = vidix_play.src.pitch.u = vidix_play.src.pitch.v = 0;
	if(info)
	{
	switch(((const vo_tune_info_t *)info)->pitch[0])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: vidix_play.src.pitch.y = ((const vo_tune_info_t *)info)->pitch[0];
		      break;
	    default: break;
	}
	switch(((const vo_tune_info_t *)info)->pitch[1])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: vidix_play.src.pitch.u = ((const vo_tune_info_t *)info)->pitch[1];
		      break;
	    default: break;
	}
	switch(((const vo_tune_info_t *)info)->pitch[2])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: vidix_play.src.pitch.v = ((const vo_tune_info_t *)info)->pitch[2];
		      break;
	    default: break;
	}
	}
	if((err=vdlConfigPlayback(vidix_handler,&vidix_play))!=0)
	{
		printf("vosub_vidix: Can't configure playback: %s\n",strerror(err));
		return -1;
	}
	printf("vosub_vidix: using %d buffers\n", vidix_play.num_frames);

	vidix_mem = vidix_play.dga_addr;

	/* select first frame */
	next_frame = 0;
//        vdlPlaybackFrameSelect(vidix_handler,next_frame);

	/* clear every frame with correct address and frame_size */
	for (i = 0; i < vidix_play.num_frames; i++)
	    memset(vidix_mem + vidix_play.offsets[i], 0x80,
		vidix_play.frame_size);
	switch(format)
	{
	    /*
	    case IMGFMT_YV09:
	    case IMGFMT_IF09:
	    */
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
	    case IMGFMT_YV12:
		apitch = vidix_play.dest.pitch.y-1;
		dstrides.y = (image_width + apitch) & ~apitch;
		apitch = vidix_play.dest.pitch.v-1;
		dstrides.v = (image_width + apitch) & ~apitch;
		apitch = vidix_play.dest.pitch.u-1;
		dstrides.u = (image_width + apitch) & ~apitch;
		image_Bpp=1;
		break;
	    case IMGFMT_RGB32:
	    case IMGFMT_BGR32:
		apitch = vidix_play.dest.pitch.y-1;
		dstrides.y = (image_width*4 + apitch) & ~apitch;
		dstrides.u = dstrides.v = 0;
		image_Bpp=4;
		break;
	    case IMGFMT_RGB24:
	    case IMGFMT_BGR24:
		apitch = vidix_play.dest.pitch.y-1;
		dstrides.y = (image_width*3 + apitch) & ~apitch;
		dstrides.u = dstrides.v = 0;
		image_Bpp=3;
		break;
	    default:
		apitch = vidix_play.dest.pitch.y-1;
		dstrides.y = (image_width*2 + apitch) & ~apitch;
		dstrides.u = dstrides.v = 0;
		image_Bpp=2;
		break;
	}
        /* tune some info here */
	sstride = src_width*2;
	is_422_planes_eq = sstride == dstrides.y;
        if(src_format == IMGFMT_YV12 || src_format == IMGFMT_I420 || src_format == IMGFMT_IYUV)
		vo_server->draw_slice = vidix_draw_slice_420;
	else	vo_server->draw_slice =
			is_422_planes_eq ?
			vidix_draw_slice_packed_fast:
			vidix_draw_slice_packed;
	return 0;
}

static uint32_t vidix_get_image(mp_image_t *mpi)
{
    if(mpi->type==MP_IMGTYPE_STATIC && vidix_play.num_frames>1) return VO_FALSE;
    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; /* slow video ram */
    mpi->planes[0]=vidix_mem+vidix_play.offsets[next_frame]+vidix_play.offset.y;
    mpi->stride[0]=dstrides.y;
    if(mpi->flags&MP_IMGFLAG_PLANAR)
    {
	mpi->planes[1]=vidix_mem+vidix_play.offsets[next_frame]+vidix_play.offset.v;
	mpi->stride[1]=dstrides.v;
	mpi->planes[2]=vidix_mem+vidix_play.offsets[next_frame]+vidix_play.offset.u;
	mpi->stride[2]=dstrides.u;
    }
    mpi->flags|=MP_IMGFLAG_DIRECT;
    return VO_TRUE;
}

uint32_t vidix_control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_VAA:
    vidix_query_vaa((vo_vaa_t*)data);
    return VO_TRUE;
  case VOCTRL_QUERY_FORMAT:
    return vidix_query_fourcc(*((uint32_t*)data));
  case VOCTRL_SCREENSHOT:
    return (*server_control)(request,data);
  case VOCTRL_GET_IMAGE:
    return vidix_get_image(data);
  case VOCTRL_GET_FRAME_NUM:
	*(uint32_t *)data = next_frame;
	return VO_TRUE;
  case VOCTRL_SET_FRAME_NUM:
	next_frame = *(uint32_t *)data;
	return VO_TRUE;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = vidix_play.num_frames;
	return VO_TRUE;
  }
  return VO_NOTIMPL;
}

int vidix_preinit(const char *drvname,void *server)
{
  int err;
  if(verbose > 1) printf("vosub_vidix: vidix_preinit(%s) was called\n",drvname);
	if(vdlGetVersion() != VIDIX_VERSION)
	{
	  printf("vosub_vidix: You have wrong version of VIDIX library\n");
	  return -1;
	}
	vidix_handler = vdlOpen(LIBDIR"/vidix/",
				drvname ? drvname[0] == ':' ? &drvname[1] : drvname[0] ? drvname : NULL : NULL,
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
	printf("vosub_vidix: Using: %s by %s\n",vidix_cap.name,vidix_cap.author);
	/* we are able to tune up this stuff depend on fourcc format */
	((vo_functions_t *)server)->draw_slice=vidix_draw_slice;
	((vo_functions_t *)server)->draw_frame=vidix_draw_frame;
	((vo_functions_t *)server)->flip_page=vidix_flip_page;
	((vo_functions_t *)server)->draw_osd=vidix_draw_osd;
	server_control = ((vo_functions_t *)server)->control;
	((vo_functions_t *)server)->control=vidix_control;
	vo_server = server;
	return 0;
}
