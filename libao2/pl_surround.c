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

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

#include "remez.h"
#include "firfilter.c"

static ao_info_t info =
{
        "Surround decoder plugin",
        "surround",
        "Steve Davies <steve@daviesfam.org>",
        ""
};

LIBAO_PLUGIN_EXTERN(surround)

// local data
typedef struct pl_surround_s
{
  int passthrough;      // Just be a "NO-OP"
  int msecs;            // Rear channel delay in milliseconds
  int16_t* databuf;     // Output audio buffer
  int16_t* Ls_delaybuf; // circular buffer to be used for delaying Ls audio
  int16_t* Rs_delaybuf; // circular buffer to be used for delaying Rs audio
  int delaybuf_len;     // delaybuf buffer length in samples
  int delaybuf_pos;     // offset in buffer where we are reading/writing
  double* filter_coefs_surround; // FIR filter coefficients for surround sound 7kHz lowpass
  int rate;             // input data rate
  int format;           // input format
  int input_channels;   // input channels

} pl_surround_t;

static pl_surround_t pl_surround={0,20,NULL,NULL,NULL,0,0,NULL,0,0,0};

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    if (pl_surround.passthrough) return CONTROL_OK;
    //fprintf(stderr, "pl_surround: AOCONTROL_PLUGIN_SET_LEN with arg=%d\n", arg);
    //fprintf(stderr, "pl_surround: ao_plugin_data.len=%d\n", ao_plugin_data.len);
    // Allocate an output buffer
    if (pl_surround.databuf != NULL) {
      free(pl_surround.databuf);  pl_surround.databuf = NULL;
    }
    // Allocate output buffer
    pl_surround.databuf = calloc(ao_plugin_data.len, 1);
    // Return back smaller len so we don't get overflowed...
    ao_plugin_data.len /= 2;
    return CONTROL_OK;
  }
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){

  fprintf(stderr, "pl_surround: init input rate=%d, channels=%d\n", ao_plugin_data.rate, ao_plugin_data.channels);
  if (ao_plugin_data.channels != 2) {
    fprintf(stderr, "pl_surround: source audio must have 2 channels, using passthrough mode\n");
    pl_surround.passthrough = 1;
    return 1;
  }
  if (ao_plugin_data.format != AFMT_S16_LE) {
    fprintf(stderr, "pl_surround: I'm dumb and can only handle AFMT_S16_LE audio format, using passthrough mode\n");
    pl_surround.passthrough = 1;
    return 1;
  }

  pl_surround.passthrough = 0;

  /* Store info on input format to expect */
  pl_surround.rate=ao_plugin_data.rate;
  pl_surround.format=ao_plugin_data.format;
  pl_surround.input_channels=ao_plugin_data.channels;

  // Input 2 channels, output will be 4 - tell ao_plugin
  ao_plugin_data.channels    = 4;
  ao_plugin_data.sz_mult    /= 2;

  // Figure out buffer space (in int16_ts) needed for the 15msec delay
  // Extra 31 samples allow for lowpass filter delay (taps-1)
  pl_surround.delaybuf_len = (pl_surround.rate * pl_surround.msecs / 1000) + 31;
  // Allocate delay buffers
  pl_surround.Ls_delaybuf=(void*)calloc(pl_surround.delaybuf_len,sizeof(int16_t));
  pl_surround.Rs_delaybuf=(void*)calloc(pl_surround.delaybuf_len,sizeof(int16_t));
  fprintf(stderr, "pl_surround: %dmsec surround delay, rate %d - buffers are %d bytes each\n",
	  pl_surround.msecs,pl_surround.rate,  pl_surround.delaybuf_len*sizeof(int16_t));
  pl_surround.delaybuf_pos = 0;
  // Surround filer coefficients
  pl_surround.filter_coefs_surround = calc_coefficients_7kHz_lowpass(pl_surround.rate);
  //dump_filter_coefficients(pl_surround.filter_coefs_surround);
  //testfilter(pl_surround.filter_coefs_surround, 32, pl_surround.rate);
  return 1;
}

// close plugin
static void uninit(){
  //  fprintf(stderr, "pl_surround: uninit called!\n");
  if (pl_surround.passthrough) return;
  if(pl_surround.Ls_delaybuf) 
    free(pl_surround.Ls_delaybuf);
  if(pl_surround.Rs_delaybuf) 
    free(pl_surround.Rs_delaybuf);
  if(pl_surround.databuf) {
    free(pl_surround.databuf);
    pl_surround.databuf = NULL;
  }
  pl_surround.delaybuf_len=0;
}

// empty buffers
static void reset()
{
  if (pl_surround.passthrough) return;
  //fprintf(stderr, "pl_surround: reset called\n");
  pl_surround.delaybuf_pos = 0;
  memset(pl_surround.Ls_delaybuf, 0, sizeof(int16_t)*pl_surround.delaybuf_len);
  memset(pl_surround.Rs_delaybuf, 0, sizeof(int16_t)*pl_surround.delaybuf_len);
}

// The beginnings of an active matrix...
static double steering_matrix[][12] = {
//	LL	RL	LR	RR	LS	RS	LLs	RLs	LRs	RRs	LC	RC	
       {.707,	.0,	.0,	.707,	.5,	-.5,	.5878,	-.3928,	.3928,	-.5878,	.5,	.5},
};

// Experimental moving average dominances
static int amp_L = 0, amp_R = 0, amp_C = 0, amp_S = 0;

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){
  int16_t *in, *out;
  int i, samples;
  double *matrix = steering_matrix[0]; // later we'll index based on detected dominance

  if (pl_surround.passthrough) return 1;

  // fprintf(stderr, "pl_surround: play %d bytes, %d samples\n", ao_plugin_data.len, samples);

  samples  = ao_plugin_data.len / sizeof(int16_t) / pl_surround.input_channels;
  out = pl_surround.databuf;  in = (int16_t *)ao_plugin_data.data;

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
    out[2] = firfilter(pl_surround.Ls_delaybuf, pl_surround.delaybuf_pos,
		       pl_surround.delaybuf_len, 32, pl_surround.filter_coefs_surround);
#ifdef SPLITREAR
    out[3] = firfilter(pl_surround.Rs_delaybuf, pl_surround.delaybuf_pos,
		       pl_surround.delaybuf_len, 32, pl_surround.filter_coefs_surround);
#else
    out[3] = -out[2];
#endif
    // calculate and save surround for 20msecs time
#ifdef SPLITREAR
    pl_surround.Ls_delaybuf[pl_surround.delaybuf_pos] =
      matrix[6]*in[0] + matrix[7]*in[1];
    pl_surround.Rs_delaybuf[pl_surround.delaybuf_pos++] =
      matrix[8]*in[0] + matrix[9]*in[1];
#else
    pl_surround.Ls_delaybuf[pl_surround.delaybuf_pos++] =
      matrix[4]*in[0] + matrix[5]*in[1];
#endif
    pl_surround.delaybuf_pos %= pl_surround.delaybuf_len;

    // next samples...
    in = &in[pl_surround.input_channels];  out = &out[4];
  }

  // Show some state
  //printf("\npl_surround: delaybuf_pos=%d, samples=%d\r\033[A", pl_surround.delaybuf_pos, samples);
  
  // Set output block/len
  ao_plugin_data.data=pl_surround.databuf;
  ao_plugin_data.len=samples*sizeof(int16_t)*4;
  return 1;
}
