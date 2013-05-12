/*
 * This audio filter delays the output signal for the different
 * channels and can be used for simple position panning.
 * An extension for this filter would be a reverb.
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
#include <inttypes.h>

#include "af.h"

#define L 65536

#define UPDATEQI(qi) qi=(qi+1)&(L-1)

// Data for specific instances of this filter
typedef struct af_delay_s
{
  void* q[AF_NCH];   	// Circular queues used for delaying audio signal
  int 	wi[AF_NCH];  	// Write index
  int 	ri;		// Read index
  float	d[AF_NCH];   	// Delay [ms]
}af_delay_t;

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_delay_t* s = af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:{
    int i;

    // Free prevous delay queues
    for(i=0;i<af->data->nch;i++)
      free(s->q[i]);

    mp_audio_copy_config(af->data, (struct mp_audio*)arg);

    // Allocate new delay queues
    for(i=0;i<af->data->nch;i++){
      s->q[i] = calloc(L,af->data->bps);
      if(NULL == s->q[i])
	mp_msg(MSGT_AFILTER, MSGL_FATAL, "[delay] Out of memory\n");
    }

    return control(af,AF_CONTROL_DELAY_LEN | AF_CONTROL_SET,s->d);
  }
  case AF_CONTROL_COMMAND_LINE:{
    int n = 1;
    int i = 0;
    char* cl = arg;
    while(n && i < AF_NCH ){
      sscanf(cl,"%f:%n",&s->d[i],&n);
      if(n==0 || cl[n-1] == '\0')
	break;
      cl=&cl[n];
      i++;
    }
    return AF_OK;
  }
  case AF_CONTROL_DELAY_LEN | AF_CONTROL_SET:{
    int i;
    if(AF_OK != af_from_ms(AF_NCH, arg, s->wi, af->data->rate, 0.0, 1000.0))
      return AF_ERROR;
    s->ri = 0;
    for(i=0;i<AF_NCH;i++){
      mp_msg(MSGT_AFILTER, MSGL_DBG2, "[delay] Channel %i delayed by %0.3fms\n",
	     i,clamp(s->d[i],0.0,1000.0));
      mp_msg(MSGT_AFILTER, MSGL_DBG3, "[delay] Channel %i delayed by %i samples\n",
	     i,s->wi[i]);
    }
    return AF_OK;
  }
  case AF_CONTROL_DELAY_LEN | AF_CONTROL_GET:{
    int i;
    for(i=0;i<AF_NCH;i++){
      if(s->ri > s->wi[i])
	s->wi[i] = L - (s->ri - s->wi[i]);
      else
	s->wi[i] = s->wi[i] - s->ri;
    }
    return af_to_ms(AF_NCH, s->wi, arg, af->data->rate);
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance* af)
{
  int i;

  free(af->data);
  for(i=0;i<AF_NCH;i++)
      free(((af_delay_t*)(af->setup))->q[i]);
  free(af->setup);
}

// Filter data through filter
static struct mp_audio* play(struct af_instance* af, struct mp_audio* data)
{
  struct mp_audio*   	c   = data;	 // Current working data
  af_delay_t*  	s   = af->setup; // Setup for this instance
  int 		nch = c->nch;	 // Number of channels
  int		len = c->len/c->bps; // Number of sample in data chunk
  int		ri  = 0;
  int 		ch,i;
  for(ch=0;ch<nch;ch++){
    switch(c->bps){
    case 1:{
      int8_t* a = c->audio;
      int8_t* q = s->q[ch];
      int wi = s->wi[ch];
      ri = s->ri;
      for(i=ch;i<len;i+=nch){
	q[wi] = a[i];
	a[i]  = q[ri];
	UPDATEQI(wi);
	UPDATEQI(ri);
      }
      s->wi[ch] = wi;
      break;
    }
    case 2:{
      int16_t* a = c->audio;
      int16_t* q = s->q[ch];
      int wi = s->wi[ch];
      ri = s->ri;
      for(i=ch;i<len;i+=nch){
	q[wi] = a[i];
	a[i]  = q[ri];
	UPDATEQI(wi);
	UPDATEQI(ri);
      }
      s->wi[ch] = wi;
      break;
    }
    case 4:{
      int32_t* a = c->audio;
      int32_t* q = s->q[ch];
      int wi = s->wi[ch];
      ri = s->ri;
      for(i=ch;i<len;i+=nch){
	q[wi] = a[i];
	a[i]  = q[ri];
	UPDATEQI(wi);
	UPDATEQI(ri);
      }
      s->wi[ch] = wi;
      break;
    }
    }
  }
  s->ri = ri;
  return c;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=1;
  af->data=calloc(1,sizeof(struct mp_audio));
  af->setup=calloc(1,sizeof(af_delay_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  return AF_OK;
}

// Description of this filter
struct af_info af_info_delay = {
    "Delay audio filter",
    "delay",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
