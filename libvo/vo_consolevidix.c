/*
    VIDIX accelerated overlay on black background
    should work on any OS
    TODO: implement blanking, aspect, geometry,fs etc.
    
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

#include "mp_msg.h"

#include "vosub_vidix.h"
#include "../vidix/vidixlib.h"


static vo_info_t info = {
    "VIDIX",
    "consolevidix",
    "Sascha Sommer",
    ""
};

LIBVO_EXTERN(consolevidix)

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

/* VIDIX related */
static char *vidix_name;


static vidix_grkey_t gr_key;
    
static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,uint32_t d_height, uint32_t flags, char *title, uint32_t format){
  if(vidix_init(width, height, 0, 0, d_width, d_height, format, 32, 640, 480))mp_msg(MSGT_VO, MSGL_FATAL, "Can't initialize VIDIX driver: %s\n", strerror(errno));
  /*set colorkey*/       
  vidix_start();
  if(vidix_grkey_support()){
    vidix_grkey_get(&gr_key);
    gr_key.key_op = KEYS_PUT;
    gr_key.ckey.op = CKEY_TRUE;
    gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0;
    vidix_grkey_set(&gr_key);
  }         
  return 0;
}

static void check_events(void){
}

/* draw_osd, flip_page, draw_slice, draw_frame should be
   overwritten with vidix functions (vosub_vidix.c) */
static void draw_osd(void){
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_consolevidix: error: didn't use vidix draw_osd!\n");
  return;
}

static void flip_page(void){
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_consolevidix: error: didn't use vidix flip_page!\n");
  return;
}

static uint32_t draw_slice(uint8_t *src[], int stride[],int w, int h, int x, int y){
  UNUSED(src);
  UNUSED(stride);
  UNUSED(w);
  UNUSED(h);
  UNUSED(x);
  UNUSED(y);
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_consolevidix: error: didn't use vidix draw_slice!\n");
  return -1;
}

static uint32_t draw_frame(uint8_t *src[]){
  UNUSED(src);
  mp_msg(MSGT_VO, MSGL_FATAL, "vo_consolevidix: error: didn't use vidix draw_frame!\n");
  return -1;
}

static uint32_t query_format(uint32_t format){
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

static uint32_t preinit(const char *arg){
  if(arg)vidix_name = strdup(arg);
  else {
    mp_msg(MSGT_VO, MSGL_INFO, "vo_consolevidix: No vidix driver name provided, probing available ones!\n");
	vidix_name = NULL;
  }
  if(vidix_preinit(vidix_name, &video_out_consolevidix))return 1;
  return 0;
}

static uint32_t control(uint32_t request, void *data, ...){
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return vidix_control(request, data);
}
