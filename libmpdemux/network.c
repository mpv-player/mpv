/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

//#define DUMP2FILE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <errno.h>
#include <ctype.h>

#include "config.h"

#include "stream.h"
#include "demuxer.h"

#include "network.h"
#include "http.h"
#include "url.h"
#include "asf.h"

streaming_ctrl_t *streaming_ctrl;

static ASF_StreamType_e streaming_type = ASF_Unknown_e;

Net_Fifo *
net_fifo_new() {
	Net_Fifo *net_fifo;
	net_fifo = (Net_Fifo*)malloc(sizeof(Net_Fifo));
	if( net_fifo==NULL ) {
		printf("Memory allocation failed\n");
		return NULL;
	}
	memset( net_fifo, 0, sizeof(Net_Fifo) );
	return net_fifo;
}

void
net_fifo_free( Net_Fifo *net_fifo ) {
	if( net_fifo->buffer!=NULL ) free( net_fifo->buffer );
	free( net_fifo );
}

int
net_fifo_push(Net_Fifo *net_fifo, char *buffer, int length ) {
	char *ptr;
	if( net_fifo==NULL || buffer==NULL || length<0 ) return -1;

	ptr = (char*)malloc(length+net_fifo->length);
	if( ptr==NULL ) {
		printf("Memory allocation failed\n");
		return -1;
	}
	if( net_fifo->buffer!=NULL ) {
		memcpy( ptr, net_fifo->buffer, net_fifo->length );
		free( net_fifo->buffer );
	}
	memcpy( ptr+net_fifo->length, buffer, length );
	net_fifo->buffer =  ptr; 
	net_fifo->length += length;
	return net_fifo->length;
}

int
net_fifo_pop(Net_Fifo *net_fifo, char *buffer, int length ) {
	char *ptr;
	int len;
	if( net_fifo==NULL || buffer==NULL || length<0 ) return -1;
	if( net_fifo->buffer==NULL || net_fifo->length==0 ) return -1;

	len = MIN(net_fifo->length, length);

	ptr = (char*)malloc(net_fifo->length-len);
	if( ptr==NULL ) {
		printf("Memory allocation failed\n");
		return -1;
	}
	memcpy( buffer, net_fifo->buffer, len );
	if( net_fifo->length-len!=0 ) {
		memcpy( ptr, net_fifo->buffer+len, net_fifo->length-len );
		free( net_fifo->buffer );
		net_fifo->buffer = ptr;
		net_fifo->length -= len;
	} else {
		free( net_fifo->buffer );
		net_fifo->buffer = NULL;
		net_fifo->length = 0;
	}
	return len;
}

streaming_ctrl_t *
streaming_ctrl_new( ) {
	streaming_ctrl_t *streaming_ctrl;
	streaming_ctrl = (streaming_ctrl_t*)malloc(sizeof(streaming_ctrl_t));
	if( streaming_ctrl==NULL ) {
		printf("Failed to allocate memory\n");
		return NULL;
	}
	memset( streaming_ctrl, 0, sizeof(streaming_ctrl_t) );
	streaming_ctrl->buffer = net_fifo_new();
	return streaming_ctrl;
}

void
streaming_ctrl_free( streaming_ctrl_t *streaming_ctrl ) {
	if( streaming_ctrl==NULL ) return;
	if( streaming_ctrl->buffer!=NULL ) net_fifo_free( streaming_ctrl->buffer );
	free( streaming_ctrl );
}

int
readFromServer(int fd, char *buffer, int length) {
	int ret;
	int done=0;
	fd_set set;
	struct timeval tv;
	if( buffer==NULL || length<0 ) return -1;

	
//	fcntl( fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK );
	return read( fd, buffer, length );

	do {
		tv.tv_sec = 0;
		tv.tv_usec = 10000;	// 10 milli-seconds timeout
		FD_ZERO( &set );
		FD_SET( fd, &set );
		ret = select( fd+1, &set, NULL, NULL, &tv );
		if( ret<0 ) {
			perror("select");
		} else if( ret==0 ) {
			printf("timeout\n");
		}
		if( FD_ISSET(fd, &set) ) {
			ret = read( fd, buffer, length );
			if( ret<0 ) {
				if( errno!=EINPROGRESS ) {
				}
			} else {
				done = 1;
			}
		} else {
			return -1;
		}
	} while( !done );

	return ret;
}

