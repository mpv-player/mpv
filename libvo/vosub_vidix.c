/*
 * vidix interface to any mplayer vo driver
 * (partly based on vesa_lvo.c)
 *
 * copyright (C) 2002 Nick Kurshev <nickols_k@mail.ru>
 * copyright (C) Alex Beregszaszi
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

#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef __MINGW32__
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "vidix/vidix.h"
#include "fastmemcpy.h"
#include "osd.h"
#include "video_out.h"
#include "sub.h"
#include "vosub_vidix.h"

#include "libmpcodecs/vfcap.h"
#include "libmpcodecs/mp_image.h"

#define NUM_FRAMES VID_PLAY_MAXFRAMES /* Temporary: driver will overwrite it */

static VDXContext *vidix_handler = NULL;
static uint8_t *vidix_mem = NULL;
static uint8_t next_frame;
static unsigned image_Bpp,image_height,image_width,src_format,forced_fourcc=0;
static int video_on=0;

static vidix_capability_t vidix_cap;
static vidix_playback_t   vidix_play;
static vidix_fourcc_t	  vidix_fourcc;
static vo_functions_t *   vo_server;
static vidix_yuv_t	  dstrides;
/*static uint32_t (*server_control)(uint32_t request, void *data, ...);*/

int vidix_start(void)
{
    int err;
    if((err=vdlPlaybackOn(vidix_handler))!=0)
    {
	mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback,strerror(err));
	return -1;
    }
    video_on=1;
    return 0;
}

int vidix_stop(void)
{
    int err;
    if((err=vdlPlaybackOff(vidix_handler))!=0)
    {
	mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback,strerror(err));
	return -1;
    }
    video_on=0;
    return 0;
}

void vidix_term( void )
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: vidix_term() was called\n"); }
	vidix_stop();
	vdlClose(vidix_handler);
//  vo_server->control=server_control;
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
    return -1;
}

static uint32_t vidix_draw_slice_410(uint8_t *image[], int stride[], int w,int h,int x,int y)
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
	mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported);
    }
    else
    {
		/* Plane V */
		dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.v;
		dest += dstrides.v*y/8 + x;
		src = image[1];
		for(i=0;i<h/4;i++){
			memcpy(dest,src,w/4);
			src+=stride[1];
			dest+=dstrides.v/4;
		}

		/* Plane U */
		dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.u;
		dest += dstrides.u*y/8 + x;
		src = image[2];
		for(i=0;i<h/4;i++){
			memcpy(dest,src,w/4);
			src+=stride[2];
			dest += dstrides.u/4;
		}
		return 0;
    }
    return -1;
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

static uint32_t vidix_draw_slice_nv12(uint8_t *image[], int stride[], int w,int h,int x,int y)
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

    /* Plane UV */
    dest = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.u;
    dest += dstrides.u*y/2 + x;
    src = image[1];
    for(i=0;i<h/2;i++){
        memcpy(dest,src,w);
        src+=stride[1];
        dest+=dstrides.u;
    }
    return 0;
}

static uint32_t vidix_draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled);
    return -1;
}

static uint32_t  vidix_draw_image(mp_image_t *mpi){
    if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
      mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: vidix_draw_image() was called\n"); }

    // if -dr or -slices then do nothing:
    if(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK)) return VO_TRUE;

    vo_server->draw_slice(mpi->planes,mpi->stride,
	vidix_play.src.w,vidix_play.src.h,vidix_play.src.x,vidix_play.src.y);
    return VO_TRUE;
}

static uint32_t vidix_draw_frame(uint8_t *image[])
{
  mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled);
  return -1;
}

static void     vidix_flip_page(void)
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: vidix_flip_page() was called\n"); }
  if(vo_doublebuffering)
  {
	vdlPlaybackFrameSelect(vidix_handler,next_frame);
	next_frame=(next_frame+1)%vidix_play.num_frames;
  }
}

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
    uint32_t apitch,bespitch;
    char *lvo_mem;
    lvo_mem = vidix_mem + vidix_play.offsets[next_frame] + vidix_play.offset.y;
    apitch = vidix_play.dest.pitch.y-1;
    switch(vidix_play.fourcc){
    case IMGFMT_NV12:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
    case IMGFMT_I420:
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
    case IMGFMT_Y8:
    case IMGFMT_Y800:
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
	return;
    }
}

static void     vidix_draw_osd(void)
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: vidix_draw_osd() was called\n"); }
  /* TODO: hw support */
  vo_draw_text(vidix_play.src.w,vidix_play.src.h,draw_alpha);
}

uint32_t vidix_query_fourcc(uint32_t format)
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: query_format was called: %x (%s)\n",format,vo_format_name(format)); }
  vidix_fourcc.fourcc = format;
  vdlQueryFourcc(vidix_handler,&vidix_fourcc);
  if (vidix_fourcc.depth == VID_DEPTH_NONE)
    return 0;
  return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN|VFCAP_OSD|VFCAP_ACCEPT_STRIDE;
}

int vidix_grkey_support(void)
{
    return vidix_fourcc.flags & VID_CAP_COLORKEY;
}

