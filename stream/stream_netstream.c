/*
 * Copyright (C) Alban Bedel - 04/2003
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
 *  Net stream allow you to access MPlayer stream accross a tcp
 *  connection.
 *  Note that at least mf and tv use a dummy stream (they are
 *  implemented at the demuxer level) so you won't be able to
 *  access those :(( but dvd, vcd and so on should work perfectly
 *  (if you have the bandwidth ;)
 *   A simple server is in TOOLS/netstream.
 *
 */


#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

#if !HAVE_WINSOCK2_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif

#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"
#include "libavutil/common.h"
#include "mpbswap.h"

#include "network.h"
#include "stream_netstream.h"
#include "tcp.h"

static struct stream_priv_s {
  char* host;
  int port;
  char* url;
} stream_priv_dflts = {
  NULL,
  10000,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  {"hostname", ST_OFF(host), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"port", ST_OFF(port), CONF_TYPE_INT, M_OPT_MIN, 1 ,0, NULL},
  {"filename", ST_OFF(url), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "netstream",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

//// When the cache is running we need a lock as
//// fill_buffer is called from another proccess
static int lock_fd(int fd) {
#if !HAVE_WINSOCK2_H
  struct flock lock;

  memset(&lock,0,sizeof(struct flock));
  lock.l_type = F_WRLCK;

  mp_msg(MSGT_STREAM,MSGL_DBG2, "Lock (%d)\n",getpid());
  do {
    if(fcntl(fd,F_SETLKW,&lock)) {
      if(errno == EAGAIN) continue;
      mp_msg(MSGT_STREAM,MSGL_ERR, "Failed to get the lock: %s\n",
	     strerror(errno));
      return 0;
    }
  } while(0);
  mp_msg(MSGT_STREAM,MSGL_DBG2, "Locked (%d)\n",getpid());
#else
printf("FIXME? should lock here\n");
#endif
  return 1;
}

static int unlock_fd(int fd) {
#if !HAVE_WINSOCK2_H
  struct flock lock;

  memset(&lock,0,sizeof(struct flock));
  lock.l_type = F_UNLCK;

  mp_msg(MSGT_STREAM,MSGL_DBG2, "Unlock (%d)\n",getpid());
  if(fcntl(fd,F_SETLK,&lock)) {
    mp_msg(MSGT_STREAM,MSGL_ERR, "Failed to release the lock: %s\n",
	   strerror(errno));
    return 0;
  }
#else
printf("FIXME? should unlock here\n");
#endif
  return 1;
}

static mp_net_stream_packet_t* send_net_stream_cmd(stream_t *s,uint16_t cmd,char* data,int len) {
  mp_net_stream_packet_t* pack;

  // Cache is enabled : lock
  if(s->cache_data && !lock_fd(s->fd))
    return NULL;
  // Send a command
  if(!write_packet(s->fd,cmd,data,len)) {
    if(s->cache_data) unlock_fd(s->fd);
    return 0;
  }
  // Read the response
  pack = read_packet(s->fd);
  // Now we can unlock
  if(s->cache_data) unlock_fd(s->fd);

  if(!pack)
    return NULL;

  switch(pack->cmd) {
  case NET_STREAM_OK:
    return pack;
  case NET_STREAM_ERROR:
    if(pack->len > sizeof(mp_net_stream_packet_t))
      mp_msg(MSGT_STREAM,MSGL_ERR, "Fill buffer failed: %s\n",pack->data);
    else
      mp_msg(MSGT_STREAM,MSGL_ERR, "Fill buffer failed\n");
    free(pack);
    return NULL;
  }

  mp_msg(MSGT_STREAM,MSGL_ERR, "Unknown response to %d: %d\n",cmd,pack->cmd);
  free(pack);
  return NULL;
}

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  uint16_t len = le2me_16(max_len);
  mp_net_stream_packet_t* pack;

  pack = send_net_stream_cmd(s,NET_STREAM_FILL_BUFFER,(char*)&len,2);
  if(!pack) {
    return -1;
  }
  len = pack->len - sizeof(mp_net_stream_packet_t);
  if(len > max_len) {
    mp_msg(MSGT_STREAM,MSGL_ERR, "Got a too big a packet %d / %d\n",len,max_len);
    free(pack);
    return 0;
  }
  if(len > 0)
    memcpy(buffer,pack->data,len);
  free(pack);
  return len;
}


static int seek(stream_t *s,off_t newpos) {
  uint64_t pos = le2me_64((uint64_t)newpos);
  mp_net_stream_packet_t* pack;

  pack = send_net_stream_cmd(s,NET_STREAM_SEEK,(char*)&pos,8);
  if(!pack) {
    return 0;
  }
  s->pos = newpos;
  free(pack);
  return 1;
}

static int net_stream_reset(struct stream *s) {
  mp_net_stream_packet_t* pack;

  pack = send_net_stream_cmd(s,NET_STREAM_RESET,NULL,0);
  if(!pack) {
    return 0;
  }
  free(pack);
  return 1;
}

static int control(struct stream *s,int cmd,void* arg) {
  switch(cmd) {
  case STREAM_CTRL_RESET:
    return net_stream_reset(s);
  }
  return STREAM_UNSUPPORTED;
}

static void close_s(struct stream *s) {
  mp_net_stream_packet_t* pack;

  pack = send_net_stream_cmd(s,NET_STREAM_CLOSE,NULL,0);
  if(pack)
    free(pack);
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  int f;
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  mp_net_stream_packet_t* pack;
  mp_net_stream_opened_t* opened;

  if(mode != STREAM_READ)
    return STREAM_UNSUPPORTED;

  if(!p->host) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "We need an host name (ex: mpst://server.net/cdda://5)\n");
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  if(!p->url || strlen(p->url) == 0) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "We need a remote url (ex: mpst://server.net/cdda://5)\n");
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

  f = connect2Server(p->host,p->port,1);
  if(f < 0) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "Connection to %s:%d failed\n",p->host,p->port);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  stream->fd = f;
  /// Now send an open command
  pack = send_net_stream_cmd(stream,NET_STREAM_OPEN,p->url,strlen(p->url) + 1);
  if(!pack) {
    goto error;
  }

  if(pack->len != sizeof(mp_net_stream_packet_t) +
     sizeof(mp_net_stream_opened_t)) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "Invalid open response packet len (%d bytes)\n",pack->len);
    free(pack);
    goto error;
  }

  opened = (mp_net_stream_opened_t*)pack->data;
  net_stream_opened_2_me(opened);

  *file_format = opened->file_format;
  stream->flags = opened->flags;
  stream->sector_size = opened->sector_size;
  stream->start_pos = opened->start_pos;
  stream->end_pos = opened->end_pos;

  stream->fill_buffer = fill_buffer;
  stream->control = control;
  if(stream->flags & MP_STREAM_SEEK)
    stream->seek = seek;
  stream->close = close_s;

  free(pack);
  m_struct_free(&stream_opts,opts);

  return STREAM_OK;

  error:
  closesocket(f);
  m_struct_free(&stream_opts,opts);
  return STREAM_ERROR;
}

const stream_info_t stream_info_netstream = {
  "Net stream",
  "netstream",
  "Albeu",
  "",
  open_s,
  { "mpst",NULL },
  &stream_opts,
  1 // Url is an option string
};
