/*
 *  Copyright (C) 2006 Benjamin Zores
 *   based on the Freebox patch for xine by Vincent Mussard
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "config.h"

#if !HAVE_WINSOCK2_H
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "mp_msg.h"
#include "rtsp.h"
#include "rtsp_rtp.h"
#include "rtsp_session.h"
#include "stream/network.h"
#include "stream/freesdp/common.h"
#include "stream/freesdp/parser.h"

#define RTSP_DEFAULT_PORT 31336
#define MAX_LENGTH 256

#define RTSP_ACCEPT_SDP "Accept: application/sdp"
#define RTSP_CONTENT_LENGTH "Content-length"
#define RTSP_CONTENT_TYPE "Content-Type"
#define RTSP_APPLICATION_SDP "application/sdp"
#define RTSP_RANGE "Range: "
#define RTSP_NPT_NOW "npt=now-"
#define RTSP_MEDIA_CONTAINER_MPEG_TS "33"
#define RTSP_TRANSPORT_REQUEST "Transport: RTP/AVP;%s;%s%i-%i;mode=\"PLAY\""

#define RTSP_TRANSPORT_MULTICAST "multicast"
#define RTSP_TRANSPORT_UNICAST "unicast"

#define RTSP_MULTICAST_PORT "port="
#define RTSP_UNICAST_CLIENT_PORT "client_port="
#define RTSP_UNICAST_SERVER_PORT "server_port="
#define RTSP_SETUP_DESTINATION "destination="

#define RTSP_SESSION "Session"
#define RTSP_TRANSPORT "Transport"

/* hardcoded RTCP RR - this is _NOT_ RFC compliant */
#define RTCP_RR_SIZE 32
#define RTCP_RR "\201\311\0\7(.JD\31+\306\343\0\0\0\0\0\0/E\0\0\2&\0\0\0\0\0\0\0\0\201"
#define RTCP_SEND_FREQUENCY 1024

int rtsp_port = 0;
char *rtsp_destination = NULL;

void
rtcp_send_rr (rtsp_t *s, struct rtp_rtsp_session_t *st)
{
  if (st->rtcp_socket == -1)
    return;

  /* send RTCP RR every RTCP_SEND_FREQUENCY packets
   * FIXME : NOT CORRECT, HARDCODED, BUT MAKES SOME SERVERS HAPPY
   * not rfc compliant
   * http://www.faqs.org/rfcs/rfc1889.html chapter 6 for RTCP
   */

  if (st->count == RTCP_SEND_FREQUENCY)
  {
    char rtcp_content[RTCP_RR_SIZE];
    strcpy (rtcp_content, RTCP_RR);
    send (st->rtcp_socket, rtcp_content, RTCP_RR_SIZE, DEFAULT_SEND_FLAGS);

    /* ping RTSP server to keep connection alive.
       we use OPTIONS instead of PING as not all servers support it */
    rtsp_request_options (s, "*");
    st->count = 0;
  }
  else
    st->count++;
}

static struct rtp_rtsp_session_t *
rtp_session_new (void)
{
  struct rtp_rtsp_session_t *st = NULL;

  st = malloc (sizeof (struct rtp_rtsp_session_t));

  st->rtp_socket = -1;
  st->rtcp_socket = -1;
  st->control_url = NULL;
  st->count = 0;

  return st;
}

void
rtp_session_free (struct rtp_rtsp_session_t *st)
{
  if (!st)
    return;

  if (st->rtp_socket != -1)
    close (st->rtp_socket);
  if (st->rtcp_socket != -1)
    close (st->rtcp_socket);

  if (st->control_url)
    free (st->control_url);
  free (st);
}

static void
rtp_session_set_fd (struct rtp_rtsp_session_t *st,
                    int rtp_sock, int rtcp_sock)
{
  if (!st)
    return;

  st->rtp_socket = rtp_sock;
  st->rtcp_socket = rtcp_sock;
}

