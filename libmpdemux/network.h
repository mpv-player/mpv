/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __NETWORK_H
#define __NETWORK_H

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "url.h"
#include "http.h"
#include "stream.h"

#define BUFFER_SIZE		2048

typedef enum {
	streaming_stopped_e,
	streaming_playing_e
} streaming_status;

typedef struct streaming_control {
	URL_t *url;
	streaming_status status;
	int buffering;	// boolean
	unsigned int prebuffer_size;
	char *buffer;
	unsigned int buffer_size;
	unsigned int buffer_pos;
	unsigned int bandwidth;	// The downstream available
	int (*streaming_read)( int fd, char *buffer, int buffer_size, struct streaming_control *stream_ctrl );
	int (*streaming_seek)( int fd, off_t pos, struct streaming_control *stream_ctrl );
	void *data;
} streaming_ctrl_t;

//int streaming_start( stream_t *stream, int *demuxer_type, URL_t *url );

int streaming_bufferize( streaming_ctrl_t *streaming_ctrl, char *buffer, int size);

int nop_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl );
int nop_streaming_seek( int fd, off_t pos, streaming_ctrl_t *stream_ctrl );

int connect2Server(char *host, int port);

int http_send_request(URL_t *url);
HTTP_header_t *http_read_response(int fd);

/* 
 * Joey Parrish <joey@yunamusic.com>:
 *
 * This define is to allow systems without inet_pton() to fallback on
 * inet_aton().  The difference between the two is that inet_aton() is
 * strictly for IPv4 networking, while inet_pton() is for IPv4 and IPv6
 * both.  Slightly limited network functionality seems better than no
 * network functionality to me, and as all systems (Cygwin) start to
 * implement inet_pton(), configure will decide not to use this code.
 */
#ifdef USE_ATON
# define inet_pton(a, b, c) inet_aton(b, c)
#endif

#endif
