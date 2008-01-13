
#include "config.h"

#include <libsmbclient.h>
#include <unistd.h>

#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"

static struct stream_priv_s {
} stream_priv_dflts = {
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
// URL definition
static const m_option_t stream_opts_fields[] = {
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const struct m_struct_st stream_opts = {
  "smb",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static char smb_password[15];
static char smb_username[15];

static void smb_auth_fn(const char *server, const char *share,
             char *workgroup, int wgmaxlen, char *username, int unmaxlen,
	     char *password, int pwmaxlen)
{
  char temp[128];
  
  strcpy(temp, "LAN");				  
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
  
  if (temp[0]) strncpy(workgroup, temp, wgmaxlen - 1);
  
  strcpy(temp, smb_username); 
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
  
  if (temp[0]) strncpy(username, temp, unmaxlen - 1);
						      
  strcpy(temp, smb_password); 
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
								
   if (temp[0]) strncpy(password, temp, pwmaxlen - 1);
}

static int control(stream_t *s, int cmd, void *arg) {
  switch(cmd) {
    case STREAM_CTRL_GET_SIZE: {
      off_t size = smbc_lseek(s->fd,0,SEEK_END);
      smbc_lseek(s->fd,s->pos,SEEK_SET);
      if(size != (off_t)-1) {
        *((off_t*)arg) = size;
        return 1;
      }
    }
  }
  return STREAM_UNSUPPORTED;
}

static int seek(stream_t *s,off_t newpos) {
  s->pos = newpos;
  if(smbc_lseek(s->fd,s->pos,SEEK_SET)<0) {
    s->eof=1;
    return 0;
  }
  return 1;
}

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  int r = smbc_read(s->fd,buffer,max_len);
  return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char* buffer, int len) {
  int r = smbc_write(s->fd,buffer,len);
  return (r <= 0) ? -1 : r;
}

static void close_f(stream_t *s){
  smbc_close(s->fd);
}

static int open_f (stream_t *stream, int mode, void *opts, int* file_format) {
  struct stream_priv_s *p = (struct stream_priv_s*)opts;
  char *filename;
  mode_t m = 0;
  off_t len;
  int fd, err;
  
  filename = stream->url;
  
  if(mode == STREAM_READ)
    m = O_RDONLY;
  else if (mode == STREAM_WRITE) //who's gonna do that ?
    m = O_RDWR|O_CREAT|O_TRUNC;
  else {
    mp_msg(MSGT_OPEN, MSGL_ERR, "[smb] Unknown open mode %d\n", mode);
    m_struct_free (&stream_opts, opts);
    return STREAM_UNSUPPORTED;
  }
  
  if(!filename) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[smb] Bad url\n");
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }
  
  err = smbc_init(smb_auth_fn, 1);
  if (err < 0) {
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBInitError,err);
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }
  
  fd = smbc_open(filename, m,0644);
  if (fd < 0) {	
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBFileNotFound, filename);
    m_struct_free(&stream_opts, opts);
    return STREAM_ERROR;
  }
  
  stream->flags = mode;
  len = 0;
  if(mode == STREAM_READ) {
    len = smbc_lseek(fd,0,SEEK_END);
    smbc_lseek (fd, 0, SEEK_SET);
  }
  if(len > 0 || mode == STREAM_WRITE) {
    stream->flags |= STREAM_SEEK;
    stream->seek = seek;
    if(mode == STREAM_READ) stream->end_pos = len;
  }
  stream->type = STREAMTYPE_SMB;
  stream->fd = fd;
  stream->fill_buffer = fill_buffer;
  stream->write_buffer = write_buffer;
  stream->close = close_f;
  stream->control = control;
  
  m_struct_free(&stream_opts, opts);
  return STREAM_OK;
}

const stream_info_t stream_info_smb = {
  "Server Message Block",
  "smb",
  "M. Tourne",
  "based on the code from 'a bulgarian' (one says)",
  open_f,
  {"smb", NULL},
  &stream_opts,
  0 //Url is an option string
};
