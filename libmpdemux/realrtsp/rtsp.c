/*
 * This file was ported to MPlayer from xine CVS rtsp.c,v 1.9 2003/04/10 02:30:48
 */

/*
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * a minimalistic implementation of rtsp protocol,
 * *not* RFC 2326 compilant yet.
 */

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "config.h"
#ifndef HAVE_WINSOCK2
#define closesocket close
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <inttypes.h>

#include "rtsp.h"

/*
#define LOG
*/

#define BUF_SIZE 4096
#define HEADER_SIZE 1024
#define MAX_FIELDS 256

struct rtsp_s {

  int           s;

  char         *host;
  int           port;
  char         *path;
  char         *mrl;
  char         *user_agent;

  char         *server;
  unsigned int  server_state;
  uint32_t      server_caps;
  
  char          buffer[BUF_SIZE]; /* scratch buffer */

  unsigned int  cseq;
  char         *session;

  char        *answers[MAX_FIELDS];   /* data of last message */
  char        *scheduled[MAX_FIELDS]; /* will be sent with next message */
};

/*
 * constants
 */

const char rtsp_protocol_version[]="RTSP/1.0";

/* server states */
#define RTSP_CONNECTED 1
#define RTSP_INIT      2
#define RTSP_READY     4
#define RTSP_PLAYING   8
#define RTSP_RECORDING 16

/* server capabilities */
#define RTSP_OPTIONS       0x001
#define RTSP_DESCRIBE      0x002
#define RTSP_ANNOUNCE      0x004
#define RTSP_SETUP         0x008
#define RTSP_GET_PARAMETER 0x010
#define RTSP_SET_PARAMETER 0x020
#define RTSP_TEARDOWN      0x040
#define RTSP_PLAY          0x080
#define RTSP_RECORD        0x100

/*
 * network utilities
 */
 
static int host_connect_attempt(struct in_addr ia, int port) {

  int                s;
  struct sockaddr_in sin;

  s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  
  if (s == -1) {
    printf ("rtsp: socket(): %s\n", strerror(errno));
    return -1;
  }

  sin.sin_family = AF_INET;
  sin.sin_addr   = ia;
  sin.sin_port   = htons(port);
  
  if (connect(s, (struct sockaddr *)&sin, sizeof(sin))==-1 
#ifndef HAVE_WINSOCK2
      && errno != EINPROGRESS) {
#else
      && WSAGetLastError() == WSAEINPROGRESS) {
#endif
    printf ("rtsp: connect(): %s\n", strerror(errno));
    closesocket(s);
    return -1;
  }

  return s;
}

static int host_connect(const char *host, int port) {

  struct hostent *h;
  int             i, s;
  
  h = gethostbyname(host);
  if (h == NULL) {
    printf ("rtsp: unable to resolve '%s'.\n", host);
    return -1;
  }

  for (i = 0; h->h_addr_list[i]; i++) {
    struct in_addr ia;

    memcpy (&ia, h->h_addr_list[i], 4);
    s = host_connect_attempt(ia, port);
    if(s != -1)
      return s;
  }
  printf ("rtsp: unable to connect to '%s'.\n", host);
  return -1;
}

static int write_stream(int s, const char *buf, int len) {
  int total, timeout;

  total = 0; timeout = 30;
  while (total < len){ 
    int n;

    n = send (s, &buf[total], len - total, 0);

    if (n > 0)
      total += n;
    else if (n < 0) {
#ifndef HAVE_WINSOCK2
      if ((timeout>0) && ((errno == EAGAIN) || (errno == EINPROGRESS))) {
#else
      if ((timeout>0) && ((errno == EAGAIN) || (WSAGetLastError() == WSAEINPROGRESS))) {
#endif
        sleep (1); timeout--;
      } else
        return -1;
    }
  }

  return total;
}

static ssize_t read_stream(int fd, void *buf, size_t count) {
  
  ssize_t ret, total;

  total = 0;

  while (total < count) {
  
    ret=recv (fd, ((uint8_t*)buf)+total, count-total, 0);

    if (ret<0) {
      if(errno == EAGAIN) {
        fd_set rset;
        struct timeval timeout;
    
        FD_ZERO (&rset);
        FD_SET  (fd, &rset);
        
        timeout.tv_sec  = 30;
        timeout.tv_usec = 0;
        
        if (select (fd+1, &rset, NULL, NULL, &timeout) <= 0) {
          return -1;
        }
        continue;
      }
      
      printf ("rtsp: read error.\n");
      return ret;
    } else
      total += ret;
    
    /* end of stream */
    if (!ret) break;
  }

  return total;
}

/*
 * debugging utilities
 */
#if 0 
static void hexdump (char *buf, int length) {

  int i;

  printf ("rtsp: ascii>");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    if ((c >= 32) && (c <= 128))
      printf ("%c", c);
    else
      printf (".");
  }
  printf ("\n");

  printf ("rtsp: hexdump> ");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    printf ("%02x", c);

    if ((i % 16) == 15)
      printf ("\nrtsp:         ");

    if ((i % 2) == 1)
      printf (" ");

  }
  printf ("\n");
}
#endif