static int
parse_port (const char *line, const char *param,
            int *rtp_port, int *rtcp_port)
{
  char *parse1;
  char *parse2;
  char *parse3;

  char *line_copy = strdup (line);

  parse1 = strstr (line_copy, param);

  if (parse1)
  {
    parse2 = strstr (parse1, "-");

    if (parse2)
    {
      parse3 = strstr (parse2, ";");

      if (parse3)
	parse3[0] = 0;

      parse2[0] = 0;
    }
    else
    {
      free (line_copy);
      return 0;
    }
  }
  else
  {
    free (line_copy);
    return 0;
  }

  *rtp_port = atoi (parse1 + strlen (param));
  *rtcp_port = atoi (parse2 + 1);

  free (line_copy);

  return 1;
}

static char *
parse_destination (const char *line)
{
  char *parse1;
  char *parse2;

  char *dest = NULL;
  char *line_copy = strdup (line);
  int len;

  parse1 = strstr (line_copy, RTSP_SETUP_DESTINATION);
  if (!parse1)
  {
    free (line_copy);
    return NULL;
  }

  parse2 = strstr (parse1, ";");
  if (!parse2)
  {
    free (line_copy);
    return NULL;
  }

  len = strlen (parse1) - strlen (parse2)
    - strlen (RTSP_SETUP_DESTINATION) + 1;
  dest = (char *) malloc (len + 1);
  snprintf (dest, len, parse1 + strlen (RTSP_SETUP_DESTINATION));
  free (line_copy);

  return dest;
}

static int
rtcp_connect (int client_port, int server_port, const char* server_hostname)
{
  struct sockaddr_in sin;
  struct hostent *hp;
  int s;

  if (client_port <= 1023)
    return -1;

  s = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == -1)
    return -1;

  hp = gethostbyname (server_hostname);
  if (!hp)
  {
    close (s);
    return -1;
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons (client_port);

  if (bind (s, (struct sockaddr *) &sin, sizeof (sin)))
  {
#if !HAVE_WINSOCK2_H
    if (errno != EINPROGRESS)
#else
    if (WSAGetLastError() != WSAEINPROGRESS)
#endif
    {
      close (s);
      return -1;
    }
  }

  sin.sin_family = AF_INET;
  memcpy (&(sin.sin_addr.s_addr), hp->h_addr, sizeof (hp->h_addr));
  sin.sin_port = htons (server_port);

  /* datagram socket */
  if (connect (s, (struct sockaddr *) &sin, sizeof (sin)) < 0)
  {
    close (s);
    return -1;
  }

  return s;
}

static int
rtp_connect (char *hostname, int port)
{
  struct sockaddr_in sin;
  struct timeval tv;
  int err, err_len;
  int rxsockbufsz;
  int s;
  fd_set set;

  if (port <= 1023)
    return -1;

  s = socket (PF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    return -1;

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  if (!hostname || !strcmp (hostname, "0.0.0.0"))
    sin.sin_addr.s_addr = htonl (INADDR_ANY);
  else
#if HAVE_INET_PTON
    inet_pton (AF_INET, hostname, &sin.sin_addr);
#elif HAVE_INET_ATON
    inet_aton (hostname, &sin.sin_addr);
#elif HAVE_WINSOCK2_H
    sin.sin_addr.s_addr = htonl (INADDR_ANY);
#endif
  sin.sin_port = htons (port);

  /* Increase the socket rx buffer size to maximum -- this is UDP */
  rxsockbufsz = 240 * 1024;
  if (setsockopt (s, SOL_SOCKET, SO_RCVBUF,
                  &rxsockbufsz, sizeof (rxsockbufsz)))
    mp_msg (MSGT_OPEN, MSGL_ERR, "Couldn't set receive socket buffer size\n");

  /* if multicast address, add membership */
  if ((ntohl (sin.sin_addr.s_addr) >> 28) == 0xe)
  {
    struct ip_mreq mcast;
    mcast.imr_multiaddr.s_addr = sin.sin_addr.s_addr;
    mcast.imr_interface.s_addr = 0;

    if (setsockopt (s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcast, sizeof (mcast)))
    {
      mp_msg (MSGT_OPEN, MSGL_ERR, "IP_ADD_MEMBERSHIP failed\n");
      close (s);
      return -1;
    }
  }

  /* datagram socket */
  if (bind (s, (struct sockaddr *) &sin, sizeof (sin)))
  {
#if !HAVE_WINSOCK2_H
    if (errno != EINPROGRESS)
#else
    if (WSAGetLastError() != WSAEINPROGRESS)
#endif
    {
      mp_msg (MSGT_OPEN, MSGL_ERR, "bind: %s\n", strerror (errno));
      close (s);
      return -1;
    }
  }

  tv.tv_sec = 1; /* 1 second timeout */
  tv.tv_usec = 0;

  FD_ZERO (&set);
  FD_SET (s, &set);

  err = select (s + 1, &set, NULL, NULL, &tv);
  if (err < 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR, "Select failed: %s\n", strerror (errno));
    close (s);
    return -1;
  }
  else if (err == 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR, "Timeout! No data from host %s\n", hostname);
    close (s);
    return -1;
  }

  err_len = sizeof (err);
  getsockopt (s, SOL_SOCKET, SO_ERROR, &err, (socklen_t *) &err_len);
  if (err)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR, "Socket error: %d\n", err);
    close (s);
    return -1;
  }

  return s;
}

