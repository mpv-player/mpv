/*
 * This file is part of MPlayer.
 *
 * Original authors: Albeu, probably Arpi
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "osdep/io.h"

#include "core/mp_msg.h"
#include "stream.h"
#include "core/m_option.h"
#include "core/m_struct.h"

struct priv {
    int fd;
    bool close;
};

static struct stream_priv_s {
  char* filename;
  char *filename2;
} stream_priv_dflts = {
  NULL, NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  {"string", ST_OFF(filename), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"filename", ST_OFF(filename2), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "file",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  struct priv *p = s->priv;
  int r = read(p->fd,buffer,max_len);
  return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char* buffer, int len) {
  struct priv *p = s->priv;
  int r;
  int wr = 0;
  while (wr < len) {
    r = write(p->fd,buffer,len);
    if (r <= 0)
      return -1;
    wr += r;
    buffer += r;
  }
  return len;
}

static int seek(stream_t *s,int64_t newpos) {
  struct priv *p = s->priv;
  s->pos = newpos;
  if(lseek(p->fd,s->pos,SEEK_SET)<0) {
    return 0;
  }
  return 1;
}

static int seek_forward(stream_t *s,int64_t newpos) {
  if(newpos<s->pos){
    mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek backward in linear streams!\n");
    return 0;
  }
  while(s->pos<newpos){
    int len=s->fill_buffer(s,s->buffer,STREAM_BUFFER_SIZE);
    if(len<=0){ s->buf_pos=s->buf_len=0; break; } // EOF
    s->buf_pos=0;
    s->buf_len=len;
    s->pos+=len;
  }
  return 1;
}

static int control(stream_t *s, int cmd, void *arg) {
  struct priv *p = s->priv;
  switch(cmd) {
    case STREAM_CTRL_GET_SIZE: {
      off_t size;

      size = lseek(p->fd, 0, SEEK_END);
      lseek(p->fd, s->pos, SEEK_SET);
      if(size != (off_t)-1) {
        *(uint64_t*)arg = size;
        return 1;
      }
    }
  }
  return STREAM_UNSUPPORTED;
}

static void s_close(stream_t *s)
{
  struct priv *p = s->priv;
  if (p->close && p->fd >= 0)
    close(p->fd);
}

static int open_f(stream_t *stream,int mode, void* opts)
{
  int f;
  mode_t m = 0;
  int64_t len;
  unsigned char *filename;
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  struct priv *priv = talloc_ptrtype(stream, priv);
  *priv = (struct priv) { .fd = -1 };
  stream->priv = priv;

  if(mode == STREAM_READ)
    m = O_RDONLY;
  else if(mode == STREAM_WRITE)
    m = O_RDWR|O_CREAT|O_TRUNC;
  else {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[file] Unknown open mode %d\n",mode);
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }

  if(p->filename)
    filename = p->filename;
  else if(p->filename2)
    filename = p->filename2;
  else
    filename = NULL;
  if(!filename) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[file] No filename\n");
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

#if HAVE_DOS_PATHS
  // extract '/' from '/x:/path'
  if( filename[ 0 ] == '/' && filename[ 1 ] && filename[ 2 ] == ':' )
    filename++;
#endif

  m |= O_BINARY;

  if(!strcmp(filename,"-")){
    if(mode == STREAM_READ) {
      // read from stdin
      mp_tmsg(MSGT_OPEN,MSGL_INFO,"Reading from stdin...\n");
      f=0; // 0=stdin
#if HAVE_SETMODE
      setmode(fileno(stdin),O_BINARY);
#endif
    } else {
      mp_msg(MSGT_OPEN,MSGL_INFO,"Writing to stdout\n");
      f=1;
#if HAVE_SETMODE
      setmode(fileno(stdout),O_BINARY);
#endif
    }
    priv->fd = f;
    priv->close = false;
  } else {
      mode_t openmode = S_IRUSR|S_IWUSR;
#ifndef __MINGW32__
      openmode |= S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
#endif
      f=open(filename,m, openmode);
    if(f<0) {
        mp_tmsg(MSGT_OPEN, MSGL_ERR, "Cannot open file '%s': %s\n", filename,
                strerror(errno));
      m_struct_free(&stream_opts,opts);
      return STREAM_ERROR;
    }
#ifndef __MINGW32__
    struct stat st;
    if (fstat(f, &st) == 0 && S_ISDIR(st.st_mode)) {
      mp_tmsg(MSGT_OPEN,MSGL_ERR,"File is a directory: '%s'\n",filename);
      close(f);
      m_struct_free(&stream_opts,opts);
      return STREAM_ERROR;
    }
#endif
    priv->fd = f;
    priv->close = true;
  }

  len=lseek(f,0,SEEK_END); lseek(f,0,SEEK_SET);
#ifdef __MINGW32__
  // seeks on stdin incorrectly succeed on MinGW
  if(f==0)
    len = -1;
#endif
  stream->type = STREAMTYPE_FILE;
  if(len == -1 && mode == STREAM_READ) {
    stream->seek = seek_forward;
    stream->flags = MP_STREAM_SEEK_FW;
  } else if(len >= 0) {
    stream->seek = seek;
    stream->end_pos = len;
  }

  mp_msg(MSGT_OPEN,MSGL_V,"[file] File size is %"PRId64" bytes\n", (int64_t)len);

  stream->fill_buffer = fill_buffer;
  stream->write_buffer = write_buffer;
  stream->control = control;
  stream->read_chunk = 64*1024;
  stream->close = s_close;

  m_struct_free(&stream_opts,opts);
  return STREAM_OK;
}

const stream_info_t stream_info_file = {
  "file",
  open_f,
  { "file", "", NULL },
  &stream_opts,
  1 // Urls are an option string
};
