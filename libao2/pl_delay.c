/* This is a null audio out plugin it doesnt't really do anything
   useful but serves an example of how audio plugins work. It delays
   the output signal by the nuber of samples set by aop_delay n
   where n is the number of bytes.
 */
#define PLUGIN

#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

static ao_info_t info =
{
        "Delay audio plugin",
        "delay",
        "Anders",
        ""
};

LIBAO_PLUGIN_EXTERN(delay)

// local data
typedef struct pl_delay_s
{
  void* data;       // local audio data block
  void* delay; 	    // data block used for delaying audio signal
  int len;          // local buffer length
  int rate;         // local data rate
  int channels;     // local number of channels
  int format;       // local format

} pl_delay_t;

static pl_delay_t pl_delay={NULL,NULL,0,0,0,0};

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    if(pl_delay.data) 
      uninit();
    pl_delay.len = ao_plugin_data.len;
    pl_delay.data=(void*)malloc(ao_plugin_data.len);
    return CONTROL_OK;
  }
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  int i=0;
  float time_delay; // The number of tsamples this plugin delays the output data
  /* if the output format of any of the below parameters differs from
     what is give it should be changed. See ao_plugin init() */
  pl_delay.rate=ao_plugin_data.rate;
  pl_delay.channels=ao_plugin_data.channels+1; //0=mono 1=stereo
  pl_delay.format=ao_plugin_data.format;

  // Tell ao_plugin how much this plugin adds to the overall time delay
  time_delay=-1*(float)ao_plugin_cfg.pl_delay_len/((float)pl_delay.channels*(float)pl_delay.rate);
  if(pl_delay.format != AFMT_U8 && pl_delay.format != AFMT_S8)
    time_delay/=2;
  ao_plugin_data.delay_fix+=time_delay;

  pl_delay.delay=(void*)malloc(ao_plugin_cfg.pl_delay_len);
  if(!pl_delay.delay)
    return 0;
  for(i=0;i<ao_plugin_cfg.pl_delay_len;i++)
    ((char*)pl_delay.delay)[i]=0;
  printf("[pl_delay] Output sound delayed by %i bytes\n",ao_plugin_cfg.pl_delay_len);
  return 1;
}

// close plugin
static void uninit(){
  if(pl_delay.delay) 
    free(pl_delay.delay);
  if(pl_delay.data) 
    free(pl_delay.data);
  ao_plugin_cfg.pl_delay_len=0;
}

// empty buffers
static void reset(){
  int i = 0;
  for(i=0;i<pl_delay.len;i++)
    ((char*)pl_delay.data)[i]=0;
  for(i=0;i<ao_plugin_cfg.pl_delay_len;i++)
    ((char*)pl_delay.delay)[i]=0;
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){
  int i=0;
  int j=0;
  int k=0;
  // Copy end of prev block to begining of buffer
  for(i=0;i<ao_plugin_cfg.pl_delay_len;i++,j++)
    ((char*)pl_delay.data)[j]=((char*)pl_delay.delay)[i];
  // Copy current block except end
  for(i=0;i<ao_plugin_data.len-ao_plugin_cfg.pl_delay_len;i++,j++,k++)
    ((char*)pl_delay.data)[j]=((char*)ao_plugin_data.data)[k];
  // Save away end of current block for next call
  for(i=0;i<ao_plugin_cfg.pl_delay_len;i++,k++)
    ((char*)pl_delay.delay)[i]=((char*)ao_plugin_data.data)[k];
  // Set output data block
  ao_plugin_data.data=pl_delay.data;
  return 1;
}



