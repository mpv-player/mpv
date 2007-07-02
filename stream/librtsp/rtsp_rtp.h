/*
 *  Copyright (C) 2006 Benjamin Zores
 *   heavily base on the Freebox patch for xine by Vincent Mussard
 *   but with many enhancements for better RTSP RFC compliance.
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

#ifndef HAVE_RTSP_RTP_H
#define HAVE_RTSP_RTP_H

#include "rtsp.h"

#define MAX_PREVIEW_SIZE 4096

struct rtp_rtsp_session_t {
  int rtp_socket;
  int rtcp_socket;
  char *control_url;
  int count;
};

struct rtp_rtsp_session_t *rtp_setup_and_play (rtsp_t* rtsp_session);
off_t rtp_read (struct rtp_rtsp_session_t* st, char *buf, off_t length);
void rtp_session_free (struct rtp_rtsp_session_t *st);
void rtcp_send_rr (rtsp_t *s, struct rtp_rtsp_session_t *st);

#endif /* HAVE_RTSP_RTP_H */

