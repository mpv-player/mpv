/*
 * Copyright (C) Joey Parrish
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

/*
 *   If you have a tivo with the vstream server installed, (and most tivo
 *   hackers do,) then you can connect to it and stream ty files using
 *   this module.  The url syntax is tivo://host/fsid or tivo://host/list
 *   to list the available recordings and their fsid's.
 *   This module depends on libvstream-client, which is available from
 *   http://armory.nicewarrior.org/projects/vstream-client .
 *
 */


#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"
#include "tcp.h"

#include <vstream-client.h>

void vstream_error(const char *format, ...) {
    char buf[1024];
    va_list va;
    va_start(va, format);
    vsnprintf(buf, 1024, format, va);
    va_end(va);
    mp_msg(MSGT_STREAM, MSGL_ERR, buf);
}

static struct stream_priv_s {
  char* host;
  char* fsid;
} stream_priv_dflts = {
  NULL,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  {"hostname", ST_OFF(host), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"filename", ST_OFF(fsid), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const struct m_struct_st stream_opts = {
  "vstream",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  struct stream_priv_s* p = (struct stream_priv_s*)s->priv;
  int len = vstream_load_chunk(p->fsid, buffer, max_len, s->pos);
  if (len <= 0) return 0;
  return len;
}

static int seek(stream_t *s,off_t newpos) {
  s->pos = newpos;
  return 1;
}

static int control(struct stream_st *s,int cmd,void* arg) {
  return STREAM_UNSUPPORTED;
}

static void close_s(struct stream_st *s) {
}

static int open_s(stream_t *stream, int mode, void* opts, int* file_format) {
  int f;
  struct stream_priv_s* p = (struct stream_priv_s*)opts;

  if(mode != STREAM_READ)
    return STREAM_UNSUPPORTED;

  if(!p->host) {
    mp_msg(MSGT_OPEN, MSGL_ERR, "We need a host name (ex: tivo://hostname/fsid)\n");
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }

  if(!p->fsid || strlen(p->fsid) == 0) {
    mp_msg(MSGT_OPEN, MSGL_ERR, "We need an fsid (ex: tivo://hostname/fsid)\n");
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }

  f = connect2Server(p->host, VSERVER_PORT, 1);

  if(f < 0) {
    mp_msg(MSGT_OPEN, MSGL_ERR, "Connection to %s failed\n", p->host);
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }
  stream->fd = f;

  vstream_set_socket_fd(f);

  if (!strcmp(p->fsid, "list")) {
    vstream_list_streams(0);
    return STREAM_ERROR;
  } else if (!strcmp(p->fsid, "llist")) {
    vstream_list_streams(1);
    return STREAM_ERROR;
  }

  if (vstream_start()) {
    mp_msg(MSGT_OPEN, MSGL_ERR, "Cryptic internal error #1\n");
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }
  if (vstream_startstream(p->fsid)) {
    mp_msg(MSGT_OPEN, MSGL_ERR, "Cryptic internal error #2\n");
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }
  
  stream->start_pos = 0;
  stream->end_pos = vstream_streamsize();
  mp_msg(MSGT_OPEN, MSGL_DBG2, "Tivo stream size is %d\n", stream->end_pos);

  stream->priv = p;
  stream->fill_buffer = fill_buffer;
  stream->control = control;
  stream->seek = seek;
  stream->close = close_s;
  stream->type = STREAMTYPE_VSTREAM;

  return STREAM_OK;
}

const stream_info_t stream_info_vstream = {
  "vstream client",
  "vstream",
  "Joey",
  "",
  open_s,
  { "tivo", NULL },
  &stream_opts,
  1 // Url is an option string
};
