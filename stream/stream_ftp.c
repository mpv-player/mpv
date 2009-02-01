
#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#if !HAVE_WINSOCK2_H
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#include "mp_msg.h"
#include "network.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"
#include "tcp.h"

static struct stream_priv_s {
  char* user;
  char* pass;
  char* host;
  int port;
  char* filename;

  char *cput,*cget;
  int handle;
  int cavail,cleft;
  char *buf;
} stream_priv_dflts = {
  "anonymous","no@spam",
  NULL,
  21,
  NULL,
  NULL,
  NULL,

  0,
  0,0,
  NULL
};

#define BUFSIZE 2048

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  {"username", ST_OFF(user), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"password", ST_OFF(pass), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"hostname", ST_OFF(host), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  {"port", ST_OFF(port), CONF_TYPE_INT, 0, 0 ,65635, NULL},
  {"filename", ST_OFF(filename), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "ftp",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

#define TELNET_IAC      255             /* interpret as command: */
#define TELNET_IP       244             /* interrupt process--permanently */
#define TELNET_SYNCH    242             /* for telfunc calls */

// Check if there is something to read on a fd. This avoid hanging
// forever if the network stop responding.
static int fd_can_read(int fd,int timeout) {
  fd_set fds;
  struct timeval tv;

  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  
  return select(fd+1, &fds, NULL, NULL, &tv) > 0;
}

/*
 * read a line of text
 *
 * return -1 on error or bytecount
 */
static int readline(char *buf,int max,struct stream_priv_s *ctl)
{
    int x,retval = 0;
    char *end,*bp=buf;
    int eof = 0;
 
    do {
      if (ctl->cavail > 0) {
	x = (max >= ctl->cavail) ? ctl->cavail : max-1;
	end = memccpy(bp,ctl->cget,'\n',x);
	if (end != NULL)
	  x = end - bp;
	retval += x;
	bp += x;
	*bp = '\0';
	max -= x;
	ctl->cget += x;
	ctl->cavail -= x;
	if (end != NULL) {
	  bp -= 2;
	  if (strcmp(bp,"\r\n") == 0) {
	    *bp++ = '\n';
	    *bp++ = '\0';
	    --retval;
	  }
	  break;
	}
      }
      if (max == 1) {
	*buf = '\0';
	break;
      }
      if (ctl->cput == ctl->cget) {
	ctl->cput = ctl->cget = ctl->buf;
	ctl->cavail = 0;
	ctl->cleft = BUFSIZE;
      }
      if(eof) {
	if (retval == 0)
	  retval = -1;
	break;
      }

      if(!fd_can_read(ctl->handle, 15)) {
        mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] read timed out\n");
        retval = -1;
        break;
      }

      if ((x = recv(ctl->handle,ctl->cput,ctl->cleft,0)) == -1) {
	mp_msg(MSGT_STREAM,MSGL_ERR, "[ftp] read error: %s\n",strerror(errno));
	retval = -1;
	break;
      }
      if (x == 0)
	eof = 1;
      ctl->cleft -= x;
      ctl->cavail += x;
      ctl->cput += x;
    } while (1);
    
    return retval;
}

/*
 * read a response from the server
 *
 * return 0 if first char doesn't match
 * return 1 if first char matches
 */
static int readresp(struct stream_priv_s* ctl,char* rsp)
{
    static char response[256];
    char match[5];
    int r;

    if (readline(response,256,ctl) == -1)
      return 0;
 
    r = atoi(response)/100;
    if(rsp) strcpy(rsp,response);

    mp_msg(MSGT_STREAM,MSGL_V, "[ftp] < %s",response);

    if (response[3] == '-') {
      strncpy(match,response,3);
      match[3] = ' ';
      match[4] = '\0';
      do {
	if (readline(response,256,ctl) == -1) {
	  mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] Control socket read failed\n");
	  return 0;
	}
	mp_msg(MSGT_OPEN,MSGL_V, "[ftp] < %s",response);
      }	while (strncmp(response,match,4));
    }
    return r;
}


