/*
 * stream layer for multiple files input, based on previous work from Albeu
 *
 * Copyright (C) 2006 Benjamin Zores
 * Original author: Albeu
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "stream.h"

static int
mf_stream_open (stream_t *stream)
{
  stream->demuxer = "mf";
  stream->allow_caching = false;

  return STREAM_OK;
}

const stream_info_t stream_info_mf = {
    .name = "mf",
    .open = mf_stream_open,
    .protocols = (const char*const[]){ "mf", NULL },
};