int vidix_grkey_get(vidix_grkey_t *gr_key)
{
    return vdlGetGrKeys(vidix_handler, gr_key);
}

int vidix_grkey_set(const vidix_grkey_t *gr_key)
{
    return vdlSetGrKeys(vidix_handler, gr_key);
}


static int is_422_planes_eq=0;
int      vidix_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp,
		   unsigned vid_w,unsigned vid_h)
{
  void *tmp, *tmpa;
  size_t i;
  int err;
  uint32_t sstride,apitch;
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) )
     mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: vidix_init() was called\n"
    	    "src_w=%u src_h=%u dest_x_y_w_h = %u %u %u %u\n"
	    "format=%s dest_bpp=%u vid_w=%u vid_h=%u\n"
	    ,src_width,src_height,x_org,y_org,dst_width,dst_height
	    ,vo_format_name(format),dest_bpp,vid_w,vid_h);

	if(vidix_query_fourcc(format) == 0)
	{
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver,
	    format,vo_format_name(format));
	  return -1;
	}

	if(((vidix_cap.maxwidth != -1) && (vid_w > vidix_cap.maxwidth)) ||
	    ((vidix_cap.minwidth != -1) && (vid_w < vidix_cap.minwidth)) ||
	    ((vidix_cap.maxheight != -1) && (vid_h > vidix_cap.maxheight)) ||
	    ((vidix_cap.minwidth != -1 ) && (vid_h < vidix_cap.minheight)))
	{
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution,
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
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth
	  ,vidix_fourcc.depth);
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER)
	{
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage,
	  src_width, src_height, dst_width, dst_height);
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
	{
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage,
	  src_width, src_height, dst_width, dst_height);
	  return -1;
	}
	image_width = src_width;
	image_height = src_height;
	src_format = format;
	if(forced_fourcc) format = forced_fourcc;
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
//	vidix_play.num_frames=vo_doublebuffering?NUM_FRAMES-1:1;
	/* we aren't mad...3 buffers are more than enough */
	vidix_play.num_frames=vo_doublebuffering?3:1;
	vidix_play.src.pitch.y = vidix_play.src.pitch.u = vidix_play.src.pitch.v = 0;

	if((err=vdlConfigPlayback(vidix_handler,&vidix_play))!=0)
	{
 		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback,strerror(err));
		return -1;
	}
	if ( mp_msg_test(MSGT_VO,MSGL_V) ) {
		mp_msg(MSGT_VO,MSGL_V, "vosub_vidix: using %d buffer(s)\n", vidix_play.num_frames); }

	vidix_mem = vidix_play.dga_addr;

	tmp = calloc(image_width, image_height);
	tmpa = malloc(image_width * image_height);
	memset(tmpa, 1, image_width * image_height);
	/* clear every frame with correct address and frame_size */
	/* HACK: use draw_alpha to clear Y component */
	for (i = 0; i < vidix_play.num_frames; i++) {
	    next_frame = i;
	    memset(vidix_mem + vidix_play.offsets[i], 0x80,
		vidix_play.frame_size);
	    draw_alpha(0, 0, image_width, image_height, tmp, tmpa, image_width);
	}
	free(tmp);
	free(tmpa);
	/* show one of the "clear" frames */
	vidix_flip_page();

	switch(format)
	{
	    case IMGFMT_NV12:
	    case IMGFMT_YV12:
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
	    case IMGFMT_YVU9:
	    case IMGFMT_IF09:
	    case IMGFMT_Y800:
	    case IMGFMT_Y8:
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
	sstride = src_width*image_Bpp;
	if(!forced_fourcc)
	{
	    is_422_planes_eq = sstride == dstrides.y;

	    if(src_format == IMGFMT_YV12 || src_format == IMGFMT_I420 || src_format == IMGFMT_IYUV)
		 vo_server->draw_slice = vidix_draw_slice_420;
	    else if (src_format == IMGFMT_YVU9 || src_format == IMGFMT_IF09)
		 vo_server->draw_slice = vidix_draw_slice_410;
	    else if (src_format == IMGFMT_NV12)
		 vo_server->draw_slice = vidix_draw_slice_nv12;
	    else vo_server->draw_slice = vidix_draw_slice_packed;
	}
	return 0;
}

