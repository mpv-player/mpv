/*
 * stream layer for TV Input, based on previous work from Albeu
 *
 * Copyright (C) 2006 Benjamin Zores
 * Original author: Albeu
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "stream.h"
#include "options/m_option.h"
#include "tv.h"

#include <stdio.h>

#define OPT_BASE_STRUCT struct tv_stream_params
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
tv_stream_open (stream_t *stream)
{

  stream->type = STREAMTYPE_TV;
  stream->close=tv_stream_close;
  stream->demuxer = "tv";
  stream->allow_caching = false;

  return STREAM_OK;
}

const stream_info_t stream_info_tv = {
  .name = "tv",
  .open = tv_stream_open,
  .protocols = (const char*const[]){ "tv", NULL },
  .priv_size = sizeof(tv_param_t),
  .priv_defaults = &(const struct tv_stream_params){
      .input = -1,
  },
  .options = stream_opts_fields,
  .url_options = (const char*const[]){
        "hostname=channel",
        "filename=input",
        NULL
    },
};
