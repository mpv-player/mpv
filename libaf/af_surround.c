/* 
   This is an ao2 plugin to do simple decoding of matrixed surround
   sound.  This will provide a (basic) surround-sound effect from
   audio encoded for Dolby Surround, Pro Logic etc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Original author: Steve Davies <steve@daviesfam.org>
*/

/* The principle:  Make rear channels by extracting anti-phase data
   from the front channels, delay by 20msec and feed to rear in anti-phase
*/


// SPLITREAR: Define to decode two distinct rear channels -
// 	this doesn't work so well in practice because
//      separation in a passive matrix is not high.
//      C (dialogue) to Ls and Rs 14dB or so -
//      so dialogue leaks to the rear.
//      Still - give it a try and send feedback.
//      comment this define for old behaviour of a single
//      surround sent to rear in anti-phase
#define SPLITREAR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "af.h"
#include "dsp.h"

// instance data
typedef struct af_surround_s
{
  float msecs;        // Rear channel delay in milliseconds
  float* Ls_delaybuf; // circular buffer to be used for delaying Ls audio
  float* Rs_delaybuf; // circular buffer to be used for delaying Rs audio
  int delaybuf_len;   // delaybuf buffer length in samples
  int delaybuf_rpos;  // offset in buffer where we are reading
  int delaybuf_wpos;  // offset in buffer where we are writing
  float filter_coefs_surround[32]; // FIR filter coefficients for surround sound 7kHz lowpass
} af_surround_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_surround_t *instance=af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:{
    float cutoff;
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch*2;
    af->data->format = ((af_data_t*)arg)->format;
    af->data->bps    = ((af_data_t*)arg)->bps;
    af_msg(AF_MSG_DEBUG0, "[surround]: rear delay=%0.2fms.\n", instance->msecs);
    if (af->data->nch != 4){
      af_msg(AF_MSG_ERROR,"Only Stereo input is supported, filter disabled.\n");
      return AF_DETACH;
    }
    // Figure out buffer space (in int16_ts) needed for the 15msec delay
    // Extra 31 samples allow for lowpass filter delay (taps-1)
    // Double size to make virtual ringbuffer easier
    instance->delaybuf_len = ((af->data->rate * instance->msecs / 1000)+31)*2;
    // Free old buffers
    if (instance->Ls_delaybuf != NULL)
      free(instance->Ls_delaybuf);
    if (instance->Rs_delaybuf != NULL)
      free(instance->Rs_delaybuf);
    // Allocate new buffers
    instance->Ls_delaybuf=(void*)calloc(instance->delaybuf_len,sizeof(*instance->Ls_delaybuf));
    instance->Rs_delaybuf=(void*)calloc(instance->delaybuf_len,sizeof(*instance->Rs_delaybuf));
    af_msg(AF_MSG_DEBUG1, "Delay buffers are %d samples each.\n", instance->delaybuf_len);
    instance->delaybuf_wpos = 0;
    instance->delaybuf_rpos = 32; // compensate for fir delay
    // Surround filer coefficients
    cutoff = af->data->rate/7000;
    if (-1 == design_fir(32, instance->filter_coefs_surround, &cutoff, LP|KAISER, 10.0)) {
      af_msg(AF_MSG_ERROR,"[surround] Unable to design prototype filter.\n");
      return AF_ERROR;
    }

    return AF_OK;
  }
  case AF_CONTROL_COMMAND_LINE:{
    float d = 0;
    sscanf((char*)arg,"%f",&d);
    if (d<0){
      af_msg(AF_MSG_ERROR,"Error setting rear delay length in af_surround. Delay has to be positive.\n");
      return AF_ERROR;
    }
    instance->msecs=d;
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance_s* af)
{
  af_surround_t *instance=af->setup;
  if(af->data->audio)
    free(af->data->audio);
  if(af->data)
    free(af->data);
  if(instance->Ls_delaybuf)
    free(instance->Ls_delaybuf);
  if(instance->Rs_delaybuf)
    free(instance->Rs_delaybuf);
  free(af->setup);
}

// The beginnings of an active matrix...
static double steering_matrix[][12] = {
//	LL	RL	LR	RR	LS	RS
//	LLs	RLs	LRs	RRs	LC	RC
       {.707,	.0,	.0,	.707,	.5,	-.5,
	.5878,	-.3928,	.3928,	-.5878,	.5,	.5},
};

