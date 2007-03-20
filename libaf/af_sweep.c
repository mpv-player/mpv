// Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
// #inlcude <GPL_v2.h>

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
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_sweept* s   = (af_sweept*)af->setup; 
  af_data_t *data= (af_data_t*)arg;

  switch(cmd){
  case AF_CONTROL_REINIT:
    af->data->nch    = data->nch;
    af->data->format = AF_FORMAT_S16_NE;
    af->data->bps    = 2;
    af->data->rate   = data->rate;

    return AF_OK;
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
static void uninit(struct af_instance_s* af)
{
    if(af->data)
        free(af->data);
    if(af->setup){
        af_sweept *s = af->setup;
        free(s);
    }
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
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

static int af_open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_sweept));
  return AF_OK;
}

af_info_t af_info_sweep = {
  "sine sweep",
  "sweep",
  "Michael Niedermayer",
  "",
  AF_FLAGS_REENTRANT,
  af_open
};

