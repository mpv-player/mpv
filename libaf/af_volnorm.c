/*=============================================================================
//	
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2004 Alex Beregszaszi & Pierre Lombard
//
//=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "af.h"

// Methods:
// 1: uses a 1 value memory and coefficients new=a*old+b*cur (with a+b=1)
// 2: uses several samples to smooth the variations (standard weighted mean
//    on past samples)

// Size of the memory array
// FIXME: should depend on the frequency of the data (should be a few seconds)
#define NSAMPLES 128

// If summing all the mem[].len is lower than MIN_SAMPLE_SIZE bytes, then we
// choose to ignore the computed value as it's not significant enough
// FIXME: should depend on the frequency of the data (0.5s maybe)
#define MIN_SAMPLE_SIZE 32000

// mul is the value by which the samples are scaled
// and has to be in [MUL_MIN, MUL_MAX]
#define MUL_INIT 1.0
#define MUL_MIN 0.1
#define MUL_MAX 5.0

// Silence level
// FIXME: should be relative to the level of the samples
#define SIL_S16 (SHRT_MAX * 0.01)
#define SIL_FLOAT (INT_MAX * 0.01) // FIXME

// smooth must be in ]0.0, 1.0[
#define SMOOTH_MUL 0.06
#define SMOOTH_LASTAVG 0.06

#define DEFAULT_TARGET 0.25

// Data for specific instances of this filter
typedef struct af_volume_s
{
    int method; // method used
    float mul;
    // method 1
    float lastavg; // history value of the filter
    // method 2
    int idx;
    struct {
	float avg; // average level of the sample
	int len; // sample size (weight)
    } mem[NSAMPLES];
    // "Ideal" level
    float mid_s16;
    float mid_float;
}af_volnorm_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_volnorm_t* s   = (af_volnorm_t*)af->setup; 

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return AF_ERROR;
    
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    
    if(((af_data_t*)arg)->format == (AF_FORMAT_S16_NE)){
      af->data->format = AF_FORMAT_S16_NE;
      af->data->bps    = 2;
    }else{
      af->data->format = AF_FORMAT_FLOAT_NE;
      af->data->bps    = 4;
    }
    return af_test_output(af,(af_data_t*)arg);
  case AF_CONTROL_COMMAND_LINE:{
    int   i = 0;
    float target = DEFAULT_TARGET;
    sscanf((char*)arg,"%d:%f", &i, &target);
    if (i != 1 && i != 2)
	return AF_ERROR;
    s->method = i-1;
    s->mid_s16 = ((float)SHRT_MAX) * target;
    s->mid_float = ((float)INT_MAX) * target;
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

static void method1_int16(af_volnorm_t *s, af_data_t *c)
{
  register int i = 0;
  int16_t *data = (int16_t*)c->audio;	// Audio data
  int len = c->len/2;		// Number of samples
  float curavg = 0.0, newavg, neededmul;
  int tmp;
  
  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);
  
  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc
  
  if (curavg > SIL_S16)
  {
    neededmul = s->mid_s16 / (curavg * s->mul);
    s->mul = (1.0 - SMOOTH_MUL) * s->mul + SMOOTH_MUL * neededmul;
    
    // clamp the mul coefficient
    s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
  }
  
  // Scale & clamp the samples
  for (i = 0; i < len; i++)
  {
    tmp = s->mul * data[i];
    tmp = clamp(tmp, SHRT_MIN, SHRT_MAX);
    data[i] = tmp;
  }
  
  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;
  
  // Stores computed values for future smoothing
  s->lastavg = (1.0 - SMOOTH_LASTAVG) * s->lastavg + SMOOTH_LASTAVG * newavg;
}

static void method1_float(af_volnorm_t *s, af_data_t *c)
{
  register int i = 0;
  float *data = (float*)c->audio;	// Audio data
  int len = c->len/4;		// Number of samples
  float curavg = 0.0, newavg, neededmul, tmp;
  
  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);
  
  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc
  
  if (curavg > SIL_FLOAT) // FIXME
  {
    neededmul = s->mid_float / (curavg * s->mul);
    s->mul = (1.0 - SMOOTH_MUL) * s->mul + SMOOTH_MUL * neededmul;
    
    // clamp the mul coefficient
    s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
  }
  
  // Scale & clamp the samples
  for (i = 0; i < len; i++)
    data[i] *= s->mul;
  
  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;
  
  // Stores computed values for future smoothing
  s->lastavg = (1.0 - SMOOTH_LASTAVG) * s->lastavg + SMOOTH_LASTAVG * newavg;
}

static void method2_int16(af_volnorm_t *s, af_data_t *c)
{
  register int i = 0;
  int16_t *data = (int16_t*)c->audio;	// Audio data
  int len = c->len/2;		// Number of samples
  float curavg = 0.0, newavg, avg = 0.0;
  int tmp, totallen = 0;
  
  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);
  
  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc
  for (i = 0; i < NSAMPLES; i++)
  {
    avg += s->mem[i].avg * (float)s->mem[i].len;
    totallen += s->mem[i].len;
  }
  
  if (totallen > MIN_SAMPLE_SIZE)
  {
    avg /= (float)totallen;
    if (avg >= SIL_S16)
    {
	s->mul = s->mid_s16 / avg;
	s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
    }
  }
  
  // Scale & clamp the samples
  for (i = 0; i < len; i++)
  {
    tmp = s->mul * data[i];
    tmp = clamp(tmp, SHRT_MIN, SHRT_MAX);
    data[i] = tmp;
  }
  
  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;
  
  // Stores computed values for future smoothing
  s->mem[s->idx].len = len;
  s->mem[s->idx].avg = newavg;
  s->idx = (s->idx + 1) % NSAMPLES;
}

static void method2_float(af_volnorm_t *s, af_data_t *c)
{
  register int i = 0;
  float *data = (float*)c->audio;	// Audio data
  int len = c->len/4;		// Number of samples
  float curavg = 0.0, newavg, avg = 0.0, tmp;
  int totallen = 0;
  
  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);
  
  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc
  for (i = 0; i < NSAMPLES; i++)
  {
    avg += s->mem[i].avg * (float)s->mem[i].len;
    totallen += s->mem[i].len;
  }
  
  if (totallen > MIN_SAMPLE_SIZE)
  {
    avg /= (float)totallen;
    if (avg >= SIL_FLOAT)
    {
	s->mul = s->mid_float / avg;
	s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
    }
  }
  
  // Scale & clamp the samples
  for (i = 0; i < len; i++)
    data[i] *= s->mul;
  
  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;
  
  // Stores computed values for future smoothing
  s->mem[s->idx].len = len;
  s->mem[s->idx].avg = newavg;
  s->idx = (s->idx + 1) % NSAMPLES;
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_volnorm_t *s = af->setup;

  if(af->data->format == (AF_FORMAT_S16_NE))
  {
    if (s->method)
	method2_int16(s, data);
    else
	method1_int16(s, data);
  }
  else if(af->data->format == (AF_FORMAT_FLOAT_NE))
  { 
    if (s->method)
	method2_float(s, data);
    else
	method1_float(s, data);
  }
  return data;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  int i = 0;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_volnorm_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;

  ((af_volnorm_t*)af->setup)->mul = MUL_INIT;
  ((af_volnorm_t*)af->setup)->lastavg = ((float)SHRT_MAX) * DEFAULT_TARGET;
  ((af_volnorm_t*)af->setup)->idx = 0;
  ((af_volnorm_t*)af->setup)->mid_s16 = ((float)SHRT_MAX) * DEFAULT_TARGET;
  ((af_volnorm_t*)af->setup)->mid_float = ((float)INT_MAX) * DEFAULT_TARGET;
  for (i = 0; i < NSAMPLES; i++)
  {
     ((af_volnorm_t*)af->setup)->mem[i].len = 0;
     ((af_volnorm_t*)af->setup)->mem[i].avg = 0;
  }
  return AF_OK;
}

// Description of this filter
af_info_t af_info_volnorm = {
    "Volume normalizer filter",
    "volnorm",
    "Alex Beregszaszi & Pierre Lombard",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
