/*
 * This file was ported to MPlayer from xine CVS rtsp_session.c,v 1.9 2003/02/11 16:20:40
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
 * high level interface to rtsp servers.
 */

#include <sys/types.h>
#include "config.h"
#ifndef HAVE_WINSOCK2
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "rtsp.h"
#include "rtsp_session.h"
#include "real.h"
#include "rmff.h"
#include "asmrp.h"

/*
#define LOG
*/

#define BUF_SIZE 4096
#define HEADER_SIZE 4096

struct rtsp_session_s {

  rtsp_t       *s;

  /* receive buffer */
  uint8_t       recv[BUF_SIZE];
  int           recv_size;
  int           recv_read;

  /* header buffer */
  uint8_t       header[HEADER_SIZE];
  int           header_len;
  int           header_read;

};

//rtsp_session_t *rtsp_session_start(char *mrl) {
rtsp_session_t *rtsp_session_start(int fd, char **mrl, char *path, char *host, int port, int *redir) {

  rtsp_session_t *rtsp_session=malloc(sizeof(rtsp_session_t));
  char *server;
  char *mrl_line = NULL;
  rmff_header_t *h;
  uint32_t bandwidth=10485800;

//connect:
  *redir = 0;

  /* connect to server */
  rtsp_session->s=rtsp_connect(fd,*mrl,path,host,port,NULL);
  if (!rtsp_session->s)
  {
    printf("rtsp_session: failed to connect to server %s\n", path);
    free(rtsp_session);
    return NULL;
  }

  /* looking for server type */
  if (rtsp_search_answers(rtsp_session->s,"Server"))
    server=strdup(rtsp_search_answers(rtsp_session->s,"Server"));
  else {
    if (rtsp_search_answers(rtsp_session->s,"RealChallenge1"))
      server=strdup("Real");
    else
      server=strdup("unknown");
  }
  if (strstr(server,"Real") || strstr(server,"Helix"))
  {
    /* we are talking to a real server ... */

    h=real_setup_and_get_header(rtsp_session->s, bandwidth);
    if (!h) {
      /* got an redirect? */
      if (rtsp_search_answers(rtsp_session->s, "Location"))
      {
        free(mrl_line);
	mrl_line=strdup(rtsp_search_answers(rtsp_session->s, "Location"));
        printf("rtsp_session: redirected to %s\n", mrl_line);
	rtsp_close(rtsp_session->s);
	free(server);
        free(*mrl);
        free(rtsp_session);
        /* tell the caller to redirect, return url to redirect to in mrl */
        *mrl = mrl_line;
        *redir = 1;
        return NULL;
//	goto connect; /* *shudder* i made a design mistake somewhere */
      } else
      {
        printf("rtsp_session: session can not be established.\n");
        rtsp_close(rtsp_session->s);
        free(rtsp_session);
        return NULL;
      }
    }
	
    rtsp_session->header_len=rmff_dump_header(h,rtsp_session->header,1024);

    memcpy(rtsp_session->recv, rtsp_session->header, rtsp_session->header_len);
    rtsp_session->recv_size = rtsp_session->header_len;
    rtsp_session->recv_read = 0;
    
  } else
  {
    printf("rtsp_session: rtsp server type is '%s' instead of Real. Please report.\n",server);
    rtsp_close(rtsp_session->s);
    free(server);
    free(rtsp_session);
    return NULL;
  }
  free(server);
  
  return rtsp_session;
}

int rtsp_session_read (rtsp_session_t *this, char *data, int len) {
  
  int to_copy=len;
  char *dest=data;
  char *source=this->recv + this->recv_read;
  int fill=this->recv_size - this->recv_read;

  if (len < 0) return 0;
  while (to_copy > fill) {
    
    memcpy(dest, source, fill);
    to_copy -= fill;
    dest += fill;
    this->recv_read = 0;
    source = this->recv;
    this->recv_size = real_get_rdt_chunk (this->s, source);
    fill = this->recv_size;

    if (this->recv_size == 0) {
#ifdef LOG
      printf ("librtsp: %d of %d bytes provided\n", len-to_copy, len);
#endif
      return len-to_copy;
    }
  }
  
  memcpy(dest, source, to_copy);
  this->recv_read += to_copy;

#ifdef LOG
  printf ("librtsp: %d bytes provided\n", len);
#endif

  return len;
}

int rtsp_session_peek_header(rtsp_session_t *this, char *buf, int maxsize) {

  int len;

  len = (this->header_len < maxsize) ? this->header_len : maxsize;

  memcpy(buf, this->header, len);
  return len;
}

void rtsp_session_end(rtsp_session_t *session) {

  rtsp_close(session->s);
  free(session);
}
