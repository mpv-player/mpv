/* 
 *  vo_tdfx_vid.c
 *
 *	Copyright (C) Alban Bedel - 03/2003
 *
 *  This file is part of MPlayer, a free movie player.
 *	
 *  MPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  MPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"

#include "fastmemcpy.h"
#include "drivers/tdfx_vid.h"


static vo_info_t info = 
{
	"tdfx_vid video output",
	"tdfx_vid",
	"Albeu",
	""
};

//#define VERBOSE

LIBVO_EXTERN(tdfx_vid)

static tdfx_vid_config_t tdfx_cfg;

static unsigned char* agp_mem = NULL;
static int tdfx_fd = -1;

static uint32_t img_fmt; // The real input format
static uint32_t src_width, src_height, src_fmt, src_bpp, src_stride;
static uint32_t dst_width, dst_height, dst_fmt, dst_bpp, dst_stride;

static uint32_t tdfx_page;
static uint32_t front_buffer;
static uint32_t back_buffer;
static uint8_t num_buffer = 3;
static uint32_t buffer_size; // Max size
static uint8_t current_buffer = 0;
static uint8_t current_ip_buf = 0;
static uint32_t buffer_stride[3];

// FIXME
static void clear_screen(void) {
  tdfx_vid_agp_move_t mov;

  memset(agp_mem,0,tdfx_cfg.screen_width*dst_bpp*tdfx_cfg.screen_height);
  
  mov.move2 = TDFX_VID_MOVE_2_PACKED;
  mov.width = tdfx_cfg.screen_width*dst_bpp;
  mov.height = tdfx_cfg.screen_height;
  mov.src = 0;
  mov.src_stride = tdfx_cfg.screen_width*dst_bpp;
  mov.dst = front_buffer;
  mov.dst_stride = tdfx_cfg.screen_stride;

  printf("Move %d(%d) x %d => %d \n", mov.width,mov.src_stride,mov.height,mov.dst_stride);

  if(ioctl(tdfx_fd,TDFX_VID_AGP_MOVE,&mov))
    printf("tdfx_vid: AGP move failed to clear the screen\n");
  
}

static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
  uint8_t* ptr[3];

#ifdef VERBOSE
  printf("Draw slices %d\n",current_buffer);
#endif
  switch(img_fmt) {
  case IMGFMT_YUY2:
  case IMGFMT_UYVY:
  case IMGFMT_BGR8:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
    // copy :( to agp_mem
    // still faster than tdfxfb wich directly copy to the video mem :)
    mem2agpcpy_pic(agp_mem + current_buffer * buffer_size + 
	       y*buffer_stride[0] + x * src_bpp,
	       image[0],
	       src_bpp*w,h,buffer_stride[0],stride[0]);
    return 0;

  case IMGFMT_YV12:
  case IMGFMT_I420:
    // Copy to agp mem
    ptr[0] = agp_mem + current_buffer * buffer_size;
    mem2agpcpy_pic(ptr[0] + y * buffer_stride[0] + x,image[0],w,h,
		   buffer_stride[0],stride[0]);
    ptr[1] = ptr[0] + (src_height*src_width);
    mem2agpcpy_pic(ptr[1] + y/2 * buffer_stride[1] + x/2,image[1],w/2,h/2,
		   buffer_stride[1],stride[1]);
    ptr[2] = ptr[1] + (src_height*src_width/4);
    mem2agpcpy_pic(ptr[2] + y/2 * buffer_stride[2] + x/2,image[2],w/2,h/2,
		   buffer_stride[2],stride[2]);
    return 0;
  }
    
  return 1;
}

static void draw_osd(void)
{
}

static void
flip_page(void)
{
  tdfx_vid_blit_t blit;
  //return;
  // Scale convert
#ifdef VERBOSE
  printf("Flip\n");
#endif
  memset(&blit,0,sizeof(tdfx_vid_blit_t));
  blit.src = back_buffer;
  blit.src_stride = src_stride;
  blit.src_x = 0;
  blit.src_y = 0;
  blit.src_w = src_width;
  blit.src_h = src_height;
  blit.src_format = src_fmt;

  blit.dst = front_buffer;
  blit.dst_stride = dst_stride;
  blit.dst_x = 0;
  blit.dst_y = 0;
  blit.dst_w = dst_width;
  blit.dst_h = dst_height;
  blit.dst_format = dst_fmt;

  if(ioctl(tdfx_fd,TDFX_VID_BLIT,&blit))
    printf("tdfx_vid: Blit failed\n");
}

static uint32_t
draw_frame(uint8_t *src[])
{
  int stride[] = { src_stride, 0, 0};
  return draw_slice(src,stride,src_width, src_height,0,0);
}

static uint32_t
query_format(uint32_t format)
{
  switch(format) {
  case IMGFMT_BGR8:
    if(tdfx_cfg.screen_format == TDFX_VID_FORMAT_BGR8)
      return 3 | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE;
    return 0;
  case IMGFMT_YUY2:
  case IMGFMT_UYVY:
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
  case IMGFMT_YV12:
  case IMGFMT_I420:
    if(tdfx_cfg.screen_format == TDFX_VID_FORMAT_BGR8)
      return 0;
    return 3 | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE;
  }
  return 0;
}

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{

  if(tdfx_fd < 0)
    return 1;

  if(!vo_screenwidth)
    vo_screenwidth = tdfx_cfg.screen_width;
  if(!vo_screenheight)
    vo_screenheight = tdfx_cfg.screen_height;

  aspect_save_orig(width,height);
  aspect_save_prescale(d_width,d_height);
  aspect_save_screenres(vo_screenwidth,vo_screenheight);
	
  if(fullscreen&0x01) { /* -fs */
    aspect(&d_width,&d_height,A_ZOOM);
    vo_fs = VO_TRUE;
  } else {
    aspect(&d_width,&d_height,A_NOZOOM);
    vo_fs = VO_FALSE;
  }

  src_width = width;
  src_height = height;
  buffer_size = 0;
  buffer_stride[0] = 0;
  src_fmt = 0;
  switch(format) {
  case IMGFMT_BGR8:
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
    src_bpp = ((format & 0x3F)+7)/8; 
    break;
  case IMGFMT_YV12:
  case IMGFMT_I420:
    buffer_size = src_width * src_height * 3 / 2;
    buffer_stride[0] = ((src_width+1)/2)*2;
    buffer_stride[1] = buffer_stride[2] = buffer_stride[0]/2;
    src_fmt = TDFX_VID_FORMAT_YUY2;
  case IMGFMT_YUY2:
  case IMGFMT_UYVY: 
    src_bpp = 2;
    break;
  default:
    printf("Unsupported input format 0x%x\n",format);
    return 1;
  }

  img_fmt = format;
  src_stride = src_width*src_bpp;
  if(!src_fmt)
    src_fmt = format;
  if(!buffer_size)
    buffer_size = src_stride*src_height;
  if(!buffer_stride[0])
    buffer_stride[0] = src_stride;

  dst_fmt = tdfx_cfg.screen_format;
  dst_bpp = ((dst_fmt & 0x3F)+7)/8;
  dst_width = d_width;
  dst_height = d_height;
  dst_stride = tdfx_cfg.screen_stride;

  tdfx_page =  tdfx_cfg.screen_stride*tdfx_cfg.screen_height;
  front_buffer = tdfx_cfg.screen_start;
  back_buffer = front_buffer + tdfx_page;

  printf("tdfxvid setup : %d(%d) x %d @ %d => %d(%d) x %d @ %d\n",
	 src_width,src_stride,src_height,src_bpp,
	 dst_width,dst_stride,dst_height,dst_bpp);
  
  return 0;
}

