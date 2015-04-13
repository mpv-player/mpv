/*
 * Copyright (C) 2004 Alex Beregszaszi & Pierre Lombard
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
#include <string.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "common/common.h"
#include "af.h"

// Data for specific instances of this filter
typedef struct af_extrastereo_s
{
    float mul;
}af_extrastereo_t;

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    mp_audio_copy_config(af->data, (struct mp_audio*)arg);
    mp_audio_force_interleaved_format(af->data);
    mp_audio_set_num_channels(af->data, 2);
    if (af->data->format != AF_FORMAT_FLOAT)
        mp_audio_set_format(af->data, AF_FORMAT_S16);

    return af_test_output(af,(struct mp_audio*)arg);
  }
  }
  return AF_UNKNOWN;
}

// Filter data through filter
static void play_s16(af_extrastereo_t *s, struct mp_audio* data)
{
  register int i = 0;
  int16_t *a = (int16_t*)data->planes[0];       // Audio data
  int len = data->samples*data->nch;            // Number of samples
  int avg, l, r;

  for (i = 0; i < len; i+=2)
  {
    avg = (a[i] + a[i + 1]) / 2;

    l = avg + (int)(s->mul * (a[i] - avg));
    r = avg + (int)(s->mul * (a[i + 1] - avg));

    a[i] = MPCLAMP(l, SHRT_MIN, SHRT_MAX);
    a[i + 1] = MPCLAMP(r, SHRT_MIN, SHRT_MAX);
  }
}

static void play_float(af_extrastereo_t *s, struct mp_audio* data)
{
  register int i = 0;
  float *a = (float*)data->planes[0];   // Audio data
  int len = data->samples * data->nch;  // Number of samples
  float avg, l, r;

  for (i = 0; i < len; i+=2)
  {
    avg = (a[i] + a[i + 1]) / 2;

    l = avg + (s->mul * (a[i] - avg));
    r = avg + (s->mul * (a[i + 1] - avg));

    a[i] = af_softclip(l);
    a[i + 1] = af_softclip(r);
  }
}

static int filter_frame(struct af_instance *af, struct mp_audio *data)
{
    if (!data)
        return 0;
    if (af_make_writeable(af, data) < 0) {
        talloc_free(data);
        return -1;
    }
    if (data->format == AF_FORMAT_FLOAT) {
        play_float(af->priv, data);
    } else {
        play_s16(af->priv, data);
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

#define OPT_BASE_STRUCT af_extrastereo_t
const struct af_info af_info_extrastereo = {
    .info = "Increase difference between audio channels",
    .name = "extrastereo",
    .flags = AF_FLAGS_NOT_REENTRANT,
    .open = af_open,
    .priv_size = sizeof(af_extrastereo_t),
    .options = (const struct m_option[]) {
        OPT_FLOAT("mul", mul, 0, OPTDEF_FLOAT(2.5)),
        {0}
    },
};
