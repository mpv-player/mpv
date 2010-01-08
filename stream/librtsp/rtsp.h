/*
 * This file was ported to MPlayer from xine CVS rtsp.h,v 1.2 2002/12/16 21:50:55
 */

/*
 * Copyright (C) 2002 the xine project
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
 *
 *    2006, Benjamin Zores and Vincent Mussard
 *      fixed a lot of RFC compliance issues.
 */

#ifndef MPLAYER_RTSP_H
#define MPLAYER_RTSP_H

#include <inttypes.h>

/* some codes returned by rtsp_request_* functions */

#define RTSP_STATUS_SET_PARAMETER  10
#define RTSP_STATUS_OK            200

#define RTSP_METHOD_OPTIONS "OPTIONS"
#define RTSP_METHOD_DESCRIBE "DESCRIBE"
#define RTSP_METHOD_SETUP "SETUP"
#define RTSP_METHOD_PLAY "PLAY"
#define RTSP_METHOD_TEARDOWN "TEARDOWN"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"

#define BUF_SIZE 4096
#define RTSP_HEADER_SIZE 1024
#define MAX_FIELDS 256


struct rtsp_s {

  int           s;

  char         *host;
  int           port;
  char         *path;
  char         *param;
  char         *mrl;
  char         *user_agent;

  char         *server;
  unsigned int  server_state;
  uint32_t      server_caps;

  unsigned int  cseq;
  char         *session;

  char        *answers[MAX_FIELDS];   /* data of last message */
  char        *scheduled[MAX_FIELDS]; /* will be sent with next message */
};

/*
 * constants
 */

#define RTSP_PROTOCOL_VERSION "RTSP/1.0"

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


typedef struct rtsp_s rtsp_t;

rtsp_t*  rtsp_connect (int fd, char *mrl, char *path, char *host, int port, char *user_agent);

int rtsp_request_options(rtsp_t *s, const char *what);
int rtsp_request_describe(rtsp_t *s, const char *what);
int rtsp_request_setup(rtsp_t *s, const char *what, char *control);
int rtsp_request_setparameter(rtsp_t *s, const char *what);
int rtsp_request_play(rtsp_t *s, const char *what);
int rtsp_request_teardown(rtsp_t *s, const char *what);

int rtsp_send_ok(rtsp_t *s);

int rtsp_read_data(rtsp_t *s, char *buffer, unsigned int size);

char* rtsp_search_answers(rtsp_t *s, const char *tag);
void rtsp_add_to_payload(char **payload, const char *string);

void rtsp_free_answers(rtsp_t *this);

int      rtsp_read (rtsp_t *this, char *data, int len);

void  rtsp_set_session(rtsp_t *s, const char *id);
char *rtsp_get_session(rtsp_t *s);

char *rtsp_get_mrl(rtsp_t *s);
char *rtsp_get_param(rtsp_t *s, const char *param);

/*int      rtsp_peek_header (rtsp_t *this, char *data); */

void rtsp_schedule_field(rtsp_t *s, const char *string);
void rtsp_unschedule_field(rtsp_t *s, const char *string);
void rtsp_unschedule_all(rtsp_t *s);

#endif /* MPLAYER_RTSP_H */