/*
 * rtsp_get gets a line from stream
 * and returns a null terminated string.
 */
 
static char *rtsp_get(rtsp_t *s) {

  int n=0;
  char *string;

  while (n<BUF_SIZE) {
    read_stream(s->s, &s->buffer[n], 1);
    if ((s->buffer[n-1]==0x0d)&&(s->buffer[n]==0x0a)) break;
    n++;
  }

  if (n>=BUF_SIZE) {
    printf("librtsp: buffer overflow in rtsp_get\n");
    exit(1);
  }
  string=malloc(sizeof(char)*n);
  memcpy(string,s->buffer,n-1);
  string[n-1]=0;

#ifdef LOG
  printf("librtsp: << '%s'\n", string);
#endif
  

  return string;
}

/*
 * rtsp_put puts a line on stream
 */
 
static void rtsp_put(rtsp_t *s, const char *string) {

  int len=strlen(string);
  char *buf=malloc(sizeof(char)*len+2);

#ifdef LOG
  printf("librtsp: >> '%s'", string);
#endif

  memcpy(buf,string,len);
  buf[len]=0x0d;
  buf[len+1]=0x0a;

  write_stream(s->s, buf, len+2);
  
#ifdef LOG
  printf(" done.\n");
#endif

  free(buf);
}

/*
 * extract server status code
 */

static int rtsp_get_code(const char *string) {

  char buf[4];
  int code=0;
 
  if (!strncmp(string, rtsp_protocol_version, strlen(rtsp_protocol_version)))
  {
    memcpy(buf, string+strlen(rtsp_protocol_version)+1, 3);
    buf[3]=0;
    code=atoi(buf);
  } else if (!strncmp(string, "SET_PARAMETER",8))
  {
    return RTSP_STATUS_SET_PARAMETER;
  }

  if(code != 200) printf("librtsp: server responds: '%s'\n",string);

  return code;
}

/*
 * send a request
 */

static void rtsp_send_request(rtsp_t *s, const char *type, const char *what) {

  char **payload=s->scheduled;
  sprintf(s->buffer,"%s %s %s",type, what, rtsp_protocol_version);
  rtsp_put(s,s->buffer);
  if (payload)
    while (*payload) {
      rtsp_put(s,*payload);
      payload++;
    }
  rtsp_put(s,"");
  rtsp_unschedule_all(s);
}

/*
 * schedule standard fields
 */

static void rtsp_schedule_standard(rtsp_t *s) {

  sprintf(s->buffer, "Cseq: %u", s->cseq);
  rtsp_schedule_field(s, s->buffer);
  if (s->session) {
    sprintf(s->buffer, "Session: %s", s->session);
    rtsp_schedule_field(s, s->buffer);
  }
}
/*
 * get the answers, if server responses with something != 200, return NULL
 */
 
static int rtsp_get_answers(rtsp_t *s) {

  char *answer=NULL;
  unsigned int answer_seq;
  char **answer_ptr=s->answers;
  int code;
  
  answer=rtsp_get(s);
  code=rtsp_get_code(answer);
  free(answer);

  rtsp_free_answers(s);
  
  do { /* while we get answer lines */
  
    answer=rtsp_get(s);
    
    if (!strncmp(answer,"Cseq:",5)) {
      sscanf(answer,"Cseq: %u",&answer_seq);
      if (s->cseq != answer_seq) {
#ifdef LOG
        printf("librtsp: warning: Cseq mismatch. got %u, assumed %u", answer_seq, s->cseq);
#endif
        s->cseq=answer_seq;
      }
    }
    if (!strncmp(answer,"Server:",7)) {
      sscanf(answer,"Server: %s",s->buffer);
      if (s->server) free(s->server);
      s->server=strdup(s->buffer);
    }
    if (!strncmp(answer,"Session:",8)) {
      memset(s->buffer,0, BUF_SIZE);
      sscanf(answer,"Session: %s",s->buffer);
      if (s->session) {
        if (strcmp(s->buffer, s->session)) {
          printf("rtsp: warning: setting NEW session: %s\n", s->buffer);
          free(s->session);
          s->session=strdup(s->buffer);
        }
      } else
      {
#ifdef LOG
        printf("rtsp: setting session id to: %s\n", s->buffer);
#endif
        s->session=strdup(s->buffer);
      }
    }
    *answer_ptr=answer;
    answer_ptr++;
  } while (strlen(answer)!=0);
  
  s->cseq++;
  
  *answer_ptr=NULL;
  rtsp_schedule_standard(s);
    
  return code;
}

