/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "network.h"
#include "http.h"
#include "url.h"
#include "asf.h"

// Connect to a server using a TCP connection
int
connect2Server(char *host, int port) {
	int socket_server_fd;
	struct sockaddr_in server_address;
	printf("Connecting to server %s:%d ...\n", host, port );
	socket_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if( socket_server_fd==-1 ) {
		perror("Failed to create socket");
		return -1;
	}

	if( isalpha(host[0]) ) {
		struct hostent *hp =(struct hostent*)gethostbyname( host );
		if( hp==NULL ) {
			printf("Unknown host: %s\n", host);
			return -1;
		}
		memcpy( (void*)&server_address.sin_addr.s_addr, (void*)hp->h_addr, hp->h_length );
	} else {
		inet_pton(AF_INET, host, &server_address.sin_addr);
	}
	server_address.sin_family=AF_INET;
	server_address.sin_port=htons(port);

	if( connect( socket_server_fd, (struct sockaddr*)&server_address, sizeof(server_address) )==-1 ) {
		perror("Failed to connect to server");
		close(socket_server_fd);
		return -1;
	}
	return socket_server_fd;
}

// By using the protocol, the extension of the file or the content-type
// we might be able to guess the streaming type.
int
autodetectProtocol(URL_t *url, int *fd_out) {
	HTTP_header_t *http_hdr;
	int fd=-1;
	int i;
	char *extension;
	char *content_type;
	char *next_url;
	char response[1024];

redo_request:
	*fd_out=-1;
	next_url = NULL;
	extension = NULL;
	content_type = NULL;

	if( url==NULL ) return STREAMING_TYPE_UNKNOWN;

	// Get the extension of the file if present
	if( url->file!=NULL ) {
		for( i=strlen(url->file) ; i>0 ; i-- ) {
			if( url->file[i]=='.' ) {
				extension=(url->file)+i+1;
				break;
			}
		}
	}
	
	if( extension!=NULL ) {
		printf("Extension: %s\n", extension );
		if( !strcasecmp(extension, "asf") ||
		    !strcasecmp(extension, "wmv") || 
		    !strcasecmp(extension, "asx") ) {
			if( url->port==0 ) url->port = 80;
			return STREAMING_TYPE_ASF;
		}
	}

	// Checking for RTSP
	if( !strcasecmp(url->protocol, "rtsp") ) {
		printf("RTSP protocol not yet implemented!\n");
		return STREAMING_TYPE_UNKNOWN;
	}

	// Checking for ASF
	if( !strcasecmp(url->protocol, "mms") ) {
		if( url->port==0 ) url->port = 80;
		return STREAMING_TYPE_ASF;
	}

	// HTTP based protocol
	if( !strcasecmp(url->protocol, "http") ) {
		if( url->port==0 ) url->port = 80;

		http_hdr = http_new_header();
		http_set_uri( http_hdr, url->file );
		http_set_field( http_hdr, "User-Agent: MPlayer");
		http_set_field( http_hdr, "Connection: closed");
		if( http_build_request( http_hdr )==NULL ) {
			return STREAMING_TYPE_UNKNOWN;
		}

		fd = connect2Server( url->hostname, url->port );
		if( fd<0 ) {
			*fd_out=-1;
			return STREAMING_TYPE_UNKNOWN;
		}
		write( fd, http_hdr->buffer, http_hdr->buffer_size );
//		http_free( http_hdr );

		http_hdr = http_new_header();
		if( http_hdr==NULL ) {
			close( fd );
			*fd_out=-1;
			return STREAMING_TYPE_UNKNOWN;
		}

		do {
			i = read( fd, response, 1024 ); 
			http_response_append( http_hdr, response, i );
		} while( !http_is_header_entired( http_hdr ) ); 
		http_response_parse( http_hdr );

		*fd_out=fd;
		//http_debug_hdr( http_hdr );

		// Check if the response is an ICY status_code reason_phrase
		if( !strcasecmp(http_hdr->protocol, "ICY") ) {
			// Ok, we have detected an mp3 streaming
			return STREAMING_TYPE_MP3;
		}
		
		switch( http_hdr->status_code ) {
			case 200: // OK
				// Look if we can use the Content-Type
				content_type = http_get_field( http_hdr, "Content-Type" );
				if( content_type!=NULL ) {
					printf("Content-Type: %s\n", content_type );
					// Check for ASF
					if( asf_http_streaming_type(content_type, NULL)!=ASF_Unknown_e ) {
						return STREAMING_TYPE_ASF;
					}
					// Check for MP3 streaming
					// Some MP3 streaming server answer with audio/mpeg
					if( !strcasecmp(content_type, "audio/mpeg") ) {
						return STREAMING_TYPE_MP3;
					}
				}
				break;
			// Redirect
			case 301: // Permanently
			case 302: // Temporarily
				// RFC 2616, recommand to detect infinite redirection loops
				next_url = http_get_field( http_hdr, "Location" );
				if( next_url!=NULL ) {
					close( fd );
					url_free( url );
					url_new( next_url );
					goto redo_request;	
				}
				//break;
			default:
				printf("Server returned %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
				close( fd );
				*fd_out=-1;
				return STREAMING_TYPE_UNKNOWN;
		}
	}
	return STREAMING_TYPE_UNKNOWN;
}


void
network_streaming() {
	int ret;
/*
	do {
		ret = select( );

	} while( );
*/
}

int
streaming_start(URL_t **url, int fd, int streaming_type) {
	switch( streaming_type ) {
		case STREAMING_TYPE_ASF:
			// Send a the appropriate HTTP request
			if( fd>0 ) close( fd );
			fd = asf_http_streaming_start( url );
			break;
		case STREAMING_TYPE_MP3:
			// Nothing else to do the server is already feeding the pipe.
			break;
		case STREAMING_TYPE_UNKNOWN:
		default:
			printf("Unable to detect the streaming type\n");
			close( fd );
			return -1;
	}

	return fd;
}

int
streaming_stop( ) {

}
