/*
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
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
#include <math.h>

#include "config.h"
#include "af.h"

typedef struct af_sweep_s{
    double x;
    double delta;
}af_sweept;


// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_sweept* s   = (af_sweept*)af->setup;
  struct mp_audio *data= (struct mp_audio*)arg;

  switch(cmd){
  case AF_CONTROL_REINIT:
    mp_audio_copy_config(af->data, data);
    mp_audio_set_format(af->data, AF_FORMAT_S16_NE);

    return af_test_output(af, data);
  case AF_CONTROL_COMMAND_LINE:
    sscanf((char*)arg,"%lf", &s->delta);
    return AF_OK;
/*  case AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET:
    af->data->rate = *(int*)arg;
    return AF_OK;*/
  }
  return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance* af)
{
    free(af->data);
    free(af->setup);
}

// Filter data through filter
static struct mp_audio* play(struct af_instance* af, struct mp_audio* data)
{
  af_sweept *s = af->setup;
  int i, j;
  int16_t *in = (int16_t*)data->audio;
  int chans   = data->nch;
  int in_len  = data->len/(2*chans);

  for(i=0; i<in_len; i++){
      for(j=0; j<chans; j++)
          in[i*chans+j]= 32000*sin(s->x*s->x);
      s->x += s->delta;
      if(2*s->x*s->delta >= 3.141592) s->x=0;
  }

  return data;
}

static int af_open(struct af_instance* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=1;
  af->data=calloc(1,sizeof(struct mp_audio));
  af->setup=calloc(1,sizeof(af_sweept));
  return AF_OK;
}

struct af_info af_info_sweep = {
  "sine sweep",
  "sweep",
  "Michael Niedermayer",
  "",
  AF_FLAGS_REENTRANT,
  af_open
};