/*
 * send an ok message
 */

int rtsp_send_ok(rtsp_t *s) {
  char cseq[16];
  
  rtsp_put(s, "RTSP/1.0 200 OK");
  sprintf(cseq,"CSeq: %u", s->cseq);
  rtsp_put(s, cseq);
  rtsp_put(s, "");
  return 0;
}

/*
 * implementation of must-have rtsp requests; functions return
 * server status code.
 */

int rtsp_request_options(rtsp_t *s, const char *what) {

  char *buf;

  if (what) {
    buf=strdup(what);
  } else
  {
    buf=malloc(sizeof(char)*(strlen(s->host)+16));
    sprintf(buf,"rtsp://%s:%i", s->host, s->port);
  }
  rtsp_send_request(s,"OPTIONS",buf);
  free(buf);

  return rtsp_get_answers(s);
}

int rtsp_request_describe(rtsp_t *s, const char *what) {

  char *buf;

  if (what) {
    buf=strdup(what);
  } else
  {
    buf=malloc(sizeof(char)*(strlen(s->host)+strlen(s->path)+16));
    sprintf(buf,"rtsp://%s:%i/%s", s->host, s->port, s->path);
  }
  rtsp_send_request(s,"DESCRIBE",buf);
  free(buf);
  
  return rtsp_get_answers(s);
}

int rtsp_request_setup(rtsp_t *s, const char *what) {

  rtsp_send_request(s,"SETUP",what);
  
  return rtsp_get_answers(s);
}

int rtsp_request_setparameter(rtsp_t *s, const char *what) {

  char *buf;

  if (what) {
    buf=strdup(what);
  } else
  {
    buf=malloc(sizeof(char)*(strlen(s->host)+strlen(s->path)+16));
    sprintf(buf,"rtsp://%s:%i/%s", s->host, s->port, s->path);
  }
  rtsp_send_request(s,"SET_PARAMETER",buf);
  free(buf);
  
  return rtsp_get_answers(s);
}

int rtsp_request_play(rtsp_t *s, const char *what) {

  char *buf;

  if (what) {
    buf=strdup(what);
  } else
  {
    buf=malloc(sizeof(char)*(strlen(s->host)+strlen(s->path)+16));
    sprintf(buf,"rtsp://%s:%i/%s", s->host, s->port, s->path);
  }
  rtsp_send_request(s,"PLAY",buf);
  free(buf);
  
  return rtsp_get_answers(s);
}

int rtsp_request_tearoff(rtsp_t *s, const char *what) {

  rtsp_send_request(s,"TEAROFF",what);
  
  return rtsp_get_answers(s);
}

/*
 * read opaque data from stream
 */

int rtsp_read_data(rtsp_t *s, char *buffer, unsigned int size) {

  int i,seq;

  if (size>=4) {
    i=read_stream(s->s, buffer, 4);
    if (i<4) return i;
    if ((buffer[0]=='S')&&(buffer[1]=='E')&&(buffer[2]=='T')&&(buffer[3]=='_'))
    {
      char *rest=rtsp_get(s);
      /* a real server wanna play table tennis? */
      memcpy(s->buffer, buffer, 4);
      strcpy(s->buffer+4, rest);
      seq=-1;
      do {
        free(rest);
        rest=rtsp_get(s);
        if (!strncmp(rest,"Cseq:",5))
          sscanf(rest,"Cseq: %u",&seq);
      } while (strlen(rest)!=0);
      free(rest);
      if (seq<0) {
#ifdef LOG
        printf("rtsp: warning: cseq not recognized!\n");
#endif
        seq=1;
      }
      /* let's make the server happy */
      rtsp_put(s, "RTSP/1.0 451 Parameter Not Understood");
      rest=malloc(sizeof(char)*16);
      sprintf(rest,"CSeq: %u", seq);
      rtsp_put(s, rest);
      rtsp_put(s, "");
      i=read_stream(s->s, buffer, size);
    } else
    {
      i=read_stream(s->s, buffer+4, size-4);
      i+=4;
    }
  } else
    i=read_stream(s->s, buffer, size);
#ifdef LOG
  printf("librtsp: << %d of %d bytes\n", i, size);
#endif

  return i;
}

/*
 * connect to a rtsp server
 */

