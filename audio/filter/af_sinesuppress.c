/*
 * Copyright (C) 2006 Michael Niedermayer
 * Copyright (C) 2004 Alex Beregszaszi
 * based upon af_extrastereo.c by Pierre Lombard
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
#include <limits.h>

#include "af.h"

// Data for specific instances of this filter
typedef struct af_sinesuppress_s
{
    double freq;
    double decay;
    double real;
    double imag;
    double ref;
    double pos;
}af_sinesuppress_t;

static int play_s16(struct af_instance* af, struct mp_audio* data, int f);
//static struct mp_audio* play_float(struct af_instance* af, struct mp_audio* data);

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    mp_audio_copy_config(af->data, (struct mp_audio*)arg);
    mp_audio_set_num_channels(af->data, 1);
#if 0
    if (((struct mp_audio*)arg)->format == AF_FORMAT_FLOAT)
    {
	af->data->format = AF_FORMAT_FLOAT;
	af->data->bps = 4;
	af->play = play_float;
    }// else
#endif
    {
        mp_audio_set_format(af->data, AF_FORMAT_S16);
	af->filter = play_s16;
    }

    return af_test_output(af,(struct mp_audio*)arg);
  }
  }
  return AF_UNKNOWN;
}

// Filter data through filter
static int play_s16(struct af_instance* af, struct mp_audio* data, int f)
{
  af_sinesuppress_t *s = af->priv;
  register int i = 0;
  int16_t *a = (int16_t*)data->planes[0];	// Audio data
  int len = data->samples*data->nch;		// Number of samples

  for (i = 0; i < len; i++)
  {
    double co= cos(s->pos);
    double si= sin(s->pos);

    s->real += co * a[i];
    s->imag += si * a[i];
    s->ref  += co * co;

    a[i] -= (s->real * co + s->imag * si) / s->ref;

    s->real -= s->real * s->decay;
    s->imag -= s->imag * s->decay;
    s->ref  -= s->ref  * s->decay;

    s->pos += 2 * M_PI * s->freq / data->rate;
  }

   MP_VERBOSE(af, "[sinesuppress] f:%8.2f: amp:%8.2f\n", s->freq, sqrt(s->real*s->real + s->imag*s->imag) / s->ref);

  return 0;
}

#if 0
static struct mp_audio* play_float(struct af_instance* af, struct mp_audio* data)
{
  af_sinesuppress_t *s = af->setup;
  register int i = 0;
  float *a = (float*)data->audio;	// Audio data
  int len = data->len/4;		// Number of samples
  float avg, l, r;

  for (i = 0; i < len; i+=2)
  {
    avg = (a[i] + a[i + 1]) / 2;

/*    l = avg + (s->mul * (a[i] - avg));
    r = avg + (s->mul * (a[i + 1] - avg));*/

    a[i] = af_softclip(l);
    a[i + 1] = af_softclip(r);
  }

  return data;
}
#endif

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
  af->control=control;
  af->filter=play_s16;
  return AF_OK;
}

#define OPT_BASE_STRUCT af_sinesuppress_t
struct af_info af_info_sinesuppress = {
    .info = "Sine Suppress",
    .name = "sinesuppress",
    .open = af_open,
    .priv_size = sizeof(af_sinesuppress_t),
    .options = (const struct m_option[]) {
        OPT_DOUBLE("freq", freq, 0, OPTDEF_DOUBLE(50.0)),
        OPT_DOUBLE("decay", decay, 0, OPTDEF_DOUBLE(0.0001)),
        {0}
    },
};
