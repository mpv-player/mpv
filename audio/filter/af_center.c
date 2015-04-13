/*
 * This filter adds a center channel to the audio stream by
 * averaging the left and right channel.
 * There are two runtime controls one for setting which channel
 * to insert the center-audio into called AF_CONTROL_SUB_CH.
 *
 * FIXME: implement a high-pass filter for better results.
 *
 * copyright (c) 2005 Alex Beregszaszi
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

#include "common/common.h"
#include "af.h"

// Data for specific instances of this filter
typedef struct af_center_s
{
  int ch;               // Channel number which to insert the filtered data
}af_center_t;

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_center_t* s   = af->priv;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return AF_ERROR;

    af->data->rate   = ((struct mp_audio*)arg)->rate;
    mp_audio_set_channels_old(af->data, MPMAX(s->ch+1,((struct mp_audio*)arg)->nch));
    mp_audio_set_format(af->data, AF_FORMAT_FLOAT);

    return af_test_output(af,(struct mp_audio*)arg);
  }
  }
  return AF_UNKNOWN;
}

static int filter_frame(struct af_instance* af, struct mp_audio* data)
{
  if (!data)
    return 0;
  if (af_make_writeable(af, data) < 0) {
    talloc_free(data);
    return -1;
  }
  struct mp_audio*    c   = data;        // Current working data
  af_center_t*  s   = af->priv; // Setup for this instance
  float*        a   = c->planes[0];      // Audio data
  int           nch = c->nch;    // Number of channels
  int           len = c->samples*c->nch;         // Number of samples in current audio block
  int           ch  = s->ch;     // Channel in which to insert the center audio
  register int  i;

  // Run filter
  for(i=0;i<len;i+=nch){
    // Average left and right
    a[i+ch] = (a[i]/2) + (a[i+1]/2);
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

#define OPT_BASE_STRUCT af_center_t
const struct af_info af_info_center = {
    .info = "Audio filter for adding a center channel",
    .name = "center",
    .flags = AF_FLAGS_NOT_REENTRANT,
    .open = af_open,
    .priv_size = sizeof(af_center_t),
    .options = (const struct m_option[]) {
        OPT_INTRANGE("channel", ch, 0, 0, AF_NCH - 1, OPTDEF_INT(1)),
        {0}
    },
};
