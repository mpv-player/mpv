/*
 * Demuxer for avisynth
 * Copyright (c) 2005 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * Avisynth C Interface Version 0.20
 * Copyright 2003 Kevin Atkinson
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_DEMUX_AVS_H
#define MPLAYER_DEMUX_AVS_H

#include <stdint.h>
#include "loader/wine/windef.h"

enum { AVISYNTH_INTERFACE_VERSION = 2 };

enum
{
  AVS_SAMPLE_INT8  = 1<<0,
  AVS_SAMPLE_INT16 = 1<<1,
  AVS_SAMPLE_INT24 = 1<<2,
  AVS_SAMPLE_INT32 = 1<<3,
  AVS_SAMPLE_FLOAT = 1<<4
};

enum
{
  AVS_PLANAR_Y=1<<0,
  AVS_PLANAR_U=1<<1,
  AVS_PLANAR_V=1<<2,
  AVS_PLANAR_ALIGNED=1<<3,
  AVS_PLANAR_Y_ALIGNED=AVS_PLANAR_Y|AVS_PLANAR_ALIGNED,
  AVS_PLANAR_U_ALIGNED=AVS_PLANAR_U|AVS_PLANAR_ALIGNED,
  AVS_PLANAR_V_ALIGNED=AVS_PLANAR_V|AVS_PLANAR_ALIGNED
};

// Colorspace properties.
enum
{
  AVS_CS_BGR = 1<<28,
  AVS_CS_YUV = 1<<29,
  AVS_CS_INTERLEAVED = 1<<30,
  AVS_CS_PLANAR = 1<<31
};

// Specific colorformats
enum
{
  AVS_CS_UNKNOWN = 0,
  AVS_CS_BGR24 = 1<<0 | AVS_CS_BGR | AVS_CS_INTERLEAVED,
  AVS_CS_BGR32 = 1<<1 | AVS_CS_BGR | AVS_CS_INTERLEAVED,
  AVS_CS_YUY2 = 1<<2 | AVS_CS_YUV | AVS_CS_INTERLEAVED,
  AVS_CS_YV12 = 1<<3 | AVS_CS_YUV | AVS_CS_PLANAR,  // y-v-u, planar
  AVS_CS_I420 = 1<<4 | AVS_CS_YUV | AVS_CS_PLANAR,  // y-u-v, planar
  AVS_CS_IYUV = 1<<4 | AVS_CS_YUV | AVS_CS_PLANAR  // same as above
};

typedef struct AVS_Clip AVS_Clip;
typedef struct AVS_ScriptEnvironment AVS_ScriptEnvironment;

typedef struct AVS_Value AVS_Value;
struct AVS_Value {
  short type;  // 'a'rray, 'c'lip, 'b'ool, 'i'nt, 'f'loat, 's'tring, 'v'oid, or 'l'ong
               // for some function e'rror
  short array_size;
  union {
    void * clip; // do not use directly, use avs_take_clip
    char boolean;
    int integer;
    float floating_pt;
    const char * string;
    const AVS_Value * array;
  } d;
};

// AVS_VideoInfo is layed out identicly to VideoInfo
typedef struct AVS_VideoInfo {
  int width, height;    // width=0 means no video
  unsigned fps_numerator, fps_denominator;
  int num_frames;

  int pixel_type;

  int audio_samples_per_second;   // 0 means no audio
  int sample_type;
  uint64_t num_audio_samples;
  int nchannels;

  // Imagetype properties

  int image_type;
} AVS_VideoInfo;

typedef struct AVS_VideoFrameBuffer {
  BYTE * data;
  int data_size;
  // sequence_number is incremented every time the buffer is changed, so
  // that stale views can tell they're no longer valid.
  long sequence_number;

  long refcount;
} AVS_VideoFrameBuffer;

typedef struct AVS_VideoFrame {
  int refcount;
  AVS_VideoFrameBuffer * vfb;
  int offset, pitch, row_size, height, offsetU, offsetV, pitchUV;  // U&V offsets are from top of picture.
} AVS_VideoFrame;

static inline AVS_Value avs_new_value_string(const char * v0)
{ AVS_Value v; v.type = 's'; v.d.string = v0; return v; }

static inline AVS_Value avs_new_value_array(AVS_Value * v0, int size)
{ AVS_Value v; v.type = 'a'; v.d.array = v0; v.array_size = size; return v; }


static inline int avs_is_error(AVS_Value v) { return v.type == 'e'; }
static inline int avs_is_clip(AVS_Value v) { return v.type == 'c'; }
static inline int avs_is_string(AVS_Value v) { return v.type == 's'; }
static inline int avs_has_video(const AVS_VideoInfo * p) { return p->width != 0; }
static inline int avs_has_audio(const AVS_VideoInfo * p) { return p->audio_samples_per_second != 0; }

static inline const char * avs_as_string(AVS_Value v)
{ return avs_is_error(v) || avs_is_string(v) ? v.d.string : 0; }

/* Color spaces */
static inline int avs_is_rgb(const AVS_VideoInfo * p)
{ return p->pixel_type & AVS_CS_BGR; }

static inline int avs_is_rgb24(const AVS_VideoInfo * p)
{ return (p->pixel_type&AVS_CS_BGR24)==AVS_CS_BGR24; } // Clear out additional properties

static inline int avs_is_rgb32(const AVS_VideoInfo * p)
{ return (p->pixel_type & AVS_CS_BGR32) == AVS_CS_BGR32 ; }

static inline int avs_is_yuy(const AVS_VideoInfo * p)
{ return p->pixel_type & AVS_CS_YUV; }

static inline int avs_is_yuy2(const AVS_VideoInfo * p)
{ return (p->pixel_type & AVS_CS_YUY2) == AVS_CS_YUY2; }

static inline int avs_is_yv12(const AVS_VideoInfo * p)
{ return ((p->pixel_type & AVS_CS_YV12) == AVS_CS_YV12)||((p->pixel_type & AVS_CS_I420) == AVS_CS_I420); }

static inline int avs_bits_per_pixel(const AVS_VideoInfo * p)
{
  switch (p->pixel_type) {
      case AVS_CS_BGR24: return 24;
      case AVS_CS_BGR32: return 32;
      case AVS_CS_YUY2:  return 16;
      case AVS_CS_YV12:
      case AVS_CS_I420:  return 12;
      default:           return 0;
    }
}

#endif /* MPLAYER_DEMUX_AVS_H */
