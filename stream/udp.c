/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Network helpers for UDP connections (originally borrowed from rtp.c).
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>

#ifndef HAVE_WINSOCK2
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "mp_msg.h"
#include "url.h"
#include "udp.h"

int reuse_socket=0;

/* Start listening on a UDP port. If multicast, join the group. */
int
udp_open_socket (URL_t *url)
{
  int socket_server_fd, rxsockbufsz;
  int err;
  socklen_t err_len;
  fd_set set;
  struct sockaddr_in server_address;
  struct ip_mreq mcast;
  struct timeval tv;
  struct hostent *hp;
  int reuse=reuse_socket;

  mp_msg (MSGT_NETWORK, MSGL_V,
          "Listening for traffic on %s:%d ...\n", url->hostname, url->port);

  socket_server_fd = socket (AF_INET, SOCK_DGRAM, 0);
  if (socket_server_fd == -1)
  {
    mp_msg (MSGT_NETWORK, MSGL_ERR, "Failed to create socket\n");
    return -1;
  }

  if (isalpha (url->hostname[0]))
  {
#ifndef HAVE_WINSOCK2
    hp = (struct hostent *) gethostbyname (url->hostname);
    if (!hp)
    {
      mp_msg (MSGT_NETWORK, MSGL_ERR,
              "Counldn't resolve name: %s\n", url->hostname);
      closesocket (socket_server_fd);
      return -1;
    }
    memcpy ((void *) &server_address.sin_addr.s_addr,
            (void *) hp->h_addr_list[0], hp->h_length);
#else
    server_address.sin_addr.s_addr = htonl (INADDR_ANY);
#endif /* HAVE_WINSOCK2 */
  }
  else
  {
#ifndef HAVE_WINSOCK2
#ifdef USE_ATON
    inet_aton (url->hostname, &server_address.sin_addr);
#else
    inet_pton (AF_INET, url->hostname, &server_address.sin_addr);
#endif /* USE_ATON */
#else
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif /* HAVE_WINSOCK2 */
  }
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons (url->port);

  if(reuse_socket && setsockopt(socket_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
      mp_msg(MSGT_NETWORK, MSGL_ERR, "SO_REUSEADDR failed! ignore.\n");

  if (bind (socket_server_fd, (struct sockaddr *) &server_address,
            sizeof (server_address)) == -1)
  {
#ifndef HAVE_WINSOCK2
    if (errno != EINPROGRESS)
#else
    if (WSAGetLastError () != WSAEINPROGRESS)
#endif /* HAVE_WINSOCK2 */
    {
      mp_msg (MSGT_NETWORK, MSGL_ERR, "Failed to connect to server\n");
      closesocket (socket_server_fd);
      return -1;
    }
  }
	
#ifdef HAVE_WINSOCK2
  if (isalpha (url->hostname[0]))
  {
    hp = (struct hostent *) gethostbyname (url->hostname);
    if (!hp)
    {
      mp_msg (MSGT_NETWORK, MSGL_ERR,
              "Counldn't resolve name: %s\n", url->hostname);
      closesocket (socket_server_fd);
      return -1;
    }
    memcpy ((void *) &server_address.sin_addr.s_addr,
            (void *) hp->h_addr, hp->h_length);
  }
  else
  {
    unsigned int addr = inet_addr (url->hostname);
    memcpy ((void *) &server_address.sin_addr, (void *) &addr, sizeof (addr));
  }
#endif /* HAVE_WINSOCK2 */

  /* Increase the socket rx buffer size to maximum -- this is UDP */
  rxsockbufsz = 240 * 1024;
  if (setsockopt (socket_server_fd, SOL_SOCKET, SO_RCVBUF,
                  &rxsockbufsz, sizeof (rxsockbufsz)))
  {
    mp_msg (MSGT_NETWORK, MSGL_ERR,
            "Couldn't set receive socket buffer size\n");
  }

  if ((ntohl (server_address.sin_addr.s_addr) >> 28) == 0xe)
  {
    mcast.imr_multiaddr.s_addr = server_address.sin_addr.s_addr;
    mcast.imr_interface.s_addr = 0;

    if (setsockopt (socket_server_fd, IPPROTO_IP,
                    IP_ADD_MEMBERSHIP, &mcast, sizeof (mcast)))
    {
      mp_msg (MSGT_NETWORK, MSGL_ERR, "IP_ADD_MEMBERSHIP failed (do you have multicasting enabled in your kernel?)\n");
      closesocket (socket_server_fd);
      return -1;
    }
  }

  tv.tv_sec = 0;
  tv.tv_usec = (1 * 1000000);	/* 1 second timeout */

  FD_ZERO (&set);
  FD_SET (socket_server_fd, &set);
  
  err = select (socket_server_fd + 1, &set, NULL, NULL, &tv);
  if (err < 0)
  {
    mp_msg (MSGT_NETWORK, MSGL_FATAL,
            "Select failed: %s\n", strerror (errno));
    closesocket (socket_server_fd);
    return -1;
  }

  if (err == 0)
  {
    mp_msg (MSGT_NETWORK, MSGL_ERR,
            "Timeout! No data from host %s\n", url->hostname);
    closesocket (socket_server_fd);
    return -1;
  }

  err_len = sizeof (err);
  getsockopt (socket_server_fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
  if (err)
  {
    mp_msg (MSGT_NETWORK, MSGL_DBG2, "Socket error: %d\n", err);
    closesocket (socket_server_fd);
    return -1;
  }

  return socket_server_fd;
}
