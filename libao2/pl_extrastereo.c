/* Extrastereo effect plugin
 * (linearly increases difference between L&R channels)
 * 
 * Current limitations:
 *  - only AFMT_S16_LE is supported currently
 *
 * License: GPLv2 (as a mix of pl_volume.c and 
 *          xmms:stereo_plugin/stereo.c)
 * 
 * Author: pl <p_l@gmx.fr> (c) 2002 and beyond...
 * */

#define PLUGIN

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

static ao_info_t info = {
        "Extra stereo plugin",
        "extrastereo",
        "pl <p_l@gmx.fr>",
        ""
};

LIBAO_PLUGIN_EXTERN(extrastereo)

// local data
static struct {
  float    mul;         // intensity
  int      inuse;      // This plugin is in use TRUE, FALSE
  int      format;     // sample format
} pl_extrastereo = {2.5, 0, 0};


// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    return CONTROL_OK;
  case AOCONTROL_PLUGIN_ES_SET:
    pl_extrastereo.mul=*((float*)arg);
    return CONTROL_OK;
  case AOCONTROL_PLUGIN_ES_GET:
    *((float*)arg)=pl_extrastereo.mul;
    return CONTROL_OK;
  }
  return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  switch(ao_plugin_data.format){
  case(AFMT_S16_LE):
    break;
  default:
    fprintf(stderr,"[pl_extrastereo] Audio format not yet suported \n");
    return 0;
  }

  pl_extrastereo.mul=ao_plugin_cfg.pl_extrastereo_mul;
  pl_extrastereo.format=ao_plugin_data.format;
  pl_extrastereo.inuse=1;

  printf("[pl_extrastereo] Extra stereo plugin in use (multiplier=%2.2f).\n",
           pl_extrastereo.mul);
  return 1;
}

// close plugin
static void uninit(){
  pl_extrastereo.inuse=0;
}

// empty buffers
static void reset(){
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){

  switch(pl_extrastereo.format){
  case(AFMT_S16_LE): {

    int16_t* data=(int16_t*)ao_plugin_data.data;
    int len=ao_plugin_data.len / 2; // 16 bits samples

    float mul = pl_extrastereo.mul;
    int32_t i, avg, ltmp, rtmp;

    for (i=0; i < len ; i += 2) {

      avg = (data[i] + data[i + 1]) / 2;

      ltmp = avg + (int) (mul * (data[i] - avg));
      rtmp = avg + (int) (mul * (data[i + 1] - avg));

      if (ltmp < -32768) {
        ltmp = -32768;
      } else if (ltmp > 32767) {
        ltmp = 32767;
      }

      if (rtmp < -32768) {
        rtmp = -32768;
      } else if (rtmp > 32767) {
        rtmp = 32767;
      }

      data[i] = ltmp;
      data[i + 1] = rtmp;
    }
    break;
  }
  default:
    return 0;
  }
  return 1;
}

