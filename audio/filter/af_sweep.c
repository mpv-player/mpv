/*
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
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

#include "config.h"
#include "af.h"

typedef struct af_sweep_s{
    double x;
    double delta;
}af_sweept;


// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  struct mp_audio *data= (struct mp_audio*)arg;

  switch(cmd){
  case AF_CONTROL_REINIT:
    mp_audio_copy_config(af->data, data);
    mp_audio_set_format(af->data, AF_FORMAT_S16);

    return af_test_output(af, data);
  }
  return AF_UNKNOWN;
}

static int filter_frame(struct af_instance *af, struct mp_audio *data)
{
  if (!data)
    return 0;
  if (af_make_writeable(af, data) < 0) {
      talloc_free(data);
    return 0;
  }

  af_sweept *s = af->priv;
  int i, j;
  int16_t *in = (int16_t*)data->planes[0];
  int chans   = data->nch;
  int in_len  = data->samples;

  for(i=0; i<in_len; i++){
      for(j=0; j<chans; j++)
          in[i*chans+j]= 32000*sin(s->x*s->x);
      s->x += s->delta;
      if(2*s->x*s->delta >= 3.141592) s->x=0;
  }

  af_add_output_frame(af, data);
  return 0;
}

static int af_open(struct af_instance* af){
  af->control=control;
  af->filter_frame = filter_frame;
  return AF_OK;
}

#define OPT_BASE_STRUCT af_sweept
const struct af_info af_info_sweep = {
    .info = "sine sweep",
    .name = "sweep",
    .open = af_open,
    .priv_size = sizeof(af_sweept),
    .options = (const struct m_option[]) {
        OPT_DOUBLE("delta", delta, 0),
        {0}
    },
};