static int
is_multicast_address (char *addr)
{
  struct sockaddr_in sin;

  if (!addr)
    return -1;

  sin.sin_family = AF_INET;

#if HAVE_INET_PTON
    inet_pton (AF_INET, addr, &sin.sin_addr);
#elif HAVE_INET_ATON
    inet_aton (addr, &sin.sin_addr);
#elif HAVE_WINSOCK2_H
    sin.sin_addr.s_addr = htonl (INADDR_ANY);
#endif

  if ((ntohl (sin.sin_addr.s_addr) >> 28) == 0xe)
    return 1;

  return 0;
}

struct rtp_rtsp_session_t *
rtp_setup_and_play (rtsp_t *rtsp_session)
{
  struct rtp_rtsp_session_t* rtp_session = NULL;
  const fsdp_media_description_t *med_dsc = NULL;
  char temp_buf[MAX_LENGTH + 1];
  char npt[256];

  char* answer;
  char* sdp;
  char *server_addr = NULL;
  char *destination = NULL;

  int statut;
  int content_length = 0;
  int is_multicast = 0;

  fsdp_description_t *dsc = NULL;
  fsdp_error_t result;

  int client_rtp_port = -1;
  int client_rtcp_port = -1;
  int server_rtp_port = -1;
  int server_rtcp_port = -1;
  int rtp_sock = -1;
  int rtcp_sock = -1;

  /* 1. send a RTSP DESCRIBE request to server */
  rtsp_schedule_field (rtsp_session, RTSP_ACCEPT_SDP);
  statut = rtsp_request_describe (rtsp_session, NULL);
  if (statut < 200 || statut > 299)
    return NULL;

  answer = rtsp_search_answers (rtsp_session, RTSP_CONTENT_LENGTH);
  if (answer)
    content_length = atoi (answer);
  else
    return NULL;

  answer = rtsp_search_answers (rtsp_session, RTSP_CONTENT_TYPE);
  if (!answer || !strstr (answer, RTSP_APPLICATION_SDP))
    return NULL;

  /* 2. read SDP message from server */
  sdp = (char *) malloc (content_length + 1);
  if (rtsp_read_data (rtsp_session, sdp, content_length) <= 0)
  {
    free (sdp);
    return NULL;
  }
  sdp[content_length] = 0;

  /* 3. parse SDP message */
  dsc = fsdp_description_new ();
  result = fsdp_parse (sdp, dsc);
  if (result != FSDPE_OK)
  {
    free (sdp);
    fsdp_description_delete (dsc);
    return NULL;
  }
  mp_msg (MSGT_OPEN, MSGL_V, "SDP:\n%s\n", sdp);
  free (sdp);

  /* 4. check for number of media streams: only one is supported */
  if (fsdp_get_media_count (dsc) != 1)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "A single media stream only is supported atm.\n");
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* 5. set the Normal Play Time parameter
   *    use range provided by server in SDP or start now if empty */
  sprintf (npt, RTSP_RANGE);
  if (fsdp_get_range (dsc))
    strcat (npt, fsdp_get_range (dsc));
  else
    strcat (npt, RTSP_NPT_NOW);

  /* 5. check for a valid media stream */
  med_dsc = fsdp_get_media (dsc, 0);
  if (!med_dsc)
  {
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* 6. parse the `m=<media>  <port>  <transport> <fmt list>' line */

  /* check for an A/V media */
  if (fsdp_get_media_type (med_dsc) != FSDP_MEDIA_VIDEO &&
      fsdp_get_media_type (med_dsc) != FSDP_MEDIA_AUDIO)
  {
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* only RTP/AVP transport method is supported right now */
  if (fsdp_get_media_transport_protocol (med_dsc) != FSDP_TP_RTP_AVP)
  {
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* only MPEG-TS is supported at the moment */
  if (!fsdp_get_media_format (med_dsc, 0) ||
      !strstr (fsdp_get_media_format (med_dsc, 0),
               RTSP_MEDIA_CONTAINER_MPEG_TS))
  {
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* get client port (if any) advised by server */
  client_rtp_port = fsdp_get_media_port (med_dsc);
  if (client_rtp_port == -1)
  {
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* if client_rtp_port = 0 => let client randomly pick one */
  if (client_rtp_port == 0)
  {
    /* TODO: we should check if the port is in use first */
    if (rtsp_port)
      client_rtp_port = rtsp_port;
    else
      client_rtp_port = RTSP_DEFAULT_PORT;
  }

  /* RTCP port generally is RTP port + 1 */
  client_rtcp_port = client_rtp_port + 1;

  mp_msg (MSGT_OPEN, MSGL_V,
          "RTP Port from SDP appears to be: %d\n", client_rtp_port);
  mp_msg (MSGT_OPEN, MSGL_V,
          "RTCP Port from SDP appears to be: %d\n", client_rtcp_port);

  /* 7. parse the `c=<network type> <addr type> <connection address>' line */

  /* check for a valid media network type (inet) */
  if (fsdp_get_media_network_type (med_dsc) != FSDP_NETWORK_TYPE_INET)
  {
    /* no control for media: try global one instead */
    if (fsdp_get_global_conn_network_type (dsc) != FSDP_NETWORK_TYPE_INET)
    {
      fsdp_description_delete (dsc);
      return NULL;
    }
  }

  /* only IPv4 is supported atm. */
  if (fsdp_get_media_address_type (med_dsc) != FSDP_ADDRESS_TYPE_IPV4)
  {
    /* no control for media: try global one instead */
    if (fsdp_get_global_conn_address_type (dsc) != FSDP_ADDRESS_TYPE_IPV4)
    {
      fsdp_description_delete (dsc);
      return NULL;
    }
  }

  /* get the media server address to connect to */
  if (fsdp_get_media_address (med_dsc))
    server_addr = strdup (fsdp_get_media_address (med_dsc));
  else if (fsdp_get_global_conn_address (dsc))
  {
    /* no control for media: try global one instead */
    server_addr = strdup (fsdp_get_global_conn_address (dsc));
  }

  if (!server_addr)
  {
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* check for a UNICAST or MULTICAST address to connect to */
  is_multicast = is_multicast_address (server_addr);

  /* 8. initiate an RTP session */
  rtp_session = rtp_session_new ();
  if (!rtp_session)
  {
    free (server_addr);
    fsdp_description_delete (dsc);
    return NULL;
  }

  /* get the media control URL */
  if (fsdp_get_media_control (med_dsc, 0))
    rtp_session->control_url = strdup (fsdp_get_media_control (med_dsc, 0));
  fsdp_description_delete (dsc);
  if (!rtp_session->control_url)
  {
    free (server_addr);
    rtp_session_free (rtp_session);
    return NULL;
  }

  /* 9. create the payload for RTSP SETUP request */
  memset (temp_buf, '\0', MAX_LENGTH);
  snprintf (temp_buf, MAX_LENGTH,
            RTSP_TRANSPORT_REQUEST,
            is_multicast ? RTSP_TRANSPORT_MULTICAST : RTSP_TRANSPORT_UNICAST,
            is_multicast ? RTSP_MULTICAST_PORT : RTSP_UNICAST_CLIENT_PORT,
            client_rtp_port, client_rtcp_port);
  mp_msg (MSGT_OPEN, MSGL_V, "RTSP Transport: %s\n", temp_buf);

  rtsp_unschedule_field (rtsp_session, RTSP_SESSION);
  rtsp_schedule_field (rtsp_session, temp_buf);

  /* 10. check for the media control URL type and initiate RTSP SETUP */
  if (!strncmp (rtp_session->control_url, "rtsp://", 7)) /* absolute URL */
    statut = rtsp_request_setup (rtsp_session,
                                 rtp_session->control_url, NULL);
  else /* relative URL */
    statut = rtsp_request_setup (rtsp_session,
                                 NULL, rtp_session->control_url);

  if (statut < 200 || statut > 299)
  {
    free (server_addr);
    rtp_session_free (rtp_session);
    return NULL;
  }

  /* 11. parse RTSP SETUP response: we need it to actually determine
   *     the real address and port to connect to */
  answer = rtsp_search_answers (rtsp_session, RTSP_TRANSPORT);
  if (!answer)
  {
    free (server_addr);
    rtp_session_free (rtp_session);
    return NULL;
  }

  /* check for RTP and RTCP ports to bind according to how request was done */
  is_multicast = 0;
  if (strstr (answer, RTSP_TRANSPORT_MULTICAST))
    is_multicast = 1;

  if (is_multicast)
    parse_port (answer, RTSP_MULTICAST_PORT,
                &client_rtp_port, &client_rtcp_port);
  else
  {
    parse_port (answer, RTSP_UNICAST_CLIENT_PORT,
                &client_rtp_port, &client_rtcp_port);
    parse_port (answer, RTSP_UNICAST_SERVER_PORT,
                &server_rtp_port, &server_rtcp_port);
  }

  /* now check network settings as determined by server */
  if (rtsp_destination)
    destination = strdup (rtsp_destination);
  else
    destination = parse_destination (answer);
  if (!destination)
    destination = strdup (server_addr);
  free (server_addr);

  mp_msg (MSGT_OPEN, MSGL_V, "RTSP Destination: %s\n", destination);
  mp_msg (MSGT_OPEN, MSGL_V, "Client RTP port : %d\n", client_rtp_port);
  mp_msg (MSGT_OPEN, MSGL_V, "Client RTCP port : %d\n", client_rtcp_port);
  mp_msg (MSGT_OPEN, MSGL_V, "Server RTP port : %d\n", server_rtp_port);
  mp_msg (MSGT_OPEN, MSGL_V, "Server RTCP port : %d\n", server_rtcp_port);

  /* 12. performs RTSP PLAY request */
  rtsp_schedule_field (rtsp_session, npt);
  statut = rtsp_request_play (rtsp_session, NULL);
  if (statut < 200 || statut > 299)
  {
    free (destination);
    rtp_session_free (rtp_session);
    return NULL;
  }

  /* 13. create RTP and RTCP connections */
  rtp_sock = rtp_connect (destination, client_rtp_port);
  rtcp_sock = rtcp_connect (client_rtcp_port, server_rtcp_port, destination);
  rtp_session_set_fd (rtp_session, rtp_sock, rtcp_sock);
  free (destination);

  mp_msg (MSGT_OPEN, MSGL_V, "RTP Sock : %d\nRTCP Sock : %d\n",
          rtp_session->rtp_socket, rtp_session->rtcp_socket);

  if (rtp_session->rtp_socket == -1)
  {
    rtp_session_free (rtp_session);
    return NULL;
  }

  return rtp_session;
}
