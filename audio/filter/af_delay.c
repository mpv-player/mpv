/*
 * This audio filter delays the output signal for the different
 * channels and can be used for simple position panning.
 * An extension for this filter would be a reverb.
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

#define L 65536

#define UPDATEQI(qi) qi=(qi+1)&(L-1)

// Data for specific instances of this filter
typedef struct af_delay_s
{
  void* q[AF_NCH];      // Circular queues used for delaying audio signal
  int   wi[AF_NCH];     // Write index
  int   ri;             // Read index
  float d[AF_NCH];      // Delay [ms]
  char *delaystr;
}af_delay_t;

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_delay_t* s = af->priv;
  switch(cmd){
  case AF_CONTROL_REINIT:{
    int i;
    struct mp_audio *in = arg;

    if (in->bps != 1 && in->bps != 2 && in->bps != 4) {
      MP_FATAL(af, "Sample format not supported\n");
      return AF_ERROR;
    }

    // Free prevous delay queues
    for(i=0;i<af->data->nch;i++)
      free(s->q[i]);

    mp_audio_force_interleaved_format(in);
    mp_audio_copy_config(af->data, in);

    // Allocate new delay queues
    for(i=0;i<af->data->nch;i++){
      s->q[i] = calloc(L,af->data->bps);
      if(NULL == s->q[i])
        MP_FATAL(af, "Out of memory\n");
    }

    if(AF_OK != af_from_ms(AF_NCH, s->d, s->wi, af->data->rate, 0.0, 1000.0))
      return AF_ERROR;
    s->ri = 0;
    for(i=0;i<AF_NCH;i++){
      MP_DBG(af, "Channel %i delayed by %0.3fms\n",
             i,MPCLAMP(s->d[i],0.0,1000.0));
      MP_TRACE(af, "Channel %i delayed by %i samples\n",
             i,s->wi[i]);
    }
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance* af)
{
  int i;

  for(i=0;i<AF_NCH;i++)
      free(((af_delay_t*)(af->priv))->q[i]);
}

static int filter_frame(struct af_instance *af, struct mp_audio *c)
{
  if (!c)
    return 0;
  af_delay_t*   s   = af->priv; // Setup for this instance
  int           nch = c->nch;    // Number of channels
  int           len = mp_audio_psize(c)/c->bps; // Number of sample in data chunk
  int           ri  = 0;
  int           ch,i;
  if (af_make_writeable(af, c) < 0) {
    talloc_free(c);
    return -1;
  }
  for(ch=0;ch<nch;ch++){
    switch(c->bps){
    case 1:{
      int8_t* a = c->planes[0];
      int8_t* q = s->q[ch];
      int wi = s->wi[ch];
      ri = s->ri;
      for(i=ch;i<len;i+=nch){
        q[wi] = a[i];
        a[i]  = q[ri];
        UPDATEQI(wi);
        UPDATEQI(ri);
      }
      s->wi[ch] = wi;
      break;
    }
    case 2:{
      int16_t* a = c->planes[0];
      int16_t* q = s->q[ch];
      int wi = s->wi[ch];
      ri = s->ri;
      for(i=ch;i<len;i+=nch){
        q[wi] = a[i];
        a[i]  = q[ri];
        UPDATEQI(wi);
        UPDATEQI(ri);
      }
      s->wi[ch] = wi;
      break;
    }
    case 4:{
      int32_t* a = c->planes[0];
      int32_t* q = s->q[ch];
      int wi = s->wi[ch];
      ri = s->ri;
      for(i=ch;i<len;i+=nch){
        q[wi] = a[i];
        a[i]  = q[ri];
        UPDATEQI(wi);
        UPDATEQI(ri);
      }
      s->wi[ch] = wi;
      break;
    }
    }
  }
  s->ri = ri;
  af_add_output_frame(af, c);
  return 0;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
    af->control=control;
    af->uninit=uninit;
    af->filter_frame = filter_frame;
    af_delay_t *s = af->priv;
    int n = 1;
    int i = 0;
    char* cl = s->delaystr;
    while(cl && n && i < AF_NCH ){
      sscanf(cl,"%f%n",&s->d[i],&n);
      if(n==0 || cl[n-1] == '\0')
        break;
      cl=&cl[n];
      if (*cl != ',')
          break;
      cl++;
      i++;
    }
    return AF_OK;
}

#define OPT_BASE_STRUCT af_delay_t
const struct af_info af_info_delay = {
    .info = "Delay audio filter",
    .name = "delay",
    .open = af_open,
    .priv_size = sizeof(af_delay_t),
    .options = (const struct m_option[]) {
        OPT_STRING("delays", delaystr, 0),
        {0}
    },
};
