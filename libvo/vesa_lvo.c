/*
 * vo_vesa interface to Linux Video Overlay
 * (partly based on vo_mga.c)
 *
 * copyright (C) 2001 Nick Kurshev <nickols_k@mail.ru>
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
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "vesa_lvo.h"
#include "libmpcodecs/img_format.h"
#include "drivers/mga_vid.h" /* <- should be changed to "linux/'something'.h" */
#include "fastmemcpy.h"
#include "osd.h"
#include "video_out.h"
#include "sub.h"
#include "libmpcodecs/vfcap.h"

#define WIDTH_ALIGN 32 /* should be 16 for rage:422 and 32 for rage:420 */
#define NUM_FRAMES 10
#define UNUSED(x) ((void)(x)) /**< Removes warning about unused arguments */

static uint8_t *frames[NUM_FRAMES];

static int lvo_handler = -1;
static uint8_t *lvo_mem = NULL;
static uint8_t next_frame;
static mga_vid_config_t mga_vid_config;
static unsigned image_bpp,image_height,image_width,src_format;
uint32_t vlvo_control(uint32_t request, void *data, ...);

#define PIXEL_SIZE() ((video_mode_info.BitsPerPixel+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (video_mode_info.XResolution*(pixel_size) )
#define IMAGE_LINE_SIZE(pixel_size) (image_width*(pixel_size))

extern vo_functions_t video_out_vesa;

int vlvo_preinit(const char *drvname)
{
  mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported);
  return -1;
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: vlvo_preinit(%s) was called\n",drvname);}
	lvo_handler = open(drvname,O_RDWR);
	if(lvo_handler == -1)
	{
 		mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_CouldntOpen,drvname);
		return -1;
	}
	/* we are able to tune up this stuff depend on fourcc format */
	video_out_vesa.draw_slice=vlvo_draw_slice;
	video_out_vesa.draw_frame=vlvo_draw_frame;
	video_out_vesa.flip_page=vlvo_flip_page;
	video_out_vesa.draw_osd=vlvo_draw_osd;
  video_out_vesa.control=vlvo_control;
	return 0;
}

int      vlvo_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp)
{
  size_t i,awidth;
  mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported);
  return -1;
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: vlvo_init() was called\n");}
	image_width = src_width;
	image_height = src_height;
	mga_vid_config.version=MGA_VID_VERSION;
        src_format = mga_vid_config.format=format;
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
            mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_InvalidOutputFormat,vo_format_name(format),format);
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
                mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_IncompatibleDriverVersion);
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
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: vlvo_term() was called\n");}
	ioctl( lvo_handler,MGA_VID_OFF,0 );
	munmap(frames[0],mga_vid_config.frame_size*mga_vid_config.num_frames);
	if(lvo_handler != -1) close(lvo_handler);
}

uint32_t vlvo_draw_slice_420(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    uint32_t bespitch,bespitch2;
    int i;

    bespitch = (mga_vid_config.src_width + (WIDTH_ALIGN-1)) & ~(WIDTH_ALIGN-1);
    bespitch2 = bespitch/2;

    dest = lvo_mem + bespitch * y + x;
    src = image[0];
    for(i=0;i<h;i++){
        fast_memcpy(dest,src,w);
        src+=stride[0];
        dest += bespitch;
    }

    w/=2;h/=2;x/=2;y/=2;

    dest = lvo_mem + bespitch*mga_vid_config.src_height + bespitch2 * y + x;
    src = image[1];
    for(i=0;i<h;i++){
        fast_memcpy(dest,src,w);
        src+=stride[1];
        dest += bespitch2;
    }

    dest = lvo_mem + bespitch*mga_vid_config.src_height
                   + bespitch*mga_vid_config.src_height / 4
                   + bespitch2 * y + x;
    src = image[2];
    for(i=0;i<h;i++){
        fast_memcpy(dest,src,w);
        src+=stride[2];
        dest += bespitch2;
    }
    return 0;
}

uint32_t vlvo_draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
 if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
   mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: vlvo_draw_slice() was called\n");}
    if(src_format == IMGFMT_YV12 || src_format == IMGFMT_I420 || src_format == IMGFMT_IYUV)
	vlvo_draw_slice_420(image,stride,w,h,x,y);
    else
    {
	uint8_t *dst;
	uint8_t bytpp;
	bytpp = (image_bpp+7)/8;
	dst = lvo_mem + (image_width * y + x)*bytpp;
	/* vlvo_draw_slice_422(image,stride,w,h,x,y); just for speed */
	fast_memcpy(dst,image[0],mga_vid_config.frame_size);
    }
 return 0;
}

uint32_t vlvo_draw_frame(uint8_t *image[])
{
/* Note it's very strange but sometime for YUY2 draw_frame is called */
  fast_memcpy(lvo_mem,image[0],mga_vid_config.frame_size);
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: vlvo_flip_page() was called\n");}
  return 0;
}

void     vlvo_flip_page(void)
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: vlvo_draw_osd() was called\n");}
  if(vo_doublebuffering)
  {
	ioctl(lvo_handler,MGA_VID_FSEL,&next_frame);
	next_frame=(next_frame+1)%mga_vid_config.num_frames;
	lvo_mem=frames[next_frame];
  }	
}

#if 0
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
    uint32_t bespitch = /*(*/mga_vid_config.src_width;// + 15) & ~15;
    switch(mga_vid_config.format){
    case IMGFMT_BGR15:
    case IMGFMT_RGB15:
	vo_draw_alpha_rgb15(w,h,src,srca,stride,lvo_mem+2*(y0*bespitch+x0),2*bespitch);
	break;
    case IMGFMT_BGR16:
    case IMGFMT_RGB16:
	vo_draw_alpha_rgb16(w,h,src,srca,stride,lvo_mem+2*(y0*bespitch+x0),2*bespitch);
	break;
    case IMGFMT_BGR24:
    case IMGFMT_RGB24:
	vo_draw_alpha_rgb24(w,h,src,srca,stride,lvo_mem+3*(y0*bespitch+x0),3*bespitch);
	break;
    case IMGFMT_BGR32:
    case IMGFMT_RGB32:
	vo_draw_alpha_rgb32(w,h,src,srca,stride,lvo_mem+4*(y0*bespitch+x0),4*bespitch);
	break;
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
#endif

void     vlvo_draw_osd(void)
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2,"vesa_lvo: vlvo_draw_osd() was called\n"); }
  /* TODO: hw support */
#if 0
/* disable this stuff until new fbvid.h interface will be implemented
  because in different fourcc radeon_vid and rage128_vid have different
  width alignment */
  vo_draw_text(mga_vid_config.src_width,mga_vid_config.src_height,draw_alpha);
#endif
}

uint32_t vlvo_query_info(uint32_t format)
{
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
    mp_msg(MSGT_VO,MSGL_DBG2, "vesa_lvo: query_format was called: %x (%s)\n",format,vo_format_name(format)); }
  return VFCAP_CSP_SUPPORTED;
}

uint32_t vlvo_control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return vlvo_query_info(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
