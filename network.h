/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __NETWORK_H
#define __NETWORK_H

#define STREAMING_TYPE_UNKNOWN	-1
#define STREAMING_TYPE_ASF 	 0
#define STREAMING_TYPE_MP3 	 1


#include "url.h"

int connect2Server(char *host, int port);
int autodetectProtocol( URL_t *url, int *fd_out );

#endif
