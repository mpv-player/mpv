/*
 * Copyright (C) 2002 Anders Johansson ajh@watri.uwa.edu.au
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

/* This filter adds a sub-woofer channels to the audio stream by
   averaging the left and right channel and low-pass filter them. The
   low-pass filter is implemented as a 4th order IIR Butterworth
   filter, with a variable cutoff frequency between 10 and 300 Hz. The
   filter gives 24dB/octave attenuation. There are two runtime
   controls one for setting which channel to insert the sub-audio into
   called AF_CONTROL_SUB_CH and one for setting the cutoff frequency
   called AF_CONTROL_SUB_FC.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include "af.h"
#include "dsp.h"

// Q value for low-pass filter
#define Q 1.0

// Analog domain biquad section 
typedef struct{
  float a[3];		// Numerator coefficients
  float b[3];		// Denominator coefficients
} biquad_t;

// S-parameters for designing 4th order Butterworth filter
static biquad_t sp[2] = {{{1.0,0.0,0.0},{1.0,0.765367,1.0}},
			 {{1.0,0.0,0.0},{1.0,1.847759,1.0}}};

// Data for specific instances of this filter
typedef struct af_sub_s
{
  float w[2][4];	// Filter taps for low-pass filter
  float q[2][2];	// Circular queues
  float	fc;		// Cutoff frequency [Hz] for low-pass filter
  float k;		// Filter gain;
  int ch;		// Channel number which to insert the filtered data
  
}af_sub_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_sub_t* s   = af->setup; 

  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = max(s->ch+1,((af_data_t*)arg)->nch);
    af->data->format = AF_FORMAT_FLOAT_NE;
    af->data->bps    = 4;

    // Design low-pass filter
    s->k = 1.0;
    if((-1 == af_filter_szxform(sp[0].a, sp[0].b, Q, s->fc,
       (float)af->data->rate, &s->k, s->w[0])) ||
       (-1 == af_filter_szxform(sp[1].a, sp[1].b, Q, s->fc,
       (float)af->data->rate, &s->k, s->w[1])))
      return AF_ERROR;
    return af_test_output(af,(af_data_t*)arg);
  }
  case AF_CONTROL_COMMAND_LINE:{
    int   ch=5;
    float fc=60.0;
    sscanf(arg,"%f:%i", &fc , &ch);
    if(AF_OK != control(af,AF_CONTROL_SUB_CH | AF_CONTROL_SET, &ch))
      return AF_ERROR;
    return control(af,AF_CONTROL_SUB_FC | AF_CONTROL_SET, &fc);
  }
  case AF_CONTROL_SUB_CH | AF_CONTROL_SET: // Requires reinit
    // Sanity check
    if((*(int*)arg >= AF_NCH) || (*(int*)arg < 0)){
      af_msg(AF_MSG_ERROR,"[sub] Subwoofer channel number must be between "
	     " 0 and %i current value is %i\n", AF_NCH-1, *(int*)arg);
      return AF_ERROR;
    }
    s->ch = *(int*)arg;
    return AF_OK;
  case AF_CONTROL_SUB_CH | AF_CONTROL_GET:
    *(int*)arg = s->ch;
    return AF_OK;
  case AF_CONTROL_SUB_FC | AF_CONTROL_SET: // Requires reinit
    // Sanity check
    if((*(float*)arg > 300) || (*(float*)arg < 20)){
      af_msg(AF_MSG_ERROR,"[sub] Cutoff frequency must be between 20Hz and"
	     " 300Hz current value is %0.2f",*(float*)arg);
      return AF_ERROR;
    }
    // Set cutoff frequency
    s->fc = *(float*)arg;
    return AF_OK;
  case AF_CONTROL_SUB_FC | AF_CONTROL_GET:
    *(float*)arg = s->fc;
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

#ifndef IIR
#define IIR(in,w,q,out) { \
  float h0 = (q)[0]; \
  float h1 = (q)[1]; \
  float hn = (in) - h0 * (w)[0] - h1 * (w)[1];  \
  out = hn + h0 * (w)[2] + h1 * (w)[3];	 \
  (q)[1] = h0; \
  (q)[0] = hn; \
}
#endif

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*    c   = data;	 // Current working data
  af_sub_t*  	s   = af->setup; // Setup for this instance
  float*   	a   = c->audio;	 // Audio data
  int		len = c->len/4;	 // Number of samples in current audio block 
  int		nch = c->nch;	 // Number of channels
  int		ch  = s->ch;	 // Channel in which to insert the sub audio
  register int  i;

  // Run filter
  for(i=0;i<len;i+=nch){
    // Average left and right
    register float x = 0.5 * (a[i] + a[i+1]);
    IIR(x * s->k, s->w[0], s->q[0], x);
    IIR(x , s->w[1], s->q[1], a[i+ch]);
  }

  return c;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  af_sub_t* s;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=s=calloc(1,sizeof(af_sub_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  // Set default values
  s->ch = 5;  	 // Channel nr 6
  s->fc = 60; 	 // Cutoff frequency 60Hz
  return AF_OK;
}

// Description of this filter
af_info_t af_info_sub = {
    "Audio filter for adding a sub-base channel",
    "sub",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