static uint32_t vidix_get_image(mp_image_t *mpi)
{
    if(mpi->type==MP_IMGTYPE_STATIC && vidix_play.num_frames>1) return VO_FALSE;
    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; /* slow video ram */
    if(( (mpi->stride[0]==dstrides.y && (!(mpi->flags&MP_IMGFLAG_PLANAR) ||
      (mpi->stride[1]==dstrides.u && mpi->stride[2]==dstrides.v)) )
      || (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH))) &&
       (!forced_fourcc && !(vidix_play.flags & VID_PLAY_INTERLEAVED_UV)))
    {
	if(mpi->flags&MP_IMGFLAG_ACCEPT_WIDTH){
	    // check if only width is enough to represent strides:
	    if(mpi->flags&MP_IMGFLAG_PLANAR){
		if((dstrides.y>>1)!=dstrides.v || dstrides.v!=dstrides.u) return VO_FALSE;
	    } else {
		if(dstrides.y % (mpi->bpp/8)) return VO_FALSE;
	    }
	}
	mpi->planes[0]=vidix_mem+vidix_play.offsets[next_frame]+vidix_play.offset.y;
	mpi->width=mpi->stride[0]=dstrides.y;
	if(mpi->flags&MP_IMGFLAG_PLANAR)
	{
	    mpi->planes[1]=vidix_mem+vidix_play.offsets[next_frame]+vidix_play.offset.v;
	    mpi->stride[1]=dstrides.v >> mpi->chroma_x_shift;
	    mpi->planes[2]=vidix_mem+vidix_play.offsets[next_frame]+vidix_play.offset.u;
	    mpi->stride[2]=dstrides.u >> mpi->chroma_x_shift;
	} else
	    mpi->width/=mpi->bpp/8;
	mpi->flags|=MP_IMGFLAG_DIRECT;
	return VO_TRUE;
    }
    return VO_FALSE;
}

uint32_t vidix_control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return vidix_query_fourcc(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return vidix_get_image(data);
  case VOCTRL_DRAW_IMAGE:
    return vidix_draw_image(data);
  case VOCTRL_GET_FRAME_NUM:
	*(uint32_t *)data = next_frame;
	return VO_TRUE;
  case VOCTRL_SET_FRAME_NUM:
	next_frame = *(uint32_t *)data;
	return VO_TRUE;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = vidix_play.num_frames;
	return VO_TRUE;
  case VOCTRL_SET_EQUALIZER:
  {
    va_list ap;
    int value;
    vidix_video_eq_t info;

    if(!video_on) return VO_FALSE;
    va_start(ap, data);
    value = va_arg(ap, int);
    va_end(ap);

//    printf("vidix seteq %s -> %d  \n",data,value);

    /* vidix eq ranges are -1000..1000 */
    if (!strcasecmp(data, "brightness"))
    {
	info.brightness = value*10;
	info.cap = VEQ_CAP_BRIGHTNESS;
    }
    else if (!strcasecmp(data, "contrast"))
    {
	info.contrast = value*10;
	info.cap = VEQ_CAP_CONTRAST;
    }
    else if (!strcasecmp(data, "saturation"))
    {
	info.saturation = value*10;
	info.cap = VEQ_CAP_SATURATION;
    }
    else if (!strcasecmp(data, "hue"))
    {
	info.hue = value*10;
	info.cap = VEQ_CAP_HUE;
    }

    if (vdlPlaybackSetEq(vidix_handler, &info) == 0)
	return VO_TRUE;
    return VO_FALSE;
  }
  case VOCTRL_GET_EQUALIZER:
  {
    va_list ap;
    int *value;
    vidix_video_eq_t info;

    if(!video_on) return VO_FALSE;
    if (vdlPlaybackGetEq(vidix_handler, &info) != 0)
	return VO_FALSE;

    va_start(ap, data);
    value = va_arg(ap, int*);
    va_end(ap);

    /* vidix eq ranges are -1000..1000 */
    if (!strcasecmp(data, "brightness"))
    {
	if (info.cap & VEQ_CAP_BRIGHTNESS)
	    *value = info.brightness/10;
    }
    else if (!strcasecmp(data, "contrast"))
    {
	if (info.cap & VEQ_CAP_CONTRAST)
	    *value = info.contrast/10;
    }
    else if (!strcasecmp(data, "saturation"))
    {
	if (info.cap & VEQ_CAP_SATURATION)
	    *value = info.saturation/10;
    }
    else if (!strcasecmp(data, "hue"))
    {
	if (info.cap & VEQ_CAP_HUE)
	    *value = info.hue/10;
    }

    return VO_TRUE;
  }
  }
  return VO_NOTIMPL;
  // WARNING: we drop extra parameters (...) here!
//  return server_control(request,data); //VO_NOTIMPL;
}

int vidix_preinit(const char *drvname,vo_functions_t *server)
{
  int err;
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vosub_vidix: vidix_preinit(%s) was called\n",drvname); }

	vidix_handler = vdlOpen(drvname ? drvname[0] == ':' ? &drvname[1] : drvname[0] ? drvname : NULL : NULL,
				TYPE_OUTPUT,
				verbose);

	if(vidix_handler == NULL)
	{
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver);
		return -1;
	}
	if((err=vdlGetCapability(vidix_handler,&vidix_cap)) != 0)
	{
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability,strerror(err));
		return -1;
	}
	mp_msg(MSGT_VO,MSGL_V, "[VO_SUB_VIDIX] Description: %s.\n", vidix_cap.name);
	mp_msg(MSGT_VO,MSGL_V, "[VO_SUB_VIDIX] Author: %s.\n", vidix_cap.author);
	/* we are able to tune up this stuff depend on fourcc format */
	server->draw_slice=vidix_draw_slice;
	server->draw_frame=vidix_draw_frame;
	server->flip_page=vidix_flip_page;
	server->draw_osd=vidix_draw_osd;
//	server_control = server->control;
//	server->control=vidix_control;
	vo_server = server;
	return 0;
}