static void
uninit(void)
{
  close(tdfx_fd);
  tdfx_fd = -1;
}


static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
 
  tdfx_fd = open(arg ? arg : "/dev/tdfx_vid", O_RDWR);
  if(tdfx_fd < 0) {
    printf("tdfx_vid: Can't open %s: %s\n",arg ? arg : "/dev/tdfx_vid",
	   strerror(errno));
    return 1;
  }

  if(ioctl(tdfx_fd,TDFX_VID_GET_CONFIG,&tdfx_cfg)) {
    printf("tdfx_vid: Can't get current cfg: %s\n",strerror(errno));
    return 1;
  }

  printf("tdfx_vid version %d\n"
	 "  Ram: %d\n"
	 "  Screen: %d x %d\n"
	 "  Format: %c%c%c%d\n",
	 tdfx_cfg.version,
	 tdfx_cfg.ram_size,
	 tdfx_cfg.screen_width, tdfx_cfg.screen_height,
	 tdfx_cfg.screen_format>>24,(tdfx_cfg.screen_format>>16)&0xFF,
	 (tdfx_cfg.screen_format>>8)&0xFF,tdfx_cfg.screen_format&0xFF);

  // For now just allocate more than i ever need
  agp_mem = mmap( NULL, 1024*768*4, PROT_READ | PROT_WRITE, MAP_SHARED,
		  tdfx_fd, 0);

  if(agp_mem == MAP_FAILED) {
    printf("tdfx_vid: Memmap failed !!!!!\n");
    return 1;
  }
  
  memset(agp_mem,0,1024*768*4);

  return 0;
}