//rtsp_t *rtsp_connect(const char *mrl, const char *user_agent) {
rtsp_t *rtsp_connect(int fd, char* mrl, char *path, char *host, int port, char *user_agent) {

  rtsp_t *s=malloc(sizeof(rtsp_t));
  int i;
  
  for (i=0; i<MAX_FIELDS; i++) {
    s->answers[i]=NULL;
    s->scheduled[i]=NULL;
  }

  s->server=NULL;
  s->server_state=0;
  s->server_caps=0;
  
  s->cseq=0;
  s->session=NULL;
  
  if (user_agent)
    s->user_agent=strdup(user_agent);
  else
    s->user_agent=strdup("User-Agent: RealMedia Player Version 6.0.9.1235 (linux-2.0-libc6-i386-gcc2.95)");

  s->mrl = strdup(mrl);
  s->host = strdup(host);
  s->port = port;
  s->path = strdup(path);
  s->s = fd;

  if (s->s < 0) {
    printf ("rtsp: failed to connect to '%s'\n", s->host);
    rtsp_close(s);
    return NULL;
  }

  s->server_state=RTSP_CONNECTED;

  /* now let's send an options request. */
  rtsp_schedule_field(s, "CSeq: 1");
  rtsp_schedule_field(s, s->user_agent);
  rtsp_schedule_field(s, "ClientChallenge: 9e26d33f2984236010ef6253fb1887f7");
  rtsp_schedule_field(s, "PlayerStarttime: [28/03/2003:22:50:23 00:00]");
  rtsp_schedule_field(s, "CompanyID: KnKV4M4I/B2FjJ1TToLycw==");
  rtsp_schedule_field(s, "GUID: 00000000-0000-0000-0000-000000000000");
  rtsp_schedule_field(s, "RegionData: 0");
  rtsp_schedule_field(s, "ClientID: Linux_2.4_6.0.9.1235_play32_RN01_EN_586");
  /*rtsp_schedule_field(s, "Pragma: initiate-session");*/
  rtsp_request_options(s, NULL);

  return s;
}


/*
 * closes an rtsp connection 
 */

void rtsp_close(rtsp_t *s) {

  if (s->server_state) closesocket(s->s); /* TODO: send a TEAROFF */
  if (s->path) free(s->path);
  if (s->host) free(s->host);
  if (s->mrl) free(s->mrl);
  if (s->session) free(s->session);
  if (s->user_agent) free(s->user_agent);
  rtsp_free_answers(s);
  rtsp_unschedule_all(s);
  free(s);  
}

/*
 * search in answers for tags. returns a pointer to the content
 * after the first matched tag. returns NULL if no match found.
 */

char *rtsp_search_answers(rtsp_t *s, const char *tag) {

  char **answer;
  char *ptr;
  
  if (!s->answers) return NULL;
  answer=s->answers;

  while (*answer) {
    if (!strncasecmp(*answer,tag,strlen(tag))) {
      ptr=strchr(*answer,':');
      ptr++;
      while(*ptr==' ') ptr++;
      return ptr;
    }
    answer++;
  }

  return NULL;
}

/*
 * session id management
 */

void rtsp_set_session(rtsp_t *s, const char *id) {

  if (s->session) free(s->session);

  s->session=strdup(id);

}

char *rtsp_get_session(rtsp_t *s) {

  return s->session;

}

char *rtsp_get_mrl(rtsp_t *s) {

  return s->mrl;

}

/*
 * schedules a field for transmission
 */

void rtsp_schedule_field(rtsp_t *s, const char *string) {

  int i=0;
  
  if (!string) return;

  while(s->scheduled[i]) {
    i++;
  }
  s->scheduled[i]=strdup(string);
}

/*
 * removes the first scheduled field which prefix matches string. 
 */

void rtsp_unschedule_field(rtsp_t *s, const char *string) {

  char **ptr=s->scheduled;
  
  if (!string) return;

  while(*ptr) {
    if (!strncmp(*ptr, string, strlen(string)))
      break;
  }
  if (*ptr) free(*ptr);
  ptr++;
  do {
    *(ptr-1)=*ptr;
  } while(*ptr);
}

/*
 * unschedule all fields
 */

void rtsp_unschedule_all(rtsp_t *s) {

  char **ptr;
  
  if (!s->scheduled) return;
  ptr=s->scheduled;

  while (*ptr) {
    free(*ptr);
    *ptr=NULL;
    ptr++;
  }
}
/*
 * free answers
 */

void rtsp_free_answers(rtsp_t *s) {

  char **answer;
  
  if (!s->answers) return;
  answer=s->answers;

  while (*answer) {
    free(*answer);
    *answer=NULL;
    answer++;
  }
}
