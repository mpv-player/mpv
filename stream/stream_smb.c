/*
 * Original author: M. Tourne
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

#include <libsmbclient.h>
#include <unistd.h>

#include "common/msg.h"
#include "stream.h"
#include "options/m_option.h"

struct priv {
    int fd;
};

static void smb_auth_fn(const char *server, const char *share,
             char *workgroup, int wgmaxlen, char *username, int unmaxlen,
             char *password, int pwmaxlen)
{
  strncpy(workgroup, "LAN", wgmaxlen - 1);
}

static int control(stream_t *s, int cmd, void *arg) {
  struct priv *p = s->priv;
  switch(cmd) {
    case STREAM_CTRL_GET_SIZE: {
      off_t size = smbc_lseek(p->fd,0,SEEK_END);
      smbc_lseek(p->fd,s->pos,SEEK_SET);
      if(size != (off_t)-1) {
        *(int64_t *)arg = size;
        return 1;
      }
    }
    break;
  }
  return STREAM_UNSUPPORTED;
}

static int seek(stream_t *s,int64_t newpos) {
  struct priv *p = s->priv;
  if(smbc_lseek(p->fd,newpos,SEEK_SET)<0) {
    return 0;
  }
  return 1;
}

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  struct priv *p = s->priv;
  int r = smbc_read(p->fd,buffer,max_len);
  return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char* buffer, int len) {
  struct priv *p = s->priv;
  int r;
  int wr = 0;
  while (wr < len) {
    r = smbc_write(p->fd,buffer,len);
    if (r <= 0)
      return -1;
    wr += r;
    buffer += r;
  }
  return len;
}

static void close_f(stream_t *s){
  struct priv *p = s->priv;
  smbc_close(p->fd);
}

static int open_f (stream_t *stream)
{
  char *filename;
  int64_t len;
  int fd, err;

  struct priv *priv = talloc_zero(stream, struct priv);
  stream->priv = priv;

  filename = stream->url;

  bool write = stream->mode == STREAM_WRITE;
  mode_t m = write ? O_RDWR|O_CREAT|O_TRUNC : O_RDONLY;

  if(!filename) {
    MP_ERR(stream, "[smb] Bad url\n");
    return STREAM_ERROR;
  }

  err = smbc_init(smb_auth_fn, 1);
  if (err < 0) {
    MP_ERR(stream, "Cannot init the libsmbclient library: %d\n",err);
    return STREAM_ERROR;
  }

  fd = smbc_open(filename, m,0644);
  if (fd < 0) {
    MP_ERR(stream, "Could not open from LAN: '%s'\n", filename);
    return STREAM_ERROR;
  }

  len = 0;
  if(!write) {
    len = smbc_lseek(fd,0,SEEK_END);
    smbc_lseek (fd, 0, SEEK_SET);
  }
  if(len > 0 || write) {
    stream->seekable = true;
    stream->seek = seek;
  }
  priv->fd = fd;
  stream->fill_buffer = fill_buffer;
  stream->write_buffer = write_buffer;
  stream->close = close_f;
  stream->control = control;
  stream->read_chunk = 128 * 1024;
  stream->streaming = true;

  return STREAM_OK;
}

const stream_info_t stream_info_smb = {
    .name = "smb",
    .open = open_f,
    .protocols = (const char*const[]){"smb", NULL},
    .can_write = true, //who's gonna do that?
};