static uint32_t get_image(mp_image_t *mpi) {
  int buf = 0;

#ifdef VERBOSE
  printf("Get image %d\n",buf);
#endif

  // Currently read are too slow bcs we read from the
  // agp aperture and not the memory directly
  //if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE;

  if(mpi->flags&MP_IMGFLAG_READABLE &&
     (mpi->type==MP_IMGTYPE_IPB || mpi->type==MP_IMGTYPE_IP)){
    // reference (I/P) frame of IP or IPB:
    if(num_buffer<2) return VO_FALSE; // not enough
    current_ip_buf^=1;
    // for IPB with 2 buffers we can DR only one of the 2 P frames:
    if(mpi->type==MP_IMGTYPE_IPB && num_buffer<3 && current_ip_buf) return VO_FALSE;
    buf=current_ip_buf;
    if(mpi->type==MP_IMGTYPE_IPB) ++buf; // preserve space for B
  }

  switch(mpi->imgfmt) {
  case IMGFMT_YUY2:
  case IMGFMT_UYVY:
  case IMGFMT_BGR8:
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
    mpi->planes[0] = agp_mem + buf * buffer_size;
    mpi->stride[0] = src_stride;
    break;
  case IMGFMT_YV12:
  case IMGFMT_I420:
    mpi->planes[0] = agp_mem + buf * buffer_size;
    mpi->stride[0] = mpi->width;
    mpi->planes[1] = mpi->planes[0] + mpi->stride[0]*mpi->height;
    mpi->stride[1] = mpi->chroma_width;
    mpi->planes[2] = mpi->planes[1] + mpi->stride[1]*mpi->chroma_height;
    mpi->stride[2] = mpi->chroma_width;
    break;
  default:
    printf("Get image todo\n");
    return VO_FALSE;
  }
  mpi->flags |= MP_IMGFLAG_DIRECT;
  mpi->priv = (void*)buf;
  
  return VO_TRUE;
}

static uint32_t start_slice(mp_image_t *mpi){
  int buf = 0;

#ifdef VERBOSE
  printf("Start slices %d\n",buf);
#endif

  if(mpi->flags & MP_IMGFLAG_DIRECT)
    buf = (int)mpi->priv;
  current_buffer = buf;

  return VO_TRUE;
}

