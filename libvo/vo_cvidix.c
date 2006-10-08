/*
    VIDIX accelerated overlay on (black) background
    
    should work on any OS
    
    (C) Sascha Sommer
    
 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "geometry.h"

#include "mp_msg.h"

#include "vosub_vidix.h"
#include "vidix/vidixlib.h"


static vo_info_t info = {
    "console VIDIX",
    "cvidix",
    "Sascha Sommer",
    ""
};

LIBVO_EXTERN(cvidix)

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

/* VIDIX related */
static char *vidix_name;
static uint32_t swidth,sheight,sformat;
/// center video only when screenw & height are set
static uint32_t center=0;
static vidix_grkey_t gr_key;


static uint32_t setup_vidix(void){
  int x=vo_dx,y=vo_dy;
  aspect(&vo_dwidth,&vo_dheight,vo_fs ? A_ZOOM : A_NOZOOM);  
  if(vo_fs || center){
    if(vo_dwidth <= vo_screenwidth)x = (vo_screenwidth - vo_dwidth)/2;
    else x=0;
    if(vo_dheight <= vo_screenheight)y = (vo_screenheight - vo_dheight)/2;
    else y=0;
  }
  if(vo_config_count)vidix_stop();
  if(vidix_init(swidth, sheight, x, y, vo_dwidth, vo_dheight, sformat, 32, vo_screenwidth,vo_screenheight)){
    mp_msg(MSGT_VO, MSGL_FATAL, "Can't setup VIDIX driver: %s\n", strerror(errno));
    return 1;
  }
  vidix_start();
  if(vidix_grkey_support()){
    vidix_grkey_get(&gr_key);
    gr_key.key_op = KEYS_PUT;
    if (!vo_fs && !(vo_colorkey & 0xff000000)){
	  gr_key.ckey.op = CKEY_TRUE;
	  gr_key.ckey.red = (vo_colorkey & 0x00FF0000) >> 16;
	  gr_key.ckey.green = (vo_colorkey & 0x0000FF00) >> 8;
	  gr_key.ckey.blue = vo_colorkey & 0x000000FF;
    }
    else gr_key.ckey.op = CKEY_FALSE;
    vidix_grkey_set(&gr_key);
  }
  return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,uint32_t d_height, uint32_t flags, char *title, uint32_t format){
  vo_fs = flags & VOFLAG_FULLSCREEN;
  if(!vo_config_count){
    if(vo_screenwidth && vo_screenheight){
      if(!vo_geometry)center=1;
    }
    else mp_msg(MSGT_VO, MSGL_WARN, "vo_cvidix: warn: screenwidth and height not set assuming 640x480\n");
  }
  if(!vo_screenwidth)vo_screenwidth=640;
  if(!vo_screenheight)vo_screenheight=480;
  swidth = width;
  sheight = height;
  sformat = format;
  vo_dwidth=d_width;
  vo_dheight=d_height;
  aspect_save_orig(width,height);
  aspect_save_prescale(d_width,d_height);
  aspect_save_screenres(vo_screenwidth,vo_screenheight);
  if(!vo_geometry){
    vo_dx=0;
    vo_dy=0;
  }
  else geometry(&vo_dx, &vo_dy, &vo_dwidth, &vo_dheight,vo_screenwidth,vo_screenheight);
  return setup_vidix();
}

static void check_events(void){
}

/* draw_osd, flip_page, draw_slice, draw_frame should be
   overwritten with vidix functions (vosub_vidix.c) */
static void draw_osd(void){
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_cvidix: error: didn't use vidix draw_osd!\n");
  return;
}

static void flip_page(void){
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_cvidix: error: didn't use vidix flip_page!\n");
  return;
}

static int draw_slice(uint8_t *src[], int stride[],int w, int h, int x, int y){
  UNUSED(src);
  UNUSED(stride);
  UNUSED(w);
  UNUSED(h);
  UNUSED(x);
  UNUSED(y);
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_cvidix: error: didn't use vidix draw_slice!\n");
  return -1;
}

static int draw_frame(uint8_t *src[]){
  UNUSED(src);
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_cvidix: error: didn't use vidix draw_frame!\n");
  return -1;
}

static int query_format(uint32_t format){
  return(vidix_query_fourcc(format));
}

static void uninit(void){
  if(!vo_config_count) return;
  vidix_term();
  if(vidix_name){
    free(vidix_name);
	vidix_name = NULL;
  }
}

static int preinit(const char *arg){
  if(arg)vidix_name = strdup(arg);
  else {
    mp_msg(MSGT_VO, MSGL_INFO, "vo_cvidix: No vidix driver name provided, probing available ones (-v option for details)!\n");
	vidix_name = NULL;
  }
  if(vidix_preinit(vidix_name, &video_out_cvidix))return 1;
  return 0;
}

static int control(uint32_t request, void *data, ...){
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_FULLSCREEN:
    if(vo_fs)vo_fs=0;
    else vo_fs=1;
    setup_vidix();
    return VO_TRUE;      
  case VOCTRL_SET_EQUALIZER:
    {
      va_list ap;
      int value;
      va_start(ap, data);
      value = va_arg(ap, int);
      va_end(ap);
      return vidix_control(request, data, value);
    }
  case VOCTRL_GET_EQUALIZER:
    {
      va_list ap;
      int *value;
      va_start(ap, data);
      value = va_arg(ap, int *);
      va_end(ap);
      return vidix_control(request, data, value);
    }
  }  
  return vidix_control(request, data);
}
