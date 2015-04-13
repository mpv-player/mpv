/*
 * Audio filter that adds and removes channels, according to the
 * command line parameter channels. It is stupid and can only add
 * silence or copy channels, not mix or filter.
 *
 * Original author: Anders
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

#include "common/common.h"
#include "af.h"

#define FR 0
#define TO 1

typedef struct af_channels_s{
  int route[AF_NCH][2];
  int nch, nr;
  int router;
  char *routes;
}af_channels_t;

// Local function for copying data
static void copy(struct af_instance *af, void* in, void* out,
                 int ins, int inos,int outs, int outos, int len, int bps)
{
  switch(bps){
  case 1:{
    int8_t* tin  = (int8_t*)in;
    int8_t* tout = (int8_t*)out;
    tin  += inos;
    tout += outos;
    len = len/ins;
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 2:{
    int16_t* tin  = (int16_t*)in;
    int16_t* tout = (int16_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(2*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 3:{
    int8_t* tin  = (int8_t*)in;
    int8_t* tout = (int8_t*)out;
    tin  += 3 * inos;
    tout += 3 * outos;
    len = len / ( 3 * ins);
    while (len--) {
      tout[0] = tin[0];
      tout[1] = tin[1];
      tout[2] = tin[2];
      tin += 3 * ins;
      tout += 3 * outs;
    }
    break;
  }
  case 4:{
    int32_t* tin  = (int32_t*)in;
    int32_t* tout = (int32_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(4*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 8:{
    int64_t* tin  = (int64_t*)in;
    int64_t* tout = (int64_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(8*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  default:
    MP_ERR(af, "Unsupported number of bytes/sample: %i"
           " please report this error on the MPlayer mailing list. \n",bps);
  }
}

// Make sure the routes are sane
static int check_routes(struct af_instance *af, int nin, int nout)
{
  af_channels_t* s = af->priv;
  int i;
  if((s->nr < 1) || (s->nr > AF_NCH)){
    MP_ERR(af, "The number of routing pairs must be"
           " between 1 and %i. Current value is %i\n",AF_NCH,s->nr);
    return AF_ERROR;
  }

  for(i=0;i<s->nr;i++){
    if((s->route[i][FR] >= nin) || (s->route[i][TO] >= nout)){
      MP_ERR(af, "Invalid routing in pair nr. %i.\n", i);
      return AF_ERROR;
    }
  }
  return AF_OK;
}

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_channels_t* s = af->priv;
  switch(cmd){
  case AF_CONTROL_REINIT: ;

    struct mp_chmap chmap;
    mp_chmap_set_unknown(&chmap, s->nch);
    mp_audio_set_channels(af->data, &chmap);

    // Set default channel assignment
    if(!s->router){
      int i;
      // Make sure this filter isn't redundant
      if(af->data->nch == ((struct mp_audio*)arg)->nch)
        return AF_DETACH;

      // If mono: fake stereo
      if(((struct mp_audio*)arg)->nch == 1){
        s->nr = MPMIN(af->data->nch,2);
        for(i=0;i<s->nr;i++){
          s->route[i][FR] = 0;
          s->route[i][TO] = i;
        }
      }
      else{
        s->nr = MPMIN(af->data->nch, ((struct mp_audio*)arg)->nch);
        for(i=0;i<s->nr;i++){
          s->route[i][FR] = i;
          s->route[i][TO] = i;
        }
      }
    }

    af->data->rate   = ((struct mp_audio*)arg)->rate;
    mp_audio_force_interleaved_format((struct mp_audio*)arg);
    mp_audio_set_format(af->data, ((struct mp_audio*)arg)->format);
    return check_routes(af,((struct mp_audio*)arg)->nch,af->data->nch);
  }
  return AF_UNKNOWN;
}

static int filter_frame(struct af_instance *af, struct mp_audio *c)
{
  af_channels_t* s = af->priv;
  int            i;

  if (!c)
    return 0;

  struct mp_audio *l = mp_audio_pool_get(af->out_pool, &af->fmt_out, c->samples);
  if (!l) {
    talloc_free(c);
    return -1;
  }
  mp_audio_copy_attributes(l, c);

  // Reset unused channels
  memset(l->planes[0],0,mp_audio_psize(c) / c->nch * l->nch);

  if(AF_OK == check_routes(af,c->nch,l->nch))
    for(i=0;i<s->nr;i++)
      copy(af, c->planes[0],l->planes[0],c->nch,s->route[i][FR],
           l->nch,s->route[i][TO],mp_audio_psize(c),c->bps);

  talloc_free(c);
  af_add_output_frame(af, l);
  return 0;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
    af->control=control;
    af->filter_frame = filter_frame;
    af_channels_t *s = af->priv;

    // If router scan commandline for routing pairs
    if(s->routes && s->routes[0]){
        char* cp = s->routes;
        int ch = 0;
        // Scan for pairs on commandline
        do {
            int n = 0;
            if (ch >= AF_NCH) {
                MP_FATAL(af, "Can't have more than %d routes.\n", AF_NCH);
                return AF_ERROR;
            }
            sscanf(cp, "%i-%i%n" ,&s->route[ch][FR], &s->route[ch][TO], &n);
            MP_VERBOSE(af, "Routing from channel %i to"
                " channel %i\n",s->route[ch][FR],s->route[ch][TO]);
            cp = &cp[n];
            ch++;
        } while(*cp == ',' && *(cp++));
        s->nr = ch;
        if (s->nr > 0)
            s->router = 1;
    }

    return AF_OK;
}

#define OPT_BASE_STRUCT af_channels_t
const struct af_info af_info_channels = {
    .info = "Insert or remove channels",
    .name = "channels",
    .open = af_open,
    .priv_size = sizeof(af_channels_t),
    .options = (const struct m_option[]) {
        OPT_INTRANGE("nch", nch, 0, 1, AF_NCH, OPTDEF_INT(2)),
        OPT_STRING("routes", routes, 0),
        {0}
    },
};
