
#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"

static struct stream_priv_s {
  char* filename;
  char *filename2;
} stream_priv_dflts = {
  NULL, NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static m_option_t stream_opts_fields[] = {
  {"string", ST_OFF(filename), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"filename", ST_OFF(filename2), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static struct m_struct_st stream_opts = {
  "file",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};  

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  int r = read(s->fd,buffer,max_len);
  return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char* buffer, int len) {
  int r = write(s->fd,buffer,len);
  return (r <= 0) ? -1 : r;
}

static int seek(stream_t *s,off_t newpos) {
  s->pos = newpos;
  if(lseek(s->fd,s->pos,SEEK_SET)<0) {
    s->eof=1;
    return 0;
  }
  return 1;
}

static int seek_forward(stream_t *s,off_t newpos) {
  if(newpos<s->pos){
    mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek backward in linear streams!\n");
    return 0;
  }
  while(s->pos<newpos){
    int len=s->fill_buffer(s,s->buffer,STREAM_BUFFER_SIZE);
    if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; break; } // EOF
    s->buf_pos=0;
    s->buf_len=len;
    s->pos+=len;
  }
  return 1;
}

static int control(stream_t *s, int cmd, void *arg) {
  switch(cmd) {
    case STREAM_CTRL_GET_SIZE: {
      off_t size;

      size = lseek(s->fd, 0, SEEK_END);
      lseek(s->fd, s->pos, SEEK_SET);
      if(size != (off_t)-1) {
        *((off_t*)arg) = size;
        return 1;
      }
    }
  }
  return STREAM_UNSUPORTED;
}

static int open_f(stream_t *stream,int mode, void* opts, int* file_format) {
  int f;
  mode_t m = 0;
  off_t len;
  unsigned char *filename;
  struct stream_priv_s* p = (struct stream_priv_s*)opts;

  if(mode == STREAM_READ)
    m = O_RDONLY;
  else if(mode == STREAM_WRITE)
    m = O_RDWR|O_CREAT|O_TRUNC;
  else {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[file] Unknown open mode %d\n",mode);
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPORTED;
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

#if defined(__CYGWIN__)|| defined(__MINGW32__)
  m |= O_BINARY;
#endif    

  if(!strcmp(filename,"-")){
    if(mode == STREAM_READ) {
      // read from stdin
      mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_ReadSTDIN);
      f=0; // 0=stdin
#ifdef __MINGW32__
	  setmode(fileno(stdin),O_BINARY);
#endif
    } else {
      mp_msg(MSGT_OPEN,MSGL_INFO,"Writing to stdout\n");
      f=1;
#ifdef __MINGW32__
	  setmode(fileno(stdout),O_BINARY);
#endif
    }
  } else {
    if(mode == STREAM_READ)
      f=open(filename,m);
    else {
      mode_t openmode = S_IRUSR|S_IWUSR;
#ifndef __MINGW32__
      openmode |= S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
#endif
      f=open(filename,m, openmode);
    }
    if(f<0) {
      mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_FileNotFound,filename);
      m_struct_free(&stream_opts,opts);
      return STREAM_ERROR;
    }
  }

  len=lseek(f,0,SEEK_END); lseek(f,0,SEEK_SET);
#ifdef __MINGW32__
  if(f==0 || len == -1) {
#else
  if(len == -1) {
#endif
    if(mode == STREAM_READ) stream->seek = seek_forward;
    stream->type = STREAMTYPE_STREAM; // Must be move to STREAMTYPE_FILE
    stream->flags |= STREAM_SEEK_FW;
  } else if(len >= 0) {
    stream->seek = seek;
    stream->end_pos = len;
    stream->type = STREAMTYPE_FILE;
  }

  mp_msg(MSGT_OPEN,MSGL_V,"[file] File size is %"PRId64" bytes\n", (int64_t)len);

  stream->fd = f;
  stream->fill_buffer = fill_buffer;
  stream->write_buffer = write_buffer;
  stream->control = control;

  m_struct_free(&stream_opts,opts);
  return STREAM_OK;
}

stream_info_t stream_info_file = {
  "File",
  "file",
  "Albeu",
  "based on the code from ??? (probably Arpi)",
  open_f,
  { "file", "", NULL },
  &stream_opts,
  1 // Urls are an option string
};
