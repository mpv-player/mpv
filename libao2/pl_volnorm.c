/* Normalizer plugin
 *
 * Limitations:
 *  - only AFMT_S16_LE supported
 *  - no parameters yet => tweak the values by editing the #defines
 *
 * License: GPLv2
 * Author: pl <p_l@gmx.fr> (c) 2002 and beyond...
 *
 * Sources: some ideas from volnorm for xmms
 *
 * */

#define PLUGIN

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
#define MUL_MAX 15.0
static float mul;

// "history" value of the filter
static float lastavg;

// SMOOTH_* must be in ]0.0, 1.0[
// The new value accounts for SMOOTH_MUL in the value and history
#define SMOOTH_MUL 0.06
#define SMOOTH_LASTAVG 0.06

// ideal average level
#define MID_S16 (INT16_MAX * 0.25)

// silence level
#define SIL_S16 (INT16_MAX * 0.02)

// local data
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
  mul = MUL_INIT;
  switch(ao_plugin_data.format) {
    case(AFMT_S16_LE):
      lastavg = MID_S16;
      break;
    default:
      fprintf(stderr,"[pl_volnorm] internal inconsistency - please bugreport.\n");
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

    int32_t i;
    register int32_t tmp;
    register float curavg;
    float newavg;
    float neededmul;

    // average of the current samples
    curavg = 0.0;
    for (i = 0; i < len ; ++i) {
      tmp = data[i];
      curavg += tmp * tmp;
    }
    curavg = sqrt(curavg / (float) len);

    if (curavg > SIL_S16) {
      neededmul = MID_S16 / ( curavg * mul);
      mul = (1.0 - SMOOTH_MUL) * mul + SMOOTH_MUL * neededmul;

      // Clamp the mul coefficient
      CLAMP(mul, MUL_MIN, MUL_MAX);
    }

    // Scale & clamp the samples
    for (i=0; i < len ; ++i) {
      tmp = data[i] * mul;
      CLAMP(tmp, INT16_MIN, INT16_MAX);
      data[i] = tmp;
    }

    // Evaluation of newavg (not 100% accurate because of values clamping)
    newavg = mul * curavg;

#if 0
    printf("time = %d len = %d curavg = %6.0f lastavg = %6.0f newavg = %6.0f\n"
           " needed_m = %2.2f m = %2.2f\n\n",
            time(NULL), len, curavg, lastavg, newavg, neededmul, mul);
#endif

    lastavg = (1.0 - SMOOTH_LASTAVG) * lastavg + SMOOTH_LASTAVG * newavg;

    break;
  }
  default:
    return 0;
  }
  return 1;

}

