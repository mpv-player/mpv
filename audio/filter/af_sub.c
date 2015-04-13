/*
 * Copyright (C) 2002 Anders Johansson ajh@watri.uwa.edu.au
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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

#include "common/common.h"
#include "af.h"
#include "dsp.h"

// Q value for low-pass filter
#define Q 1.0

// Analog domain biquad section
typedef struct{
  float a[3];           // Numerator coefficients
  float b[3];           // Denominator coefficients
} biquad_t;

// S-parameters for designing 4th order Butterworth filter
static const biquad_t sp[2] = {{{1.0,0.0,0.0},{1.0,0.765367,1.0}},
                               {{1.0,0.0,0.0},{1.0,1.847759,1.0}}};

// Data for specific instances of this filter
typedef struct af_sub_s
{
  float w[2][4];        // Filter taps for low-pass filter
  float q[2][2];        // Circular queues
  float fc;             // Cutoff frequency [Hz] for low-pass filter
  float k;              // Filter gain;
  int ch;               // Channel number which to insert the filtered data

}af_sub_t;

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_sub_t* s   = af->priv;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    af->data->rate   = ((struct mp_audio*)arg)->rate;
    mp_audio_set_channels_old(af->data, MPMAX(s->ch+1,((struct mp_audio*)arg)->nch));
    mp_audio_set_format(af->data, AF_FORMAT_FLOAT);

    // Design low-pass filter
    s->k = 1.0;
    if((-1 == af_filter_szxform(sp[0].a, sp[0].b, Q, s->fc,
       (float)af->data->rate, &s->k, s->w[0])) ||
       (-1 == af_filter_szxform(sp[1].a, sp[1].b, Q, s->fc,
       (float)af->data->rate, &s->k, s->w[1])))
      return AF_ERROR;
    return af_test_output(af,(struct mp_audio*)arg);
  }
  }
  return AF_UNKNOWN;
}

#ifndef IIR
#define IIR(in,w,q,out) { \
  float h0 = (q)[0]; \
  float h1 = (q)[1]; \
  float hn = (in) - h0 * (w)[0] - h1 * (w)[1];  \
  out = hn + h0 * (w)[2] + h1 * (w)[3];  \
  (q)[1] = h0; \
  (q)[0] = hn; \
}
#endif

static int filter_frame(struct af_instance *af, struct mp_audio *data)
{
  if (!data)
    return 0;
  if (af_make_writeable(af, data) < 0) {
    talloc_free(data);
    return -1;
  }

  struct mp_audio*    c   = data;        // Current working data
  af_sub_t*     s   = af->priv; // Setup for this instance
  float*        a   = c->planes[0];      // Audio data
  int           len = c->samples*c->nch;         // Number of samples in current audio block
  int           nch = c->nch;    // Number of channels
  int           ch  = s->ch;     // Channel in which to insert the sub audio
  register int  i;

  // Run filter
  for(i=0;i<len;i+=nch){
    // Average left and right
    register float x = 0.5 * (a[i] + a[i+1]);
    IIR(x * s->k, s->w[0], s->q[0], x);
    IIR(x , s->w[1], s->q[1], a[i+ch]);
  }

  af_add_output_frame(af, data);
  return 0;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
  af->control=control;
  af->filter_frame = filter_frame;
  return AF_OK;
}

#define OPT_BASE_STRUCT af_sub_t
const struct af_info af_info_sub = {
    .info = "Audio filter for adding a sub-base channel",
    .name = "sub",
    .flags = AF_FLAGS_NOT_REENTRANT,
    .open = af_open,
    .priv_size = sizeof(af_sub_t),
    .options = (const struct m_option[]) {
        OPT_FLOATRANGE("fc", fc, 0, 20, 300, OPTDEF_FLOAT(60.0)),
        OPT_INTRANGE("ch", ch, 0, 0, AF_NCH - 1, OPTDEF_INT(5)),
        {0}
    },
};
