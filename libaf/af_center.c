/*
 * This filter adds a center channel to the audio stream by
 * averaging the left and right channel.
 * There are two runtime controls one for setting which channel
 * to insert the center-audio into called AF_CONTROL_SUB_CH.
 *
 * FIXME: implement a high-pass filter for better results.
 *
 * copyright (c) 2005 Alex Beregszaszi
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"

// Data for specific instances of this filter
typedef struct af_center_s
{
  int ch;		// Channel number which to insert the filtered data
}af_center_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_center_t* s   = af->setup;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = max(s->ch+1,((af_data_t*)arg)->nch);
    af->data->format = AF_FORMAT_FLOAT_NE;
    af->data->bps    = 4;

    return af_test_output(af,(af_data_t*)arg);
  }
  case AF_CONTROL_COMMAND_LINE:{
    int   ch=1;
    sscanf(arg,"%i", &ch);
    return control(af,AF_CONTROL_CENTER_CH | AF_CONTROL_SET, &ch);
  }
  case AF_CONTROL_CENTER_CH | AF_CONTROL_SET: // Requires reinit
    // Sanity check
    if((*(int*)arg >= AF_NCH) || (*(int*)arg < 0)){
      mp_msg(MSGT_AFILTER, MSGL_ERR, "[sub] Center channel number must be between "
	     " 0 and %i current value is %i\n", AF_NCH-1, *(int*)arg);
      return AF_ERROR;
    }
    s->ch = *(int*)arg;
    return AF_OK;
  case AF_CONTROL_CENTER_CH | AF_CONTROL_GET:
    *(int*)arg = s->ch;
    return AF_OK;
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
  af_data_t*    c   = data;	 // Current working data
  af_center_t*  s   = af->setup; // Setup for this instance
  float*   	a   = c->audio;	 // Audio data
  int		len = c->len/4;	 // Number of samples in current audio block
  int		nch = c->nch;	 // Number of channels
  int		ch  = s->ch;	 // Channel in which to insert the center audio
  register int  i;

  // Run filter
  for(i=0;i<len;i+=nch){
    // Average left and right
    a[i+ch] = (a[i]/2) + (a[i+1]/2);
  }

  return c;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  af_center_t* s;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=s=calloc(1,sizeof(af_center_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  // Set default values
  s->ch = 1;  	 // Channel nr 2
  return AF_OK;
}

// Description of this filter
af_info_t af_info_center = {
    "Audio filter for adding a center channel",
    "center",
    "Alex Beregszaszi",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
