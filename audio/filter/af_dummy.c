/*
 * The name speaks for itself. This filter is a dummy and will
 * not blow up regardless of what you do with it.
 *
 * Original author: Anders
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

#include "af.h"

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT: ;
    *af->data = *(struct mp_audio*)arg;
    MP_VERBOSE(af, "Was reinitialized: %iHz/%ich/%s\n",
        af->data->rate,af->data->nch,af_fmt_to_str(af->data->format));
    return AF_OK;
  }
  return AF_UNKNOWN;
}

// Filter data through filter
static int filter(struct af_instance* af, struct mp_audio* data, int flags)
{
    return 0;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
  af->control=control;
  af->filter=filter;
  return AF_OK;
}

// Description of this filter
struct af_info af_info_dummy = {
    .info = "dummy",
    .name = "dummy",
    .open = af_open,
};
