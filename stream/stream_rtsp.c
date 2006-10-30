/*
 *  Copyright (C) 2006 Benjamin Zores
 *   based on previous Real RTSP support from Roberto Togni and xine team.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include "config.h"
#ifndef HAVE_WINSOCK2
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <errno.h>

#include "stream.h"
#include "tcp.h"
#include "librtsp/rtsp.h"
#include "librtsp/rtsp_session.h"

#define RTSP_DEFAULT_PORT 554

extern int network_bandwidth;

static int
rtsp_streaming_read (int fd, char *buffer,
                     int size, streaming_ctrl_t *stream_ctrl)
{
  return rtsp_session_read (stream_ctrl->data, buffer, size);
}

static int
rtsp_streaming_start (stream_t *stream)
{
  int fd;
  rtsp_session_t *rtsp;
  char *mrl;
  char *file;
  int port;
  int redirected, temp;

  if (!stream)
    return -1;

  /* counter so we don't get caught in infinite redirections */
  temp = 5;

  do {
    redirected = 0;

    fd = connect2Server (stream->streaming_ctrl->url->hostname,
                         port = (stream->streaming_ctrl->url->port ?
                                 stream->streaming_ctrl->url->port :
                                 RTSP_DEFAULT_PORT), 1);
    
    if (fd < 0 && !stream->streaming_ctrl->url->port)
      fd = connect2Server (stream->streaming_ctrl->url->hostname,
                           port = 7070, 1);

    if (fd < 0)
      return -1;
    
    file = stream->streaming_ctrl->url->file;
    if (file[0] == '/')
      file++;

    mrl = malloc (strlen (stream->streaming_ctrl->url->hostname)
                  + strlen (file) + 16);
    
    sprintf (mrl, "rtsp://%s:%i/%s",
             stream->streaming_ctrl->url->hostname, port, file);

    rtsp = rtsp_session_start (fd, &mrl, file,
                               stream->streaming_ctrl->url->hostname,
                               port, &redirected,
                               stream->streaming_ctrl->bandwidth,
                               stream->streaming_ctrl->url->username,
                               stream->streaming_ctrl->url->password);

    if (redirected == 1)
    {
      url_free (stream->streaming_ctrl->url);
      stream->streaming_ctrl->url = url_new (mrl);
      closesocket (fd);
    }

    free (mrl);
    temp--;
  } while ((redirected != 0) && (temp > 0));    

  if (!rtsp)
    return -1;

  stream->fd = fd;
  stream->streaming_ctrl->data = rtsp;
  
  stream->streaming_ctrl->streaming_read = rtsp_streaming_read;
  stream->streaming_ctrl->streaming_seek = NULL;
  stream->streaming_ctrl->prebuffer_size = 128*1024;  // 640 KBytes
  stream->streaming_ctrl->buffering = 1;
  stream->streaming_ctrl->status = streaming_playing_e;
  
  return 0;
}

static void
rtsp_streaming_close (struct stream_st *s)
{
  rtsp_session_t *rtsp = NULL;
  
  rtsp = (rtsp_session_t *) s->streaming_ctrl->data;
  if (rtsp)
    rtsp_session_end (rtsp);
}

static int
rtsp_streaming_open (stream_t *stream, int mode, void *opts, int *file_format)
{
  URL_t *url;
  extern int index_mode;
  
  mp_msg (MSGT_OPEN, MSGL_V, "STREAM_RTSP, URL: %s\n", stream->url);
  stream->streaming_ctrl = streaming_ctrl_new ();
  if (!stream->streaming_ctrl)
    return STREAM_ERROR;

  stream->streaming_ctrl->bandwidth = network_bandwidth;
  url = url_new (stream->url);
  stream->streaming_ctrl->url = check4proxies (url);

  stream->fd = -1;
  index_mode = -1; /* prevent most RTSP streams from locking due to -idx */
  if (rtsp_streaming_start (stream) < 0)
  {
    streaming_ctrl_free (stream->streaming_ctrl);
    stream->streaming_ctrl = NULL;
    return STREAM_UNSUPORTED;
  }

  fixup_network_stream_cache (stream);
  stream->type = STREAMTYPE_STREAM;
  stream->close = rtsp_streaming_close;

  return STREAM_OK;
}

stream_info_t stream_info_rtsp = {
  "RTSP streaming",
  "rtsp",
  "Benjamin Zores, Roberto Togni",
  "ported from xine",
  rtsp_streaming_open,
  {"rtsp", NULL},
  NULL,
  0 /* Urls are an option string */
};
