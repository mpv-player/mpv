/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Stream layer for TV Input, based on previous work from Albeu
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
#include "m_option.h"
#include "m_struct.h"

#include <stdio.h>

static struct stream_priv_s {
    /* if channels parameter exist here will be channel number otherwise - frequency */
    int input;
    char* channel;
} stream_priv_dflts = {
    -1,
    NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
static m_option_t stream_opts_fields[] = {
    {"hostname", ST_OFF(channel), CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"filename", ST_OFF(input), CONF_TYPE_INT, 0, 0 ,0, NULL},
    { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static struct m_struct_st stream_opts = {
    "tv",
    sizeof(struct stream_priv_s),
    &stream_priv_dflts,
    stream_opts_fields
};

static int
tv_stream_open (stream_t *stream, int mode, void *opts, int *file_format)
{
  extern char* tv_param_channel;
  extern int tv_param_input;
  struct stream_priv_s* p=(struct stream_priv_s*)opts;
  
  stream->type = STREAMTYPE_TV;
  *file_format =  DEMUXER_TYPE_TV;
  
  /* don't override input= option value if no input id is
     passed in tv:// url */
  if(p->input!=-1)
  tv_param_input=p->input;
  if (p->channel)
      tv_param_channel=strdup (p->channel);
  return STREAM_OK;
}

stream_info_t stream_info_tv = {
  "TV Input",
  "tv",
  "Benjamin Zores, Albeu",
  "",
  tv_stream_open, 			
  { "tv", NULL },
  &stream_opts,
  1
};
