// Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
// #inlcude <GPL_v2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "af.h"

#ifdef USE_LIBAVCODEC

#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#include <ffmpeg/rational.h>
#else
#include "../libavcodec/avcodec.h"
#include "../libavcodec/rational.h"
#endif

#define CHANS 6

int64_t ff_gcd(int64_t a, int64_t b);

// Data for specific instances of this filter
typedef struct af_resample_s{
    struct AVResampleContext *avrctx;
    int16_t *in[CHANS];
    int in_alloc;
    int index;
    
    int filter_length;
    int linear;
    int phase_shift;
    double cutoff;
}af_resample_t;


// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  int g;
  af_resample_t* s   = (af_resample_t*)af->setup; 
  af_data_t *data= (af_data_t*)arg;
  int out_rate, test_output_res; // helpers for checking input format

  switch(cmd){
  case AF_CONTROL_REINIT:
    if((af->data->rate == data->rate) || (af->data->rate == 0))
        return AF_DETACH;

    af->data->nch    = data->nch;
    if (af->data->nch > CHANS) af->data->nch = CHANS;
    af->data->format = AF_FORMAT_SI | AF_FORMAT_NE;
    af->data->bps    = 2;
    g= ff_gcd(af->data->rate, data->rate);
    af->mul.n = af->data->rate/g;
    af->mul.d = data->rate/g;
    af->delay = 500*s->filter_length/(double)min(af->data->rate, data->rate);

    if(s->avrctx) av_resample_close(s->avrctx);
    s->avrctx= av_resample_init(af->mul.n, /*in_rate*/af->mul.d, s->filter_length, s->phase_shift, s->linear, s->cutoff);

    // hack to make af_test_output ignore the samplerate change
    out_rate = af->data->rate;
    af->data->rate = data->rate;
    test_output_res = af_test_output(af, (af_data_t*)arg);
    af->data->rate = out_rate;
    return test_output_res;
  case AF_CONTROL_COMMAND_LINE:{
    sscanf((char*)arg,"%d:%d:%d:%d:%lf", &af->data->rate, &s->filter_length, &s->linear, &s->phase_shift, &s->cutoff);
    if(s->cutoff <= 0.0) s->cutoff= max(1.0 - 1.0/s->filter_length, 0.80);
    return AF_OK;
  }
  case AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET:
    af->data->rate = *(int*)arg;
    return AF_OK;
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
    if(af->data)
        free(af->data);
    if(af->setup){
        af_resample_t *s = af->setup;
        if(s->avrctx) av_resample_close(s->avrctx);
        free(s);
    }
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{    
  af_resample_t *s = af->setup;
  int i, j, consumed, ret;
  int16_t *in = (int16_t*)data->audio;
  int16_t *out;
  int chans   = data->nch;
  int in_len  = data->len/(2*chans);
  int out_len = (in_len*af->mul.n) / af->mul.d + 10;
  int16_t tmp[CHANS][out_len];
    
  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
      return NULL;
  
  out= (int16_t*)af->data->audio;
  
  out_len= min(out_len, af->data->len/(2*chans));
  
  if(s->in_alloc < in_len + s->index){
      s->in_alloc= in_len + s->index;
      for(i=0; i<chans; i++){
          s->in[i]= realloc(s->in[i], s->in_alloc*sizeof(int16_t)); //FIXME free this maybe ;)
      }
  }

  for(j=0; j<in_len; j++){
      for(i=0; i<chans; i++){
          s->in[i][j + s->index]= *(in++);
      }
  }
  in_len += s->index;

  for(i=0; i<chans; i++){
      ret= av_resample(s->avrctx, tmp[i], s->in[i], &consumed, in_len, out_len, i+1 == chans);
  }
  out_len= ret;
  
  s->index= in_len - consumed;
  for(i=0; i<chans; i++){
      memmove(s->in[i], s->in[i] + consumed, s->index*sizeof(int16_t));
  }

  for(j=0; j<out_len; j++){
      for(i=0; i<chans; i++){
          *(out++)= tmp[i][j];
      }
  }

  data->audio = af->data->audio;
  data->len   = out_len*chans*2;
  data->rate  = af->data->rate;
  return data;
}

static int open(af_instance_t* af){
  af_resample_t *s = calloc(1,sizeof(af_resample_t));
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  s->filter_length= 16;
  s->cutoff= max(1.0 - 1.0/s->filter_length, 0.80);
  s->phase_shift= 10;
//  s->setup = RSMP_INT | FREQ_SLOPPY;
  af->setup=s;
  return AF_OK;
}

af_info_t af_info_lavcresample = {
  "Sample frequency conversion using libavcodec",
  "lavcresample",
  "Michael Niedermayer",
  "",
  AF_FLAGS_REENTRANT,
  open
};
#endif
