/* This audio output plugin changes the volume of the sound, and can
   be used when the mixer doesn't support the PCM channel. The volume
   is set in fixed steps between 0 - 2^8. */

#define PLUGIN

// Some limits
#define MIN_S16 -32650
#define MAX_S16  32650
#define MIN_U8   0
#define MAX_U8   255
#define MIN_S8  -128
#define MAX_S8   127

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

static ao_info_t info =
{
        "Volume control audio plugin",
        "volume",
        "Anders",
        ""
};

LIBAO_PLUGIN_EXTERN(volume)

// local data
typedef struct pl_volume_s
{
  uint16_t volume;   	// output volume level
  int      inuse;     	// This plugin is in use TRUE, FALSE
  int      format;	// sample fomat
} pl_volume_t;

static pl_volume_t pl_volume={0,0,0};

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    return CONTROL_OK;
  case AOCONTROL_GET_VOLUME:{
    if(pl_volume.inuse){
      ((ao_control_vol_t *)arg)->right=((float)pl_volume.volume)/2.55;
      ((ao_control_vol_t *)arg)->left=((float)pl_volume.volume)/2.55;
      return CONTROL_OK;
    }
    else
      return CONTROL_ERROR;
  }
  case AOCONTROL_SET_VOLUME:{
    if(pl_volume.inuse){
      // Calculate avarage between left and right
      float vol =2.55*((((ao_control_vol_t *)arg)->right)+(((ao_control_vol_t *)arg)->left))/2;
      pl_volume.volume=(uint16_t)vol;
      // Volume must be between 0 and 255
      if(vol > 255)
	pl_volume.volume = 0xFF;
      if(vol < 0)
	pl_volume.volume = 0;
      return CONTROL_OK;
    }
    else
      return CONTROL_ERROR;
  }
  }
  return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  // Sanity sheck this plugin supports AFMT_U8 and AFMT_S16_LE
  switch(ao_plugin_data.format){
  case(AFMT_U8):
  case(AFMT_S16_LE):
    break;
  default: 
    fprintf(stderr,"[pl_volume] Audio format not yet suported \n");
    return 0;
  }
  // Initialize volume to this value
  pl_volume.volume=ao_plugin_cfg.pl_volume_volume;
  pl_volume.format=ao_plugin_data.format;
  /* The inuse flag is used in control to detremine if the return
  value since that function always is called from ao_plugin regardless
  of wether this plugin is in use or not. */
  pl_volume.inuse=1;
  // Tell the world what we are up to
  printf("[pl_volume] Software volume control in use%s.\n",ao_plugin_cfg.pl_volume_softclip?", soft clipping enabled":"");
  return 1;
}

// close plugin
static void uninit(){
  pl_volume.inuse=0;
}

// empty buffers
static void reset(){
}

#define SIGN(x) (x>0?1:-1) 
// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){
  register int i=0;
  register int vol=pl_volume.volume; // Logarithmic control sounds more natural
  vol=(vol*vol*vol)>>12;
  // Change the volume.
  switch(pl_volume.format){
  case(AFMT_U8):{
    register uint8_t* data=(uint8_t*)ao_plugin_data.data;
    register int x;
      for(i=0;i<ao_plugin_data.len;i++){
        x=((data[i]-128) * vol) >> 8; 
        if(x>MAX_S8)
	  data[i]=MAX_U8;
	  else if(x<MIN_S8)
	  data[i]=MIN_U8;
        else{
	  if(ao_plugin_cfg.pl_volume_softclip)
	    data[i] = ((3*x - ((x*x*x) >> 14)) >> 1) + 128;
	  else
	    data[i] = x + 128;
	}
      }
      break;
  }
  case(AFMT_S16_LE):{
    register int len=ao_plugin_data.len>>1;
    register int16_t* data=(int16_t*)ao_plugin_data.data;
    register int x;
    for(i=0;i<len;i++){
      x=(data[i] * vol) >> 8;
      if(x>MAX_S16)
	data[i]=MAX_S16;
      else if(x<MIN_S16)
	data[i]=MIN_S16;
      else{
	if(ao_plugin_cfg.pl_volume_softclip){
	  int64_t t=x*x;
	  t=(t*x) >> 30;
	  data[i] = (3*x - (int)t) >> 1;
	  //data[i] = 2*x - SIGN(x)*((x*x)>>15);
	}
	else
	  data[i] = x;
      }
    }
    break;
  }
  default: 
    return 0;
  }
  return 1;

}
