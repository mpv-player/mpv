/* Normalizer plugin
 *
 * Limitations:
 *  - only AFMT_S16_LE supported
 *  - no parameters yet => tweak the values by editing the #defines
 *
 * License: GPLv2
 * Author: pl <p_l@gmx.fr> (c) 2002 and beyond...
 *
 * Sources: some ideas from volnorm plugin for xmms
 *
 * */

#define PLUGIN

/* Values for AVG:
 * 1: uses a 1 value memory and coefficients new=a*old+b*cur (with a+b=1)
 *
 * 2: uses several samples to smooth the variations (standard weighted mean
 *    on past samples)
 *
 * */
#define AVG 1

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>	// for sqrt()

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

static ao_info_t info = {
        "Volume normalizer",
        "volnorm",
        "pl <p_l@gmx.fr>",
        ""
};

LIBAO_PLUGIN_EXTERN(volnorm)

// mul is the value by which the samples are scaled
// and has to be in [MUL_MIN, MUL_MAX]
#define MUL_INIT 1.0
#define MUL_MIN 0.1
#define MUL_MAX 5.0
static float mul;


#if AVG==1
// "history" value of the filter
static float lastavg;

// SMOOTH_* must be in ]0.0, 1.0[
// The new value accounts for SMOOTH_MUL in the value and history
#define SMOOTH_MUL 0.06
#define SMOOTH_LASTAVG 0.06


#elif AVG==2
// Size of the memory array
// FIXME: should depend on the frequency of the data (should be a few seconds)
#define NSAMPLES 128

// Indicates where to write (in 0..NSAMPLES-1)
static int idx;
// The array
static struct {
    float avg;		// average level of the sample
    int32_t len;	// sample size (weight)
} mem[NSAMPLES];

// If summing all the mem[].len is lower than MIN_SAMPLE_SIZE bytes, then we
// choose to ignore the computed value as it's not significant enough
// FIXME: should depend on the frequency of the data (0.5s maybe)
#define MIN_SAMPLE_SIZE 32000

#else
// Kab00m !
#error "Unknown AVG"
#endif


// Some limits
#define MIN_S16 -32768
#define MAX_S16  32767

// "Ideal" level
#define MID_S16 (MAX_S16 * 0.25)

// Silence level
// FIXME: should be relative to the level of the samples
#define SIL_S16 (MAX_S16 * 0.01)


// Local data
static struct {
  int      inuse;     	// This plugin is in use TRUE, FALSE
  int      format;	// sample fomat
} pl_volnorm = {0, 0};


// minimal interface
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    return CONTROL_OK;
  }
  return CONTROL_UNKNOWN;
}

// minimal interface
// open & setup audio device
// return: 1=success 0=fail
static int init(){
  switch(ao_plugin_data.format){
    case(AFMT_S16_LE):
      break;
    default:
      fprintf(stderr,"[pl_volnorm] Audio format not yet supported.\n");
      return 0;
  }

  pl_volnorm.format = ao_plugin_data.format;
  pl_volnorm.inuse = 1;

  reset();

  printf("[pl_volnorm] Normalizer plugin in use.\n");
  return 1;
}

// close plugin
static void uninit(){
  pl_volnorm.inuse=0;
}

// empty buffers
static void reset(){
  int i;
  mul = MUL_INIT;
  switch(ao_plugin_data.format) {
    case(AFMT_S16_LE):
#if AVG==1
      lastavg = MID_S16;
#elif AVG==2
      for(i=0; i < NSAMPLES; ++i) {
	      mem[i].len = 0;
	      mem[i].avg = 0;
      }
      idx = 0;
#endif

      break;
    default:
      fprintf(stderr,"[pl_volnorm] internal inconsistency - bugreport !\n");
      *(char *) 0 = 0;
  }
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){

  switch(pl_volnorm.format){
  case(AFMT_S16_LE): {

#define CLAMP(x,m,M) do { if ((x)<(m)) (x) = (m); else if ((x)>(M)) (x) = (M); } while(0)

    int16_t* data=(int16_t*)ao_plugin_data.data;
    int len=ao_plugin_data.len / 2; // 16 bits samples

    int32_t i, tmp;
    float curavg, newavg;

#if AVG==1
    float neededmul;
#elif AVG==2
    float avg;
    int32_t totallen;
#endif

    // Evaluate current samples average level
    curavg = 0.0;
    for (i = 0; i < len ; ++i) {
      tmp = data[i];
      curavg += tmp * tmp;
    }
    curavg = sqrt(curavg / (float) len);

    // Evaluate an adequate 'mul' coefficient based on previous state, current
    // samples level, etc
#if AVG==1
    if (curavg > SIL_S16) {
      neededmul = MID_S16 / ( curavg * mul);
      mul = (1.0 - SMOOTH_MUL) * mul + SMOOTH_MUL * neededmul;

      // Clamp the mul coefficient
      CLAMP(mul, MUL_MIN, MUL_MAX);
    }
#elif AVG==2
    avg = 0.0;
    totallen = 0;

    for (i = 0; i < NSAMPLES; ++i) {
        avg += mem[i].avg * (float) mem[i].len;
        totallen += mem[i].len;
    }

    if (totallen > MIN_SAMPLE_SIZE) {
    	avg /= (float) totallen;
    	if (avg >= SIL_S16) {
    	    mul = (float) MID_S16 / avg;
    	    CLAMP(mul, MUL_MIN, MUL_MAX);
    	}
    }
#endif

    // Scale & clamp the samples
    for (i = 0; i < len ; ++i) {
      tmp = mul * data[i];
      CLAMP(tmp, MIN_S16, MAX_S16);
      data[i] = tmp;
    }

    // Evaluation of newavg (not 100% accurate because of values clamping)
    newavg = mul * curavg;

    // Stores computed values for future smoothing
#if AVG==1
    lastavg = (1.0 - SMOOTH_LASTAVG) * lastavg + SMOOTH_LASTAVG * newavg;
    //printf("\rmul=%02.1f ", mul);
#elif AVG==2
    mem[idx].len = len;
    mem[idx].avg = newavg;
    idx = (idx + 1) % NSAMPLES;
    //printf("\rmul=%02.1f (%04dKiB) ", mul, totallen/1024);
#endif
    //fflush(stdout);

    break;
  }
  default:
    return 0;
  }
  return 1;

}

