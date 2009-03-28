/*
 * scaletempo audio filter
 *
 * scale tempo while maintaining pitch
 * (WSOLA technique with cross correlation)
 * inspired by SoundTouch library by Olli Parviainen
 *
 * basic algorithm
 *   - produce 'stride' output samples per loop
 *   - consume stride*scale input samples per loop
 *
 * to produce smoother transitions between strides, blend next overlap
 * samples from last stride with correlated samples of current input
 *
 * Copyright (c) 2007 Robert Juliano
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "af.h"
#include "libavutil/common.h"
#include "subopt-helper.h"
#include "help_mp.h"

// Data for specific instances of this filter
typedef struct af_scaletempo_s
{
  // stride
  float   scale;
  float   speed;
  float   frames_stride_scaled;
  float   frames_stride_error;
  int     bytes_per_frame;
  int     bytes_stride;
  float   bytes_stride_scaled;
  int     bytes_queue;
  int     bytes_queued;
  int     bytes_to_slide;
  int8_t* buf_queue;
  // overlap
  int     samples_overlap;
  int     samples_standing;
  int     bytes_overlap;
  int     bytes_standing;
  void*   buf_overlap;
  void*   table_blend;
  void    (*output_overlap)(struct af_scaletempo_s* s, void* out_buf, int bytes_off);
  // best overlap
  int     frames_search;
  int     num_channels;
  void*   buf_pre_corr;
  void*   table_window;
  int     (*best_overlap_offset)(struct af_scaletempo_s* s);
  // command line
  float   scale_nominal;
  float   ms_stride;
  float   percent_overlap;
  float   ms_search;
  short   speed_tempo;
  short   speed_pitch;
} af_scaletempo_t;

static int fill_queue(struct af_instance_s* af, af_data_t* data, int offset)
{
  af_scaletempo_t* s = af->setup;
  int bytes_in = data->len - offset;
  int offset_unchanged = offset;

  if (s->bytes_to_slide > 0) {
    if (s->bytes_to_slide < s->bytes_queued) {
      int bytes_move = s->bytes_queued - s->bytes_to_slide;
      memmove(s->buf_queue,
              s->buf_queue + s->bytes_to_slide,
              bytes_move);
      s->bytes_to_slide = 0;
      s->bytes_queued = bytes_move;
    } else {
      int bytes_skip;
      s->bytes_to_slide -= s->bytes_queued;
      bytes_skip = FFMIN(s->bytes_to_slide, bytes_in);
      s->bytes_queued = 0;
      s->bytes_to_slide -= bytes_skip;
      offset += bytes_skip;
      bytes_in -= bytes_skip;
    }
  }

  if (bytes_in > 0) {
    int bytes_copy = FFMIN(s->bytes_queue - s->bytes_queued, bytes_in);
    memcpy(s->buf_queue + s->bytes_queued,
           (int8_t*)data->audio + offset,
           bytes_copy);
    s->bytes_queued += bytes_copy;
    offset += bytes_copy;
  }

  return offset - offset_unchanged;
}

#define UNROLL_PADDING (4*4)

static int best_overlap_offset_float(af_scaletempo_t* s)
{
  float *pw, *po, *ppc, *search_start;
  float best_corr = INT_MIN;
  int best_off = 0;
  int i, off;

  pw  = s->table_window;
  po  = s->buf_overlap;
  po += s->num_channels;
  ppc = s->buf_pre_corr;
  for (i=s->num_channels; i<s->samples_overlap; i++) {
    *ppc++ = *pw++ * *po++;
  }

  search_start = (float*)s->buf_queue + s->num_channels;
  for (off=0; off<s->frames_search; off++) {
    float corr = 0;
    float* ps = search_start;
    ppc = s->buf_pre_corr;
    for (i=s->num_channels; i<s->samples_overlap; i++) {
      corr += *ppc++ * *ps++;
    }
    if (corr > best_corr) {
      best_corr = corr;
      best_off  = off;
    }
    search_start += s->num_channels;
  }

  return best_off * 4 * s->num_channels;
}

static int best_overlap_offset_s16(af_scaletempo_t* s)
{
  int32_t *pw, *ppc;
  int16_t *po, *search_start;
  int64_t best_corr = INT64_MIN;
  int best_off = 0;
  int off;
  long i;

  pw  = s->table_window;
  po  = s->buf_overlap;
  po += s->num_channels;
  ppc = s->buf_pre_corr;
  for (i=s->num_channels; i<s->samples_overlap; i++) {
    *ppc++ = ( *pw++ * *po++ ) >> 15;
  }

  search_start = (int16_t*)s->buf_queue + s->num_channels;
  for (off=0; off<s->frames_search; off++) {
    int64_t corr = 0;
    int16_t* ps = search_start;
    ppc = s->buf_pre_corr;
    ppc += s->samples_overlap - s->num_channels;
    ps  += s->samples_overlap - s->num_channels;
    i  = -(s->samples_overlap - s->num_channels);
    do {
      corr += ppc[i+0] * ps[i+0];
      corr += ppc[i+1] * ps[i+1];
      corr += ppc[i+2] * ps[i+2];
      corr += ppc[i+3] * ps[i+3];
      i += 4;
    } while (i < 0);
    if (corr > best_corr) {
      best_corr = corr;
      best_off  = off;
    }
    search_start += s->num_channels;
  }

  return best_off * 2 * s->num_channels;
}

static void output_overlap_float(af_scaletempo_t* s, void* buf_out,
				  int bytes_off)
{
  float* pout = buf_out;
  float* pb   = s->table_blend;
  float* po   = s->buf_overlap;
  float* pin  = (float*)(s->buf_queue + bytes_off);
  int i;
  for (i=0; i<s->samples_overlap; i++) {
    *pout++ = *po - *pb++ * ( *po - *pin++ ); po++;
  }
}
static void output_overlap_s16(af_scaletempo_t* s, void* buf_out,
			       int bytes_off)
{
  int16_t* pout = buf_out;
  int32_t* pb   = s->table_blend;
  int16_t* po   = s->buf_overlap;
  int16_t* pin  = (int16_t*)(s->buf_queue + bytes_off);
  int i;
  for (i=0; i<s->samples_overlap; i++) {
    *pout++ = *po - ( ( *pb++ * ( *po - *pin++ ) ) >> 16 ); po++;
  }
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_scaletempo_t* s = af->setup;
  int offset_in;
  int max_bytes_out;
  int8_t* pout;

  if (s->scale == 1.0) {
    return data;
  }

  // RESIZE_LOCAL_BUFFER - can't use macro
  max_bytes_out = ((int)(data->len / s->bytes_stride_scaled) + 1) * s->bytes_stride;
  if (max_bytes_out > af->data->len) {
    mp_msg(MSGT_AFILTER, MSGL_V, "[libaf] Reallocating memory in module %s, "
          "old len = %i, new len = %i\n",af->info->name,af->data->len,max_bytes_out);
    af->data->audio = realloc(af->data->audio, max_bytes_out);
    if (!af->data->audio) {
      mp_msg(MSGT_AFILTER, MSGL_FATAL, "[libaf] Could not allocate memory\n");
      return NULL;
    }
    af->data->len = max_bytes_out;
  }

  offset_in = fill_queue(af, data, 0);
  pout = af->data->audio;
  while (s->bytes_queued >= s->bytes_queue) {
    int ti;
    float tf;
    int bytes_off = 0;

    // output stride
    if (s->output_overlap) {
      if (s->best_overlap_offset)
        bytes_off = s->best_overlap_offset(s);
      s->output_overlap(s, pout, bytes_off);
    }
    memcpy(pout + s->bytes_overlap,
           s->buf_queue + bytes_off + s->bytes_overlap,
           s->bytes_standing);
    pout += s->bytes_stride;

    // input stride
    memcpy(s->buf_overlap,
           s->buf_queue + bytes_off + s->bytes_stride,
           s->bytes_overlap);
    tf = s->frames_stride_scaled + s->frames_stride_error;
    ti = (int)tf;
    s->frames_stride_error = tf - ti;
    s->bytes_to_slide = ti * s->bytes_per_frame;

    offset_in += fill_queue(af, data, offset_in);
  }

  // This filter can have a negative delay when scale > 1:
  // output corresponding to some length of input can be decided and written
  // after receiving only a part of that input.
  af->delay = s->bytes_queued - s->bytes_to_slide;

  data->audio = af->data->audio;
  data->len   = pout - (int8_t *)af->data->audio;
  return data;
}

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_scaletempo_t* s = af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:{
    af_data_t* data = (af_data_t*)arg;
    float srate = data->rate / 1000;
    int nch = data->nch;
    int bps;
    int use_int = 0;
    int frames_stride, frames_overlap;
    int i, j;

    mp_msg(MSGT_AFILTER, MSGL_V,
           "[scaletempo] %.3f speed * %.3f scale_nominal = %.3f\n",
           s->speed, s->scale_nominal, s->scale);

    if (s->scale == 1.0) {
      if (s->speed_tempo && s->speed_pitch)
        return AF_DETACH;
      memcpy(af->data, data, sizeof(af_data_t));
      return af_test_output(af, data);
    }

    af->data->rate = data->rate;
    af->data->nch  = data->nch;
    if ( data->format == AF_FORMAT_S16_LE
         || data->format == AF_FORMAT_S16_BE ) {
      use_int = 1;
      af->data->format  = AF_FORMAT_S16_NE;
      af->data->bps     = bps = 2;
    } else {
      af->data->format = AF_FORMAT_FLOAT_NE;
      af->data->bps    = bps = 4;
    }

    frames_stride           = srate * s->ms_stride;
    s->bytes_stride         = frames_stride * bps * nch;
    s->bytes_stride_scaled  = s->scale * s->bytes_stride;
    s->frames_stride_scaled = s->scale * frames_stride;
    s->frames_stride_error  = 0;
    af->mul = (double)s->bytes_stride / s->bytes_stride_scaled;

    frames_overlap = frames_stride * s->percent_overlap;
    if (frames_overlap <= 0) {
      s->bytes_standing   = s->bytes_stride;
      s->samples_standing = s->bytes_standing / bps;
      s->output_overlap   = NULL;
    } else {
      s->samples_overlap  = frames_overlap * nch;
      s->bytes_overlap    = frames_overlap * nch * bps;
      s->bytes_standing   = s->bytes_stride - s->bytes_overlap;
      s->samples_standing = s->bytes_standing / bps;
      s->buf_overlap      = realloc(s->buf_overlap, s->bytes_overlap);
      s->table_blend      = realloc(s->table_blend, s->bytes_overlap * 4);
      if(!s->buf_overlap || !s->table_blend) {
        mp_msg(MSGT_AFILTER, MSGL_FATAL, "[scaletempo] Out of memory\n");
        return AF_ERROR;
      }
      memset(s->buf_overlap, 0, s->bytes_overlap);
      if (use_int) {
        int32_t* pb = s->table_blend;
        int64_t blend = 0;
        for (i=0; i<frames_overlap; i++) {
          int32_t v = blend / frames_overlap;
          for (j=0; j<nch; j++) {
            *pb++ = v;
          }
          blend += 65536;  // 2^16
        }
        s->output_overlap = output_overlap_s16;
      } else {
        float* pb = s->table_blend;
        for (i=0; i<frames_overlap; i++) {
          float v = i / (float)frames_overlap;
          for (j=0; j<nch; j++) {
            *pb++ = v;
          }
        }
        s->output_overlap = output_overlap_float;
      }
    }

    s->frames_search = (frames_overlap > 1) ? srate * s->ms_search : 0;
    if (s->frames_search <= 0) {
      s->best_overlap_offset = NULL;
    } else {
      if (use_int) {
        int64_t t = frames_overlap;
        int32_t n = 8589934588LL / (t * t);  // 4 * (2^31 - 1) / t^2
        int32_t* pw;
        s->buf_pre_corr = realloc(s->buf_pre_corr, s->bytes_overlap * 2 + UNROLL_PADDING);
        s->table_window = realloc(s->table_window, s->bytes_overlap * 2 - nch * bps * 2);
        if(!s->buf_pre_corr || !s->table_window) {
          mp_msg(MSGT_AFILTER, MSGL_FATAL, "[scaletempo] Out of memory\n");
          return AF_ERROR;
        }
        memset((char *)s->buf_pre_corr + s->bytes_overlap * 2, 0, UNROLL_PADDING);
        pw = s->table_window;
        for (i=1; i<frames_overlap; i++) {
          int32_t v = ( i * (t - i) * n ) >> 15;
          for (j=0; j<nch; j++) {
            *pw++ = v;
          }
        }
        s->best_overlap_offset = best_overlap_offset_s16;
      } else {
        float* pw;
        s->buf_pre_corr = realloc(s->buf_pre_corr, s->bytes_overlap);
        s->table_window = realloc(s->table_window, s->bytes_overlap - nch * bps);
        if(!s->buf_pre_corr || !s->table_window) {
          mp_msg(MSGT_AFILTER, MSGL_FATAL, "[scaletempo] Out of memory\n");
          return AF_ERROR;
        }
        pw = s->table_window;
        for (i=1; i<frames_overlap; i++) {
          float v = i * (frames_overlap - i);
          for (j=0; j<nch; j++) {
            *pw++ = v;
          }
        }
        s->best_overlap_offset = best_overlap_offset_float;
      }
    }

    s->bytes_per_frame = bps * nch;
    s->num_channels    = nch;

    s->bytes_queue
      = (s->frames_search + frames_stride + frames_overlap) * bps * nch;
    s->buf_queue = realloc(s->buf_queue, s->bytes_queue + UNROLL_PADDING);
    if(!s->buf_queue) {
      mp_msg(MSGT_AFILTER, MSGL_FATAL, "[scaletempo] Out of memory\n");
      return AF_ERROR;
    }

    mp_msg (MSGT_AFILTER, MSGL_DBG2, "[scaletempo] "
            "%.2f stride_in, %i stride_out, %i standing, "
            "%i overlap, %i search, %i queue, %s mode\n",
            s->frames_stride_scaled,
            (int)(s->bytes_stride / nch / bps),
            (int)(s->bytes_standing / nch / bps),
            (int)(s->bytes_overlap / nch / bps),
            s->frames_search,
            (int)(s->bytes_queue / nch / bps),
            (use_int?"s16":"float"));

    return af_test_output(af, (af_data_t*)arg);
  }
  case AF_CONTROL_PLAYBACK_SPEED | AF_CONTROL_SET:{
    if (s->speed_tempo) {
      if (s->speed_pitch) {
        break;
      }
      s->speed = *(float*)arg;
      s->scale = s->speed * s->scale_nominal;
    } else {
      if (s->speed_pitch) {
        s->speed = 1 / *(float*)arg;
        s->scale = s->speed * s->scale_nominal;
        break;
      }
    }
    return AF_OK;
  }
  case AF_CONTROL_SCALETEMPO_AMOUNT | AF_CONTROL_SET:{
    s->scale = *(float*)arg;
    s->scale = s->speed * s->scale_nominal;
    return AF_OK;
  }
  case AF_CONTROL_SCALETEMPO_AMOUNT | AF_CONTROL_GET:
    *(float*)arg = s->scale;
    return AF_OK;
  case AF_CONTROL_COMMAND_LINE:{
    strarg_t speed = {};
    opt_t subopts[] = {
      {"scale",   OPT_ARG_FLOAT, &s->scale_nominal, NULL},
      {"stride",  OPT_ARG_FLOAT, &s->ms_stride, NULL},
      {"overlap", OPT_ARG_FLOAT, &s->percent_overlap, NULL},
      {"search",  OPT_ARG_FLOAT, &s->ms_search, NULL},
      {"speed",   OPT_ARG_STR,   &speed, NULL},
      {NULL},
    };
    if (subopt_parse(arg, subopts) != 0) {
      return AF_ERROR;
    }
    if (s->scale_nominal <= 0) {
      mp_msg(MSGT_AFILTER, MSGL_ERR, "[scaletempo] "
             MSGTR_ErrorParsingCommandLine ": " MSGTR_AF_ValueOutOfRange
             ": scale > 0\n");
      return AF_ERROR;
    }
    if (s->ms_stride <= 0) {
      mp_msg(MSGT_AFILTER, MSGL_ERR, "[scaletempo] "
             MSGTR_ErrorParsingCommandLine ": " MSGTR_AF_ValueOutOfRange
             ": stride > 0\n");
      return AF_ERROR;
    }
    if (s->percent_overlap < 0 || s->percent_overlap > 1) {
      mp_msg(MSGT_AFILTER, MSGL_ERR, "[scaletempo] "
             MSGTR_ErrorParsingCommandLine ": " MSGTR_AF_ValueOutOfRange
             ": 0 <= overlap <= 1\n");
      return AF_ERROR;
    }
    if (s->ms_search < 0) {
      mp_msg(MSGT_AFILTER, MSGL_ERR, "[scaletempo] "
             MSGTR_ErrorParsingCommandLine ": " MSGTR_AF_ValueOutOfRange
             ": search >= 0\n");
      return AF_ERROR;
    }
    if (speed.len > 0) {
      if (strcmp(speed.str, "pitch") == 0) {
        s->speed_tempo = 0;
        s->speed_pitch = 1;
      } else if (strcmp(speed.str, "tempo") == 0) {
        s->speed_tempo = 1;
        s->speed_pitch = 0;
      } else if (strcmp(speed.str, "none") == 0) {
        s->speed_tempo = 0;
        s->speed_pitch = 0;
      } else if (strcmp(speed.str, "both") == 0) {
        s->speed_tempo = 1;
        s->speed_pitch = 1;
      } else {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "[scaletempo] "
               MSGTR_ErrorParsingCommandLine ": " MSGTR_AF_ValueOutOfRange
               ": speed=[pitch|tempo|none|both]\n");
        return AF_ERROR;
      }
    }
    s->scale = s->speed * s->scale_nominal;
    mp_msg(MSGT_AFILTER, MSGL_DBG2, "[scaletempo] %6.3f scale, %6.2f stride, %6.2f overlap, %6.2f search, speed = %s\n", s->scale_nominal, s->ms_stride, s->percent_overlap, s->ms_search, (s->speed_tempo?(s->speed_pitch?"tempo and speed":"tempo"):(s->speed_pitch?"pitch":"none")));
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance_s* af)
{
  af_scaletempo_t* s = af->setup;
  free(af->data->audio);
  free(af->data);
  free(s->buf_queue);
  free(s->buf_overlap);
  free(s->buf_pre_corr);
  free(s->table_blend);
  free(s->table_window);
  free(af->setup);
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  af_scaletempo_t* s;

  af->control   = control;
  af->uninit    = uninit;
  af->play      = play;
  af->mul       = 1;
  af->data      = calloc(1,sizeof(af_data_t));
  af->setup     = calloc(1,sizeof(af_scaletempo_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;

  s = af->setup;
  s->scale = s->speed = s->scale_nominal = 1.0;
  s->speed_tempo = 1;
  s->speed_pitch = 0;
  s->ms_stride = 60;
  s->percent_overlap = .20;
  s->ms_search = 14;

  return AF_OK;
}

// Description of this filter
af_info_t af_info_scaletempo = {
  "Scale audio tempo while maintaining pitch",
  "scaletempo",
  "Robert Juliano",
  "",
  AF_FLAGS_REENTRANT,
  af_open
};