// Connect to a server using a TCP connection
int
connect2Server(char *host, int port) {
	int socket_server_fd;
	int err, err_len;
	fd_set set;
	struct timeval tv;
	struct sockaddr_in server_address;

	printf("Connecting to server %s:%d ...\n", host, port );

	socket_server_fd = socket(AF_INET, SOCK_STREAM, 0);
//	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) | O_NONBLOCK );
	if( socket_server_fd==-1 ) {
		perror("Failed to create socket");
		return -1;
	}

	if( isalpha(host[0]) ) {
		struct hostent *hp =(struct hostent*)gethostbyname( host );
		if( hp==NULL ) {
			printf("Counldn't resolve name: %s\n", host);
			return -1;
		}
		memcpy( (void*)&server_address.sin_addr.s_addr, (void*)hp->h_addr, hp->h_length );
	} else {
		inet_pton(AF_INET, host, &server_address.sin_addr);
	}
	server_address.sin_family=AF_INET;
	server_address.sin_port=htons(port);

	if( connect( socket_server_fd, (struct sockaddr*)&server_address, sizeof(server_address) )==-1 ) {
		if( errno!=EINPROGRESS ) {
			perror("Failed to connect to server");
			close(socket_server_fd);
			return -1;
		}
	}

	tv.tv_sec = 0;
	tv.tv_usec = 10000;	// 10 milli-seconds timeout
	FD_ZERO( &set );
	FD_SET( socket_server_fd, &set );
	if( select(socket_server_fd+1, NULL, &set, NULL, &tv)>0 ) {
		err_len = sizeof( err );
		getsockopt( socket_server_fd, SOL_SOCKET, SO_ERROR, &err, &err_len );
		if( err ) {
			printf("Couldn't connect to host %s\n", host );
			printf("Socket error: %d\n", err );
			close(socket_server_fd);
			return -1;
		}
	}
	return socket_server_fd;
}

int
http_send_request( URL_t *url ) {
	HTTP_header_t *http_hdr;
	int fd;
	http_hdr = http_new_header();
	http_set_uri( http_hdr, url->file );
	http_set_field( http_hdr, "User-Agent: MPlayer");
	http_set_field( http_hdr, "Connection: closed");
	if( http_build_request( http_hdr )==NULL ) {
		return -1;
	}

	fd = connect2Server( url->hostname, url->port );
	if( fd<0 ) {
		return -1; 
	}
	write( fd, http_hdr->buffer, http_hdr->buffer_size );
	http_free( http_hdr );

	return fd;
}

HTTP_header_t *
http_read_response( int fd ) {
	HTTP_header_t *http_hdr;
	char response[BUFFER_SIZE];
	int i;

	http_hdr = http_new_header();
	if( http_hdr==NULL ) {
		return NULL;
	}

	do {
		i = readFromServer( fd, response, BUFFER_SIZE ); 
		if( i<0 ) {
			printf("Read failed\n");
		}
		http_response_append( http_hdr, response, i );
	} while( !http_is_header_entire( http_hdr ) ); 
	http_response_parse( http_hdr );
	return http_hdr;
}

