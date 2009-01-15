/*
 * Copyright (C)2002 Anders Johansson ajh@atri.curtin.edu.au
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

/* This audio filter changes the volume of the sound, and can be used
   when the mixer doesn't support the PCM channel. It can handle
   between 1 and 6 channels. The volume can be adjusted between -60dB
   to +20dB and is set on a per channels basis. The is accessed through
   AF_CONTROL_VOLUME_LEVEL.

   The filter has support for soft-clipping, it is enabled by
   AF_CONTROL_VOLUME_SOFTCLIPP. It has also a probing feature which
   can be used to measure the power in the audio stream, both an
   instantaneous value and the maximum value can be probed. The
   probing is enable by AF_CONTROL_VOLUME_PROBE_ON_OFF and is done on a
   per channel basis. The result from the probing is obtained using
   AF_CONTROL_VOLUME_PROBE_GET and AF_CONTROL_VOLUME_PROBE_GET_MAX. The
   probed values are calculated in dB. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "af.h"

// Data for specific instances of this filter
typedef struct af_volume_s
{
  int   enable[AF_NCH];		// Enable/disable / channel
  float	pow[AF_NCH];		// Estimated power level [dB]
  float	max[AF_NCH];		// Max Power level [dB]
  float level[AF_NCH];		// Gain level for each channel
  float time;			// Forgetting factor for power estimate
  int soft;			// Enable/disable soft clipping
  int fast;			// Use fix-point volume control
}af_volume_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_volume_t* s   = (af_volume_t*)af->setup; 

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return AF_ERROR;
    
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    
    if(s->fast && (((af_data_t*)arg)->format != (AF_FORMAT_FLOAT_NE))){
      af->data->format = AF_FORMAT_S16_NE;
      af->data->bps    = 2;
    }
    else{
      // Cutoff set to 10Hz for forgetting factor
      float x = 2.0*M_PI*15.0/(float)af->data->rate;
      float t = 2.0-cos(x);
      s->time = 1.0 - (t - sqrt(t*t - 1));
      af_msg(AF_MSG_DEBUG0,"[volume] Forgetting factor = %0.5f\n",s->time);
      af->data->format = AF_FORMAT_FLOAT_NE;
      af->data->bps    = 4;
    }
    return af_test_output(af,(af_data_t*)arg);
  case AF_CONTROL_COMMAND_LINE:{
    float v=0.0;
    float vol[AF_NCH];
    int   i;
    sscanf((char*)arg,"%f:%i", &v, &s->soft);
    for(i=0;i<AF_NCH;i++) vol[i]=v;
    return control(af,AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, vol);
  }
  case AF_CONTROL_POST_CREATE:	
    s->fast = ((((af_cfg_t*)arg)->force & AF_INIT_FORMAT_MASK) == 
      AF_INIT_FLOAT) ? 0 : 1;
    return AF_OK;
  case AF_CONTROL_VOLUME_ON_OFF | AF_CONTROL_SET:
    memcpy(s->enable,(int*)arg,AF_NCH*sizeof(int));
    return AF_OK; 
  case AF_CONTROL_VOLUME_ON_OFF | AF_CONTROL_GET:
    memcpy((int*)arg,s->enable,AF_NCH*sizeof(int));
    return AF_OK; 
  case AF_CONTROL_VOLUME_SOFTCLIP | AF_CONTROL_SET:
    s->soft = *(int*)arg;
    return AF_OK; 
  case AF_CONTROL_VOLUME_SOFTCLIP | AF_CONTROL_GET:
    *(int*)arg = s->soft;
    return AF_OK; 
  case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET:
    return af_from_dB(AF_NCH,(float*)arg,s->level,20.0,-200.0,60.0);
  case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->level,(float*)arg,20.0);
  case AF_CONTROL_VOLUME_PROBE | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->pow,(float*)arg,10.0);
  case AF_CONTROL_VOLUME_PROBE_MAX | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->max,(float*)arg,10.0);
  case AF_CONTROL_PRE_DESTROY:{
    float m = 0.0;
    int i;
    if(!s->fast){
      for(i=0;i<AF_NCH;i++)
	m=max(m,s->max[i]);
	af_to_dB(1, &m, &m, 10.0);
	af_msg(AF_MSG_INFO,"[volume] The maximum volume was %0.2fdB \n", m);
    }
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data);
  if(af->setup)
    free(af->setup);
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*    c   = data;			// Current working data
  af_volume_t*  s   = (af_volume_t*)af->setup; 	// Setup for this instance
  int           ch  = 0;			// Channel counter
  register int	nch = c->nch;			// Number of channels	
  register int  i   = 0;

  // Basic operation volume control only (used on slow machines)
  if(af->data->format == (AF_FORMAT_S16_NE)){
    int16_t*    a   = (int16_t*)c->audio;	// Audio data
    int         len = c->len/2;			// Number of samples
    for(ch = 0; ch < nch ; ch++){
      if(s->enable[ch]){
	register int vol = (int)(255.0 * s->level[ch]); 
	for(i=ch;i<len;i+=nch){
	  register int x = (a[i] * vol) >> 8;
	  a[i]=clamp(x,SHRT_MIN,SHRT_MAX);
	}
      }
    }
  }
  // Machine is fast and data is floating point
  else if(af->data->format == (AF_FORMAT_FLOAT_NE)){ 
    float*   	a   	= (float*)c->audio;	// Audio data
    int       	len 	= c->len/4;		// Number of samples
    for(ch = 0; ch < nch ; ch++){
      // Volume control (fader)
      if(s->enable[ch]){
	float	t   = 1.0 - s->time;
	for(i=ch;i<len;i+=nch){
	  register float x 	= a[i];
	  register float pow 	= x*x;	
	  // Check maximum power value
	  if(pow > s->max[ch])
	    s->max[ch] = pow;
	  // Set volume
	  x *= s->level[ch];
	  // Peak meter
	  pow 	= x*x;
	  if(pow > s->pow[ch])
	    s->pow[ch] = pow;
	  else
	    s->pow[ch] = t*s->pow[ch] + pow*s->time; // LP filter
	  /* Soft clipping, the sound of a dream, thanks to Jon Wattes
	     post to Musicdsp.org */
	  if(s->soft)
	    x=af_softclip(x);
	  // Hard clipping
	  else
	    x=clamp(x,-1.0,1.0);
	  a[i] = x;
	}
      }
    }
  }
  return c;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  int i = 0;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_volume_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  // Enable volume control and set initial volume to 0dB.
  for(i=0;i<AF_NCH;i++){
    ((af_volume_t*)af->setup)->enable[i] = 1;
    ((af_volume_t*)af->setup)->level[i]  = 1.0;
  }
  return AF_OK;
}

// Description of this filter
af_info_t af_info_volume = {
    "Volume control audio filter",
    "volume",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