static uint32_t draw_image(mp_image_t *mpi){
  int buf = 0;
  tdfx_vid_agp_move_t mov;
  tdfx_vid_yuv_t yuv;
  int p;
  uint8_t* planes[3];
  int stride[3];

#ifdef VERBOSE
  printf("Draw image %d\n",buf);
#endif

  if(mpi->flags & MP_IMGFLAG_DIRECT)
    buf = (int)mpi->priv;

  switch(mpi->imgfmt) {
  case IMGFMT_YUY2:
  case IMGFMT_UYVY:
  case IMGFMT_BGR8:
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
    if(!(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))) {
      // copy to agp_mem
#ifdef VERBOSE
      printf("Memcpy\n");
#endif
      planes[0] = agp_mem + buf * buffer_size;
      mem2agpcpy_pic(planes[0],mpi->planes[0],src_bpp*mpi->width,mpi->height,
		     buffer_stride[0],mpi->stride[0]);
    } else
      planes[0] = agp_mem + buf * buffer_size;

    mov.move2 = TDFX_VID_MOVE_2_PACKED;
    mov.width = mpi->width*((mpi->bpp+7)/8);
    mov.height = mpi->height;
    mov.src = planes[0] - agp_mem;
    mov.src_stride = buffer_stride[0];
  
    mov.dst = back_buffer;
    mov.dst_stride = src_stride;
	 
    if(ioctl(tdfx_fd,TDFX_VID_AGP_MOVE,&mov))
      printf("tdfx_vid: AGP move failed\n");
    break;

  case IMGFMT_YV12:
  case IMGFMT_I420:
    if(!(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))) {
      // Copy to agp mem
#ifdef VERBOSE
      printf("Memcpy\n");
#endif
      planes[0] = agp_mem + buf * buffer_size;
      memcpy_pic(planes[0],mpi->planes[0],mpi->width,mpi->height,
		 buffer_stride[0],mpi->stride[0]);
      planes[1] = planes[0] + (mpi->height*mpi->stride[0]);
      memcpy_pic(planes[1],mpi->planes[1],mpi->chroma_width,mpi->chroma_height,
		 buffer_stride[1],mpi->stride[1]);
      planes[2] = planes[1] + (mpi->chroma_height*mpi->stride[1]);
      memcpy_pic(planes[2],mpi->planes[2],mpi->chroma_width,mpi->chroma_height,
		 buffer_stride[2],mpi->stride[2]);
    } else {
      planes[0] = agp_mem + buf * buffer_size;
      planes[1] = planes[0] + buffer_stride[0] * src_height;
      planes[2] = planes[1] + buffer_stride[1] * src_height/2;
    }

    // Setup the yuv thing
    yuv.base = back_buffer;
    yuv.stride = src_stride;
    if(ioctl(tdfx_fd,TDFX_VID_SET_YUV,&yuv)) {
      printf("tdfx_vid: Set yuv failed\n");
      break;
    }
    

    // Now agp move that
    // Y
    mov.move2 = TDFX_VID_MOVE_2_YUV;
    mov.width = mpi->width;
    mov.height = mpi->height;
    mov.src = planes[0] - agp_mem;
    mov.src_stride =  buffer_stride[0];
    mov.dst = 0x0;
    mov.dst_stride = TDFX_VID_YUV_STRIDE;

    if(ioctl(tdfx_fd,TDFX_VID_AGP_MOVE,&mov)) {
      printf("tdfx_vid: AGP move failed on Y plane\n");
      break;
    }
    //return 0;
    // U
    p = mpi->imgfmt == IMGFMT_YV12 ? 1 : 2;
    mov.width = mpi->chroma_width;
    mov.height = mpi->chroma_height;
    mov.src = planes[p] - agp_mem;
    mov.src_stride = buffer_stride[p];
    mov.dst += TDFX_VID_YUV_PLANE_SIZE;
    if(ioctl(tdfx_fd,TDFX_VID_AGP_MOVE,&mov)) {
      printf("tdfx_vid: AGP move failed on U plane\n");
      break;
    }
    // V
    p = mpi->imgfmt == IMGFMT_YV12 ? 2 : 1;
    mov.src = planes[p] - agp_mem;
    mov.src_stride = buffer_stride[p];
    mov.dst += TDFX_VID_YUV_PLANE_SIZE;
    if(ioctl(tdfx_fd,TDFX_VID_AGP_MOVE,&mov)) {
      printf("tdfx_vid: AGP move failed on U plane\n");
      break;
    }
    break;
  default:
    printf("What's that for a format 0x%x\n",mpi->imgfmt);
    return VO_TRUE;
  }

  return VO_TRUE;
}

static uint32_t fullscreen(void) {
  vo_fs ^= 1;
  aspect(&dst_width,&dst_height,vo_fs ? A_ZOOM : A_NOZOOM);
  // This does not work :((
  //clear_screen();
  return VO_TRUE;
}


static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_DRAW_IMAGE:
    return draw_image(data);
  case VOCTRL_START_SLICE:
    return start_slice(data);
  case VOCTRL_FULLSCREEN:
    return fullscreen();
  }
  return VO_NOTIMPL;
}

