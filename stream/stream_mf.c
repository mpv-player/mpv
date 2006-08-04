/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Stream layer for multiple files input, based on previous work from Albeu
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "stream.h"
#include "libmpdemux/demuxer.h"

static int
mf_stream_open (stream_t *stream, int mode, void *opts, int *file_format)
{
  stream->type = STREAMTYPE_MF;
  *file_format = DEMUXER_TYPE_MF;
  
  return STREAM_OK;
}

stream_info_t stream_info_mf = {
  "Multiple files input",
  "mf",
  "Benjamin Zores, Albeu",
  "",
  mf_stream_open, 			
  { "mf", NULL },
  NULL,
  1
};
