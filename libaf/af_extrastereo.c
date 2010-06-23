/*
 * Copyright (C) 2004 Alex Beregszaszi & Pierre Lombard
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
typedef struct af_extrastereo_s
{
    float mul;
}af_extrastereo_t;

static af_data_t* play_s16(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_float(struct af_instance_s* af, af_data_t* data);

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_extrastereo_t* s   = (af_extrastereo_t*)af->setup;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = 2;
    if (((af_data_t*)arg)->format == AF_FORMAT_FLOAT_NE)
    {
	af->data->format = AF_FORMAT_FLOAT_NE;
	af->data->bps = 4;
	af->play = play_float;
    }// else
    {
	af->data->format = AF_FORMAT_S16_NE;
	af->data->bps = 2;
	af->play = play_s16;
    }

    return af_test_output(af,(af_data_t*)arg);
  }
  case AF_CONTROL_COMMAND_LINE:{
    float f;
    sscanf((char*)arg,"%f", &f);
    s->mul = f;
    return AF_OK;
  }
  case AF_CONTROL_ES_MUL | AF_CONTROL_SET:
    s->mul = *(float*)arg;
    return AF_OK;
  case AF_CONTROL_ES_MUL | AF_CONTROL_GET:
    *(float*)arg = s->mul;
    return AF_OK;
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

// Filter data through filter
static af_data_t* play_s16(struct af_instance_s* af, af_data_t* data)
{
  af_extrastereo_t *s = af->setup;
  register int i = 0;
  int16_t *a = (int16_t*)data->audio;	// Audio data
  int len = data->len/2;		// Number of samples
  int avg, l, r;

  for (i = 0; i < len; i+=2)
  {
    avg = (a[i] + a[i + 1]) / 2;

    l = avg + (int)(s->mul * (a[i] - avg));
    r = avg + (int)(s->mul * (a[i + 1] - avg));

    a[i] = clamp(l, SHRT_MIN, SHRT_MAX);
    a[i + 1] = clamp(r, SHRT_MIN, SHRT_MAX);
  }

  return data;
}

static af_data_t* play_float(struct af_instance_s* af, af_data_t* data)
{
  af_extrastereo_t *s = af->setup;
  register int i = 0;
  float *a = (float*)data->audio;	// Audio data
  int len = data->len/4;		// Number of samples
  float avg, l, r;

  for (i = 0; i < len; i+=2)
  {
    avg = (a[i] + a[i + 1]) / 2;

    l = avg + (s->mul * (a[i] - avg));
    r = avg + (s->mul * (a[i + 1] - avg));

    a[i] = af_softclip(l);
    a[i + 1] = af_softclip(r);
  }

  return data;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play_s16;
  af->mul=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_extrastereo_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;

  ((af_extrastereo_t*)af->setup)->mul = 2.5;
  return AF_OK;
}

// Description of this filter
af_info_t af_info_extrastereo = {
    "Increase difference between audio channels",
    "extrastereo",
    "Alex Beregszaszi & Pierre Lombard",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
