/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __NETWORK_H
#define __NETWORK_H

#include <pthread.h>

#include "stream.h"

#include "url.h"

#define BUFFER_SIZE		2048

typedef enum {
	streaming_stopped_e,
	streaming_playing_e
} streaming_status;

typedef struct {
	char *buffer;
	int length;
} Net_Fifo;

typedef struct streaming_control {
	URL_t **url;
	int fd_net;
	int fd_pipe_in;
	streaming_status status;
	pthread_t thread_id;
	Net_Fifo *buffer;
	int buffering;	// boolean
	int prebuffer_size;
	int (*streaming_read)( struct streaming_control *stream_ctrl );
} streaming_ctrl_t;

Net_Fifo* net_fifo_new( );
void net_fifo_free(Net_Fifo *net_fifo );
int net_fifo_pop(Net_Fifo *net_fifo, char *buffer, int length );
int net_fifo_push(Net_Fifo *net_fifo, char *buffer, int length );

int connect2Server(char *host, int port);
int readFromServer(int fd, char *buffer, int length );
int autodetectProtocol( URL_t *url, int *fd_out );

#endif
