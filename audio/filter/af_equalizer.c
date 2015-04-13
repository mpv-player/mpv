/*
 * Equalizer filter, implementation of a 10 band time domain graphic
 * equalizer using IIR filters. The IIR filters are implemented using a
 * Direct Form II approach, but has been modified (b1 == 0 always) to
 * save computation.
 *
 * Copyright (C) 2001 Anders Johansson ajh@atri.curtin.edu.au
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

#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>
#include <math.h>

#include "common/common.h"
#include "af.h"

#define L       2      // Storage for filter taps
#define KM      10     // Max number of bands

#define Q   1.2247449 /* Q value for band-pass filters 1.2247=(3/2)^(1/2)
                         gives 4dB suppression @ Fc*2 and Fc/2 */

/* Center frequencies for band-pass filters
   The different frequency bands are:
   nr.          center frequency
   0    31.25 Hz
   1    62.50 Hz
   2    125.0 Hz
   3    250.0 Hz
   4    500.0 Hz
   5    1.000 kHz
   6    2.000 kHz
   7    4.000 kHz
   8    8.000 kHz
   9    16.00 kHz
*/
#define CF      {31.25,62.5,125,250,500,1000,2000,4000,8000,16000}

// Maximum and minimum gain for the bands
#define G_MAX   +12.0
#define G_MIN   -12.0

// Data for specific instances of this filter
typedef struct af_equalizer_s
{
  float   a[KM][L];             // A weights
  float   b[KM][L];             // B weights
  float   wq[AF_NCH][KM][L];    // Circular buffer for W data
  float   g[AF_NCH][KM];        // Gain factor for each channel and band
  int     K;                    // Number of used eq bands
  int     channels;             // Number of channels
  float   gain_factor;     // applied at output to avoid clipping
  double  p[KM];
} af_equalizer_t;

// 2nd order Band-pass Filter design
static void bp2(float* a, float* b, float fc, float q){
  double th= 2.0 * M_PI * fc;
  double C = (1.0 - tan(th*q/2.0))/(1.0 + tan(th*q/2.0));

  a[0] = (1.0 + C) * cos(th);
  a[1] = -1 * C;

  b[0] = (1.0 - C)/2.0;
  b[1] = -1.0050;
}

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_equalizer_t* s   = (af_equalizer_t*)af->priv;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    int k =0, i =0;
    float F[KM] = CF;

    s->gain_factor=0.0;

    // Sanity check
    if(!arg) return AF_ERROR;

    mp_audio_copy_config(af->data, (struct mp_audio*)arg);
    mp_audio_set_format(af->data, AF_FORMAT_FLOAT);

    // Calculate number of active filters
    s->K=KM;
    while(F[s->K-1] > (float)af->data->rate/2.2)
      s->K--;

    if(s->K != KM)
      MP_INFO(af, "Limiting the number of filters to"
             " %i due to low sample rate.\n",s->K);

    // Generate filter taps
    for(k=0;k<s->K;k++)
      bp2(s->a[k],s->b[k],F[k]/((float)af->data->rate),Q);

    // Calculate how much this plugin adds to the overall time delay
    af->delay = 2.0 / (double)af->data->rate;

    // Calculate gain factor to prevent clipping at output
    for(k=0;k<AF_NCH;k++)
    {
        for(i=0;i<KM;i++)
        {
            if(s->gain_factor < s->g[k][i]) s->gain_factor=s->g[k][i];
        }
    }

    s->gain_factor=log10(s->gain_factor + 1.0) * 20.0;

    if(s->gain_factor > 0.0)
    {
        s->gain_factor=0.1+(s->gain_factor/12.0);
    }else{
        s->gain_factor=1;
    }

    return af_test_output(af,arg);
  }
  }
  return AF_UNKNOWN;
}

static int filter(struct af_instance* af, struct mp_audio* data)
{
  struct mp_audio*       c      = data;                         // Current working data
  if (!c)
    return 0;
  af_equalizer_t*  s    = (af_equalizer_t*)af->priv;    // Setup
  uint32_t         ci   = af->data->nch;                // Index for channels
  uint32_t         nch  = af->data->nch;                // Number of channels

  if (af_make_writeable(af, data) < 0) {
    talloc_free(data);
    return -1;
  }

  while(ci--){
    float*      g   = s->g[ci];      // Gain factor
    float*      in  = ((float*)c->planes[0])+ci;
    float*      out = ((float*)c->planes[0])+ci;
    float*      end = in + c->samples*c->nch; // Block loop end

    while(in < end){
      register int      k  = 0;         // Frequency band index
      register float    yt = *in;       // Current input sample
      in+=nch;

      // Run the filters
      for(;k<s->K;k++){
        // Pointer to circular buffer wq
        register float* wq = s->wq[ci][k];
        // Calculate output from AR part of current filter
        register float w=yt*s->b[k][0] + wq[0]*s->a[k][0] + wq[1]*s->a[k][1];
        // Calculate output form MA part of current filter
        yt+=(w + wq[1]*s->b[k][1])*g[k];
        // Update circular buffer
        wq[1] = wq[0];
        wq[0] = w;
      }
      // Calculate output
      *out=yt*s->gain_factor;
      out+=nch;
    }
  }
  af_add_output_frame(af, data);
  return 0;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
  af->control=control;
  af->filter_frame = filter;
  af_equalizer_t *priv = af->priv;
  for(int i=0;i<AF_NCH;i++){
      for(int j=0;j<KM;j++){
        priv->g[i][j] = pow(10.0,MPCLAMP(priv->p[j],G_MIN,G_MAX)/20.0)-1.0;
      }
    }
  return AF_OK;
}

#define OPT_BASE_STRUCT af_equalizer_t
const struct af_info af_info_equalizer = {
  .info = "Equalizer audio filter",
  .name = "equalizer",
  .flags = AF_FLAGS_NOT_REENTRANT,
  .open = af_open,
  .priv_size = sizeof(af_equalizer_t),
  .options = (const struct m_option[]) {
#define BAND(n) OPT_DOUBLE("e" #n, p[n], 0)
        BAND(0), BAND(1), BAND(2), BAND(3), BAND(4),
        BAND(5), BAND(6), BAND(7), BAND(8), BAND(9),
        {0}
  },
};