// Experimental moving average dominances
//static int amp_L = 0, amp_R = 0, amp_C = 0, amp_S = 0;

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data){
  af_surround_t* instance = (af_surround_t*)af->setup;
  int16_t*     in = data->audio;
  int16_t*     out;
  int i, samples;
  double *matrix = steering_matrix[0]; // later we'll index based on detected dominance

  if (AF_OK != RESIZE_LOCAL_BUFFER(af, data))
    return NULL;

  out = af->data->audio;

  // fprintf(stderr, "pl_surround: play %d bytes, %d samples\n", ao_plugin_data.len, samples);

  samples  = data->len / (data->nch * sizeof(int16_t));

  // Testing - place a 1kHz tone on Lt and Rt in anti-phase: should decode in S
  //sinewave(in, samples, pl_surround.input_channels, 1000, 0.0, pl_surround.rate);
  //sinewave(&in[1], samples, pl_surround.input_channels, 1000, PI, pl_surround.rate);

  for (i=0; i<samples; i++) {

    // Dominance:
    //abs(in[0])  abs(in[1]);
    //abs(in[0]+in[1])  abs(in[0]-in[1]);
    //10 * log( abs(in[0]) / (abs(in[1])|1) );
    //10 * log( abs(in[0]+in[1]) / (abs(in[0]-in[1])|1) );

    // About volume balancing...
    //   Surround encoding does the following:
    //       Lt=L+.707*C+.707*S, Rt=R+.707*C-.707*S
    //   So S should be extracted as:
    //       (Lt-Rt)
    //   But we are splitting the S to two output channels, so we
    //   must take 3dB off as we split it:
    //       Ls=Rs=.707*(Lt-Rt)
    //   Trouble is, Lt could be +32767, Rt -32768, so possibility that S will
    //   overflow.  So to avoid that, we cut L/R by 3dB (*.707), and S by 6dB (/2).
    //   this keeps the overall balance, but guarantees no overflow.

    // output front left and right
    out[0] = matrix[0]*in[0] + matrix[1]*in[1];
    out[1] = matrix[2]*in[0] + matrix[3]*in[1];
    // output Ls and Rs - from 20msec ago, lowpass filtered @ 7kHz
    out[2] = fir(32, instance->filter_coefs_surround,
                 &instance->Ls_delaybuf[instance->delaybuf_rpos +
                                        instance->delaybuf_len/2]);
#ifdef SPLITREAR
    out[3] = fir(32, instance->filter_coefs_surround,
                 &instance->Rs_delaybuf[instance->delaybuf_rpos +
                                        instance->delaybuf_len/2]);
#else
    out[3] = -out[2];
#endif
    // calculate and save surround for 20msecs time
#ifdef SPLITREAR
    instance->Ls_delaybuf[instance->delaybuf_wpos] =
    instance->Ls_delaybuf[instance->delaybuf_wpos + instance->delaybuf_len/2] =
      matrix[6]*in[0] + matrix[7]*in[1];
    instance->Rs_delaybuf[instance->delaybuf_wpos] =
    instance->Rs_delaybuf[instance->delaybuf_wpos++ + instance->delaybuf_len/2] =
      matrix[8]*in[0] + matrix[9]*in[1];
#else
    instance->Ls_delaybuf[instance->delaybuf_wpos] =
    instance->Ls_delaybuf[instance->delaybuf_wpos++ + instance->delaybuf_len/2] =
      matrix[4]*in[0] + matrix[5]*in[1];
#endif
    instance->delaybuf_rpos++;
    instance->delaybuf_wpos %= instance->delaybuf_len/2;
    instance->delaybuf_rpos %= instance->delaybuf_len/2;

    // next samples...
    in = &in[data->nch];  out = &out[af->data->nch];
  }

  // Show some state
  //printf("\npl_surround: delaybuf_pos=%d, samples=%d\r\033[A", pl_surround.delaybuf_pos, samples);
  
  // Set output data
  data->audio = af->data->audio;
  data->len   = (data->len*af->mul.n)/af->mul.d;
  data->nch   = af->data->nch;

  return data;
}

static int open(af_instance_t* af){
  af_surround_t *pl_surround;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=2;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=pl_surround=calloc(1,sizeof(af_surround_t));
  pl_surround->msecs=20;
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  return AF_OK;
}

af_info_t af_info_surround =
{
        "Surround decoder filter",
        "surround",
        "Steve Davies <steve@daviesfam.org>",
        "",
        AF_FLAGS_REENTRANT,
        open
};