static int FtpSendCmd(const char *cmd, struct stream_priv_s *nControl,char* rsp)
{
  int l = strlen(cmd);
  int hascrlf = cmd[l - 2] == '\r' && cmd[l - 1] == '\n';

  if(hascrlf && l == 2) mp_msg(MSGT_STREAM,MSGL_V, "\n");
  else mp_msg(MSGT_STREAM,MSGL_V, "[ftp] > %s",cmd);
  while(l > 0) {
    int s = send(nControl->handle,cmd,l,0);

    if(s <= 0) {
      mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] write error: %s\n",strerror(errno));
      return 0;
    }
    
    cmd += s;
    l -= s;
  }
    
  if (hascrlf)  
    return readresp(nControl,rsp);
  else
    return FtpSendCmd("\r\n", nControl, rsp);
}

static int FtpOpenPort(struct stream_priv_s* p) {
  int resp,fd;
  char rsp_txt[256];
  char* par,str[128];
  int num[6];

  resp = FtpSendCmd("PASV",p,rsp_txt);
  if(resp != 2) {
    mp_msg(MSGT_OPEN,MSGL_WARN, "[ftp] command 'PASV' failed: %s\n",rsp_txt);
    return 0;
  }
  
  par = strchr(rsp_txt,'(');
  
  if(!par || !par[0] || !par[1]) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] invalid server response: %s ??\n",rsp_txt);
    return 0;
  }

  sscanf(par+1,"%u,%u,%u,%u,%u,%u",&num[0],&num[1],&num[2],
	 &num[3],&num[4],&num[5]);
  snprintf(str,127,"%d.%d.%d.%d",num[0],num[1],num[2],num[3]);
  fd = connect2Server(str,(num[4]<<8)+num[5],0);

  if(fd < 0)
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] failed to create data connection\n");

  return fd;
}

static int FtpOpenData(stream_t* s,size_t newpos) {
  struct stream_priv_s* p = s->priv;
  int resp;
  char str[256],rsp_txt[256];

  // Open a new connection
  s->fd = FtpOpenPort(p);

  if(s->fd < 0) return 0;

  if(newpos > 0) {
    snprintf(str,255,"REST %"PRId64, (int64_t)newpos);

    resp = FtpSendCmd(str,p,rsp_txt);
    if(resp != 3) {
      mp_msg(MSGT_OPEN,MSGL_WARN, "[ftp] command '%s' failed: %s\n",str,rsp_txt);
      newpos = 0;
    }
  }

  // Get the file
  snprintf(str,255,"RETR %s",p->filename);
  resp = FtpSendCmd(str,p,rsp_txt);

  if(resp != 1) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] command '%s' failed: %s\n",str,rsp_txt);
    return 0;
  }

  s->pos = newpos;
  return 1;
}

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  int r;

  if(s->fd < 0 && !FtpOpenData(s,s->pos))
    return -1;
  
  if(!fd_can_read(s->fd, 15)) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] read timed out\n");
    return -1;
  }

  r = recv(s->fd,buffer,max_len,0);
  return (r <= 0) ? -1 : r;
}

static int seek(stream_t *s,off_t newpos) {
  struct stream_priv_s* p = s->priv;
  int resp;
  char rsp_txt[256];

  if(s->pos > s->end_pos) {
    s->eof=1;
    return 0;
  }

  // Check to see if the server did not already terminate the transfer
  if(fd_can_read(p->handle, 0)) {
    if(readresp(p,rsp_txt) != 2)
      mp_msg(MSGT_OPEN,MSGL_WARN, "[ftp] Warning the server didn't finished the transfer correctly: %s\n",rsp_txt);
    closesocket(s->fd);
    s->fd = -1;
  }

  // Close current download
  if(s->fd >= 0) {
    static const char pre_cmd[]={TELNET_IAC,TELNET_IP,TELNET_IAC,TELNET_SYNCH};
    //int fl;
    
    // First close the fd
    closesocket(s->fd);
    s->fd = 0;
    
    // Send send the telnet sequence needed to make the server react
    
    // Dunno if this is really needed, lftp have it. I let
    // it here in case it turn out to be needed on some other OS
    //fl=fcntl(p->handle,F_GETFL);
    //fcntl(p->handle,F_SETFL,fl&~O_NONBLOCK);

    // send only first byte as OOB due to OOB braindamage in many unices
    send(p->handle,pre_cmd,1,MSG_OOB);
    send(p->handle,pre_cmd+1,sizeof(pre_cmd)-1,0);
    
    //fcntl(p->handle,F_SETFL,fl);

    // Get the 426 Transfer aborted
    // Or the 226 Transfer complete
    resp = readresp(p,rsp_txt);
    if(resp != 4 && resp != 2) {
      mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] Server didn't abort correctly: %s\n",rsp_txt);
      s->eof = 1;
      return 0;
    }
    // Send the ABOR command
    // Ignore the return code as sometimes it fail with "nothing to abort"
    FtpSendCmd("ABOR",p,rsp_txt);
  }
  return FtpOpenData(s,newpos);
}