// By using the protocol, the extension of the file or the content-type
// we might be able to guess the streaming type.
int
autodetectProtocol(URL_t *url, int *fd_out) {
	HTTP_header_t *http_hdr;
	int fd=-1;
	int i;
	int redirect;
	char *extension;
	char *content_type;
	char *next_url;
	char response[1024];

	do {
		*fd_out=-1;
		next_url = NULL;
		extension = NULL;
		content_type = NULL;
		redirect = 0;

		if( url==NULL ) return DEMUXER_TYPE_UNKNOWN;

		// Get the extension of the file if present
		if( url->file!=NULL ) {
			for( i=strlen(url->file) ; i>0 ; i-- ) {
				if( url->file[i]=='.' ) {
					extension=(url->file)+i+1;
					break;
				}
			}
		}
//	extension=NULL;	
		if( extension!=NULL ) {
			printf("Extension: %s\n", extension );
			if( !strcasecmp(extension, "asf") ||
			    !strcasecmp(extension, "wmv") || 
			    !strcasecmp(extension, "asx") ) {
				if( url->port==0 ) url->port = 80;
				return DEMUXER_TYPE_ASF;
			}
			if( !strcasecmp(extension, "mpg") ||
			    !strcasecmp(extension, "mpeg") ) {
				if( url->port==0 ) url->port = 80;
				return DEMUXER_TYPE_MPEG_PS;
			}
			if( !strcasecmp(extension, "avi") ) {
				if( url->port==0 ) url->port = 80;
				return DEMUXER_TYPE_AVI;
			}
		}

		// Checking for RTSP
		if( !strcasecmp(url->protocol, "rtsp") ) {
			printf("RTSP protocol not yet implemented!\n");
			return DEMUXER_TYPE_UNKNOWN;
		}

		// Checking for ASF
		if( !strcasecmp(url->protocol, "mms") ) {
			if( url->port==0 ) url->port = 80;
			return DEMUXER_TYPE_ASF;
		}

		// HTTP based protocol
		if( !strcasecmp(url->protocol, "http") ) {
			if( url->port==0 ) url->port = 80;

			fd = http_send_request( url );
			if( fd<0 ) {
				*fd_out=-1;
				return DEMUXER_TYPE_UNKNOWN;
			}

			http_hdr = http_read_response( fd );
			if( http_hdr==NULL ) {
				close( fd );
				*fd_out=-1;
				return DEMUXER_TYPE_UNKNOWN;
			}

			*fd_out=fd;
			//http_debug_hdr( http_hdr );

			// Check if the response is an ICY status_code reason_phrase
			if( !strcasecmp(http_hdr->protocol, "ICY") ) {
				// Ok, we have detected an mp3 streaming
				return DEMUXER_TYPE_MPEG_PS;
			}
			
			switch( http_hdr->status_code ) {
				case 200: // OK
					// Look if we can use the Content-Type
					content_type = http_get_field( http_hdr, "Content-Type" );
					if( content_type!=NULL ) {
						printf("Content-Type: [%s]\n", content_type );
						printf("Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length") );
						// Check for ASF
						if( asf_http_streaming_type(content_type, NULL)!=ASF_Unknown_e ) {
							return DEMUXER_TYPE_ASF;
						}
						// Check for MP3 streaming
						// Some MP3 streaming server answer with audio/mpeg
						if( !strcasecmp(content_type, "audio/mpeg") ) {
							return DEMUXER_TYPE_MPEG_PS;
						}
						// Check for MPEG streaming
						if( !strcasecmp(content_type, "video/mpeg") ) {
							return DEMUXER_TYPE_MPEG_PS;
						}
						// AVI ??? => video/x-msvideo
						if( !strcasecmp(content_type, "video/x-msvideo") ) {
							return DEMUXER_TYPE_AVI;
						}
					}
					break;
				// Redirect
				case 301: // Permanently
				case 302: // Temporarily
					// TODO: RFC 2616, recommand to detect infinite redirection loops
					next_url = http_get_field( http_hdr, "Location" );
					if( next_url!=NULL ) {
						close( fd );
						url_free( url );
						url = url_new( next_url );
						redirect = 1;	
					}
					break;
				default:
					printf("Server returned %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
					close( fd );
					*fd_out=-1;
					return DEMUXER_TYPE_UNKNOWN;
			}
		}
	} while( redirect );

	return DEMUXER_TYPE_UNKNOWN;
}

int
nop_streaming_read( streaming_ctrl_t *streaming_ctrl ) {
	char *buffer;
	int len;
	if( streaming_ctrl==NULL ) return -1;
	len = streaming_ctrl->buffer->length;
	if( len==0 ) return 0;
		
	buffer = (char*)malloc( len );
        if( buffer==NULL ) {
                printf("Memory allocation failed\n");
                return -1;
        }
	net_fifo_pop( streaming_ctrl->buffer, buffer, len );
	write( streaming_ctrl->fd_pipe_in, buffer, len );
	free( buffer );
	return len;
}

int
nop_streaming_start( streaming_ctrl_t *streaming_ctrl ) {
	HTTP_header_t *http_hdr;
	int fd;
	if( streaming_ctrl==NULL ) return -1;

	fd = streaming_ctrl->fd_net;
	if( fd<0 ) {
		fd = http_send_request( *(streaming_ctrl->url) ); 
		if( fd<0 ) return -1;
		http_hdr = http_read_response( fd );
		if( http_hdr==NULL ) return -1;

		switch( http_hdr->status_code ) {
			case 200: // OK
				printf("Content-Type: [%s]\n", http_get_field(http_hdr, "Content-Type") );
				printf("Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length") );
				if( http_hdr->body_size>0 ) {
					write( streaming_ctrl->fd_pipe_in, http_hdr->body, http_hdr->body_size );
				}
				break;
			default:
				printf("Server return %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
				close( fd );
				fd = -1;
		}
		streaming_ctrl->fd_net = fd;
	}

	http_free( http_hdr );

	streaming_ctrl->streaming_read = nop_streaming_read;
	streaming_ctrl->prebuffer_size = 180000;
//	streaming_ctrl->prebuffer_size = 0;
	streaming_ctrl->buffering = 1;
//	streaming_ctrl->buffering = 0;
	streaming_ctrl->status = streaming_playing_e;
	return fd;
}

