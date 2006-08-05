/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __NETWORK_H
#define __NETWORK_H

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "config.h"
#ifndef HAVE_WINSOCK2
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "url.h"
#include "http.h"
#include "stream.h"

#define BUFFER_SIZE		2048

typedef struct {
	const char *mime_type;
	int demuxer_type;
} mime_struct_t;

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
streaming_ctrl_t *streaming_ctrl_new(void);
int streaming_bufferize( streaming_ctrl_t *streaming_ctrl, char *buffer, int size);

int nop_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl );
int nop_streaming_seek( int fd, off_t pos, streaming_ctrl_t *stream_ctrl );
void streaming_ctrl_free( streaming_ctrl_t *streaming_ctrl );

int http_send_request(URL_t *url, off_t pos);
HTTP_header_t *http_read_response(int fd);

int http_authenticate(HTTP_header_t *http_hdr, URL_t *url, int *auth_retry);
URL_t* check4proxies(URL_t *url);

#endif
