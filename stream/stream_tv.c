/*
 * stream layer for TV Input, based on previous work from Albeu
 *
 * Copyright (C) 2006 Benjamin Zores
 * Original author: Albeu
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "stream.h"
#include "core/m_option.h"
#include "tv.h"

#include <stdio.h>

tv_param_t stream_tv_defaults = {
    NULL,          //freq
    NULL,          //channel
    "europe-east", //chanlist
    "pal",         //norm
    0,             //automute
    -1,            //normid
    NULL,          //device
    NULL,          //driver
    -1,            //width
    -1,            //height
    0,             //input, used in v4l and bttv
    -1,            //outfmt
    -1.0,          //fps
    NULL,          //channels
    0,             //noaudio;
    1,             //immediate;
    44100,         //audiorate;
    0,             //audio_id
    -1,            //amode
    -1,            //volume
    -1,            //bass
    -1,            //treble
    -1,            //balance
    -1,            //forcechan
    0,             //force_audio
    -1,            //buffer_size
    0,             //mjpeg
    2,             //decimation
    90,            //quality
    0,             //alsa
    NULL,          //adevice
    0,             //brightness
    0,             //contrast
    0,             //hue
    0,             //saturation
    -1,            //gain
    0,             //scan_autostart
    50,            //scan_threshold
    0.5,           //scan_period
};

#define OPT_BASE_STRUCT tv_param_t
static const m_option_t stream_opts_fields[] = {
    OPT_STRING("channel", channel, 0),
    OPT_INT("input", input, 0),
    {0}
};

static void
tv_stream_close (stream_t *stream)
{
}
static int
tv_stream_open (stream_t *stream, int mode)
{

  stream->type = STREAMTYPE_TV;
  stream->close=tv_stream_close;
  stream->demuxer = "tv";

  return STREAM_OK;
}

const stream_info_t stream_info_tv = {
  "tv",
  tv_stream_open,
  { "tv", NULL },
  .priv_size = sizeof(tv_param_t),
  .priv_defaults = &stream_tv_defaults,
  .options = stream_opts_fields,
  .url_options = {
        {"hostname", "channel"},
        {"filename", "input"},
        {0}
    },
};