void
network_streaming(void *arg) {
	char buffer[BUFFER_SIZE];
	fd_set fd_net_in;
	int ret;

	arg = arg;

	do {
		FD_ZERO( &fd_net_in );
		FD_SET( streaming_ctrl->fd_net, &fd_net_in );
		
		ret = select( streaming_ctrl->fd_net+1, &fd_net_in, NULL, NULL, NULL );
		if( ret<0 ) {
			perror("select");
			return; //exit(1); // FIXME!
		}
		if( FD_ISSET( streaming_ctrl->fd_net, &fd_net_in ) ) {
			ret = readFromServer( streaming_ctrl->fd_net, buffer, BUFFER_SIZE );
			if( ret<=0 ) {
				streaming_ctrl->status=streaming_stopped_e;
			} else {
//printf("  push: 0x%02X\n", *((unsigned int*)buffer) );
				net_fifo_push( streaming_ctrl->buffer, buffer, ret );
				if( !streaming_ctrl->buffering ) {
					do {
						ret = streaming_ctrl->streaming_read( streaming_ctrl );
						if( ret<0 && streaming_ctrl->buffer->length<streaming_ctrl->prebuffer_size ) {
							// Need buffering
							streaming_ctrl->buffering = 1;
						}
					} while( streaming_ctrl->buffer->length>streaming_ctrl->prebuffer_size );
				} else {
					if( streaming_ctrl->buffer->length>streaming_ctrl->prebuffer_size ) {
						streaming_ctrl->buffering = 0;
						printf("\n");
					} else {
						printf(" Buffering: %d \%\r", (int)((float)(((float)streaming_ctrl->buffer->length)/((float)streaming_ctrl->prebuffer_size))*100) );
						fflush(stdout);
					}
				}
			}
		} else {
			printf("Network fd not set\n");
		}
	} while( streaming_ctrl->status==streaming_playing_e );

	// Flush the buffer
	while( streaming_ctrl->buffer->length>0 ) {
		ret = streaming_ctrl->streaming_read( streaming_ctrl );
		if( ret<0 ) break;
	}

printf("Network thread done\n");

	// Close to the pipe to stop mplayer.
	close( streaming_ctrl->fd_pipe_in );

}

int
streaming_start(URL_t **url, int fd, int streaming_type) {
	int fd_pipe[2];
	// Open the pipe
	if( pipe(fd_pipe)<0 ) {
		printf("Pipe creation failed\n");
		return -1;
	}

	streaming_ctrl = streaming_ctrl_new( ); 
	if( streaming_ctrl==NULL ) {
		return -1;
	}
	streaming_ctrl->url = url;
	streaming_ctrl->fd_pipe_in = fd_pipe[1];
	streaming_ctrl->fd_net = fd;

#ifdef DUMP2FILE
{
	int fd_file;
	fd_file = open("dump.stream", O_WRONLY | O_CREAT );
	if( fd_file<0 ) {
		perror("open");
	}
	streaming_ctrl->fd_pipe_in = fd_file;
}
#endif

	switch( streaming_type ) {
		case DEMUXER_TYPE_ASF:
			// Send the appropriate HTTP request
			fd = asf_http_streaming_start( streaming_ctrl );
			break;
		case DEMUXER_TYPE_AVI:
		case DEMUXER_TYPE_MPEG_ES:
		case DEMUXER_TYPE_MPEG_PS:
			fd = nop_streaming_start( streaming_ctrl );
			break;
		case DEMUXER_TYPE_UNKNOWN:
		default:
			printf("Unable to detect the streaming type\n");
			close( fd );
			free( streaming_ctrl );
			return -1;
	}

	if( fd<0 ) {
		free( streaming_ctrl );
		return -1;
	}

	// Start the network thread
	if( pthread_create( &(streaming_ctrl->thread_id), NULL , (void*)network_streaming, (void*)NULL)<0 ) {
		printf("Unable to start the network thread.\n");	
		close( fd );
		free( streaming_ctrl );
		return -1;
	}
printf("Network thread created with id: %d\n", streaming_ctrl->thread_id );
	
//	streaming_ctrl->status = streaming_stopped_e;

//	return fd;
	return fd_pipe[0];
}

int
streaming_stop( ) {
	streaming_ctrl->status = streaming_stopped_e;
	return 0;
}