static void close_f(stream_t *s) {
  struct stream_priv_s* p = s->priv;

  if(!p) return;

  if(s->fd > 0) {
    closesocket(s->fd);
    s->fd = 0;
  }

  FtpSendCmd("QUIT",p,NULL);

  if(p->handle) closesocket(p->handle);
  if(p->buf) free(p->buf);

  m_struct_free(&stream_opts,p);
}



static int open_f(stream_t *stream,int mode, void* opts, int* file_format) {
  int len = 0,resp;
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  char str[256],rsp_txt[256];

  if(mode != STREAM_READ) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] Unknown open mode %d\n",mode);
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }

  if(!p->filename || !p->host) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] Bad url\n");
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

  // Open the control connection
  p->handle = connect2Server(p->host,p->port,1);
  
  if(p->handle < 0) {
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

  // We got a connection, let's start serious things
  stream->fd = -1;
  stream->priv = p;
  p->buf = malloc(BUFSIZE);

  if (readresp(p, NULL) == 0) {
    close_f(stream);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

  // Login
  snprintf(str,255,"USER %s",p->user);
  resp = FtpSendCmd(str,p,rsp_txt);

  // password needed
  if(resp == 3) {
    snprintf(str,255,"PASS %s",p->pass);
    resp = FtpSendCmd(str,p,rsp_txt);
    if(resp != 2) {
      mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] command '%s' failed: %s\n",str,rsp_txt);
      close_f(stream);
      return STREAM_ERROR;
    }
  } else if(resp != 2) {
    mp_msg(MSGT_OPEN,MSGL_ERR, "[ftp] command '%s' failed: %s\n",str,rsp_txt);
    close_f(stream);
    return STREAM_ERROR;
  }
    
  // Set the transfer type
  resp = FtpSendCmd("TYPE I",p,rsp_txt);
  if(resp != 2) {
    mp_msg(MSGT_OPEN,MSGL_WARN, "[ftp] command 'TYPE I' failed: %s\n",rsp_txt);
    close_f(stream);
    return STREAM_ERROR;
  }

  // Get the filesize
  snprintf(str,255,"SIZE %s",p->filename);
  resp = FtpSendCmd(str,p,rsp_txt);
  if(resp != 2) {
    mp_msg(MSGT_OPEN,MSGL_WARN, "[ftp] command '%s' failed: %s\n",str,rsp_txt);
  } else {
    int dummy;
    sscanf(rsp_txt,"%d %d",&dummy,&len);
  }

  if(len > 0) {
    stream->seek = seek;
    stream->end_pos = len;
  }

  // The data connection is really opened only at the first
  // read/seek. This must be done when the cache is used
  // because the connection would stay open in the main process,
  // preventing correct abort with many servers.
  stream->fd = -1;
  stream->priv = p;
  stream->fill_buffer = fill_buffer;
  stream->close = close_f;

  return STREAM_OK;
}

const stream_info_t stream_info_ftp = {
  "File Transfer Protocol",
  "ftp",
  "Albeu",
  "reuse a bit of code from ftplib written by Thomas Pfau",
  open_f,
  { "ftp", NULL },
  &stream_opts,
  1 // Urls are an option string
};
