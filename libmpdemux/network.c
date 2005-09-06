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

#include <errno.h>
#include <ctype.h>

#include "config.h"

#ifndef HAVE_WINSOCK2
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "stream.h"
#include "demuxer.h"
#include "../m_config.h"

#include "network.h"
#include "http.h"
#include "cookies.h"
#include "url.h"

#include "../version.h"

extern int verbose;
extern int stream_cache_size;

extern int mp_input_check_interrupt(int time);

/* Variables for the command line option -user, -passwd, -bandwidth,
   -user-agent and -nocookies */

char *network_username=NULL;
char *network_password=NULL;
int   network_bandwidth=0;
int   network_cookies_enabled = 0;
char *network_useragent=NULL;

/* IPv6 options */
int   network_prefer_ipv4 = 0;
int   network_ipv4_only_proxy = 0;


mime_struct_t mime_type_table[] = {
	// MP3 streaming, some MP3 streaming server answer with audio/mpeg
	{ "audio/mpeg", DEMUXER_TYPE_AUDIO },
	// MPEG streaming
	{ "video/mpeg", DEMUXER_TYPE_UNKNOWN },
	{ "video/x-mpeg", DEMUXER_TYPE_UNKNOWN },
	{ "video/x-mpeg2", DEMUXER_TYPE_UNKNOWN },
	// AVI ??? => video/x-msvideo
	{ "video/x-msvideo", DEMUXER_TYPE_AVI },
	// MOV => video/quicktime
	{ "video/quicktime", DEMUXER_TYPE_MOV },
	// ASF
        { "audio/x-ms-wax", DEMUXER_TYPE_ASF },
	{ "audio/x-ms-wma", DEMUXER_TYPE_ASF },
	{ "video/x-ms-asf", DEMUXER_TYPE_ASF },
	{ "video/x-ms-afs", DEMUXER_TYPE_ASF },
	{ "video/x-ms-wvx", DEMUXER_TYPE_ASF },
	{ "video/x-ms-wmv", DEMUXER_TYPE_ASF },
	{ "video/x-ms-wma", DEMUXER_TYPE_ASF },
	// Playlists
	{ "video/x-ms-wmx", DEMUXER_TYPE_PLAYLIST },
	{ "audio/x-scpls", DEMUXER_TYPE_PLAYLIST },
	{ "audio/x-mpegurl", DEMUXER_TYPE_PLAYLIST },
	{ "audio/x-pls", DEMUXER_TYPE_PLAYLIST },
	// Real Media
//	{ "audio/x-pn-realaudio", DEMUXER_TYPE_REAL },
	// OGG Streaming
	{ "application/x-ogg", DEMUXER_TYPE_OGG },
	// NullSoft Streaming Video
	{ "video/nsv", DEMUXER_TYPE_NSV},
	{ "misc/ultravox", DEMUXER_TYPE_NSV},
	{ NULL, DEMUXER_TYPE_UNKNOWN},
};


streaming_ctrl_t *
streaming_ctrl_new( ) {
	streaming_ctrl_t *streaming_ctrl;
	streaming_ctrl = (streaming_ctrl_t*)malloc(sizeof(streaming_ctrl_t));
	if( streaming_ctrl==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Failed to allocate memory\n");
		return NULL;
	}
	memset( streaming_ctrl, 0, sizeof(streaming_ctrl_t) );
	return streaming_ctrl;
}

void
streaming_ctrl_free( streaming_ctrl_t *streaming_ctrl ) {
	if( streaming_ctrl==NULL ) return;
	if( streaming_ctrl->url ) url_free( streaming_ctrl->url );
	if( streaming_ctrl->buffer ) free( streaming_ctrl->buffer );
	if( streaming_ctrl->data ) free( streaming_ctrl->data );
	free( streaming_ctrl );
}


// Converts an address family constant to a string

char *af2String(int af) {
	switch (af) {
		case AF_INET:	return "AF_INET";
		
#ifdef HAVE_AF_INET6
		case AF_INET6:	return "AF_INET6";
#endif
		default:	return "Unknown address family!";
	}
}



// Connect to a server using a TCP connection, with specified address family
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port

int
connect2Server_with_af(char *host, int port, int af,int verb) {
	int socket_server_fd;
	int err, err_len;
	int ret,count = 0;
	fd_set set;
	struct timeval tv;
	union {
		struct sockaddr_in four;
#ifdef HAVE_AF_INET6
		struct sockaddr_in6 six;
#endif
	} server_address;
	size_t server_address_size;
	void *our_s_addr;	// Pointer to sin_addr or sin6_addr
	struct hostent *hp=NULL;
	char buf[255];
	
#ifdef HAVE_WINSOCK2
	u_long val;
#endif
	
	socket_server_fd = socket(af, SOCK_STREAM, 0);
	
	
	if( socket_server_fd==-1 ) {
//		mp_msg(MSGT_NETWORK,MSGL_ERR,"Failed to create %s socket:\n", af2String(af));
		return -2;
	}

	switch (af) {
		case AF_INET:  our_s_addr = (void *) &server_address.four.sin_addr; break;
#ifdef HAVE_AF_INET6
		case AF_INET6: our_s_addr = (void *) &server_address.six.sin6_addr; break;
#endif
		default:
			mp_msg(MSGT_NETWORK,MSGL_ERR, "Unknown address family %d:\n", af);
			return -2;
	}
	
	
	memset(&server_address, 0, sizeof(server_address));
	
#ifndef HAVE_WINSOCK2
#ifdef USE_ATON
	if (inet_aton(host, our_s_addr)!=1)
#else
	if (inet_pton(af, host, our_s_addr)!=1)
#endif
#else
	if ( inet_addr(host)==INADDR_NONE )
#endif
	{
		if(verb) mp_msg(MSGT_NETWORK,MSGL_STATUS,"Resolving %s for %s...\n", host, af2String(af));
		
#ifdef HAVE_GETHOSTBYNAME2
		hp=(struct hostent*)gethostbyname2( host, af );
#else
		hp=(struct hostent*)gethostbyname( host );
#endif
		if( hp==NULL ) {
			if(verb) mp_msg(MSGT_NETWORK,MSGL_ERR,"Couldn't resolve name for %s: %s\n", af2String(af), host);
			return -2;
		}
		
		memcpy( our_s_addr, (void*)hp->h_addr, hp->h_length );
	}
#ifdef HAVE_WINSOCK2
	else {
		unsigned long addr = inet_addr(host);
		memcpy( our_s_addr, (void*)&addr, sizeof(addr) );
	}
#endif
	
	switch (af) {
		case AF_INET:
			server_address.four.sin_family=af;
			server_address.four.sin_port=htons(port);			
			server_address_size = sizeof(server_address.four);
			break;
#ifdef HAVE_AF_INET6		
		case AF_INET6:
			server_address.six.sin6_family=af;
			server_address.six.sin6_port=htons(port);
			server_address_size = sizeof(server_address.six);
			break;
#endif
		default:
			mp_msg(MSGT_NETWORK,MSGL_ERR, "Unknown address family %d:\n", af);
			return -2;
	}

#if defined(USE_ATON) || defined(HAVE_WINSOCK2)
	strncpy( buf, inet_ntoa( *((struct in_addr*)our_s_addr) ), 255);
#else
	inet_ntop(af, our_s_addr, buf, 255);
#endif
	if(verb) mp_msg(MSGT_NETWORK,MSGL_STATUS,"Connecting to server %s[%s]:%d ...\n", host, buf , port );

	// Turn the socket as non blocking so we can timeout on the connection
#ifndef HAVE_WINSOCK2
	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) | O_NONBLOCK );
#else
	val = 1;
	ioctlsocket( socket_server_fd, FIONBIO, &val );
#endif
	if( connect( socket_server_fd, (struct sockaddr*)&server_address, server_address_size )==-1 ) {
#ifndef HAVE_WINSOCK2
		if( errno!=EINPROGRESS ) {
#else
		if( (WSAGetLastError() != WSAEINPROGRESS) && (WSAGetLastError() != WSAEWOULDBLOCK) ) {
#endif
			if(verb) mp_msg(MSGT_NETWORK,MSGL_ERR,"Failed to connect to server with %s\n", af2String(af));
			closesocket(socket_server_fd);
			return -1;
		}
	}
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	FD_ZERO( &set );
	FD_SET( socket_server_fd, &set );
	// When the connection will be made, we will have a writable fd
	while((ret = select(socket_server_fd+1, NULL, &set, NULL, &tv)) == 0) {
	      if( ret<0 ) mp_msg(MSGT_NETWORK,MSGL_ERR,"select failed\n");
	      else if(ret > 0) break;
	      else if(count > 30 || mp_input_check_interrupt(500)) {
		if(count > 30)
		  mp_msg(MSGT_NETWORK,MSGL_ERR,"Connection timeout\n");
		else
		  mp_msg(MSGT_NETWORK,MSGL_V,"Connection interuppted by user\n");
		return -3;
	      }
	      count++;
	      FD_ZERO( &set );
	      FD_SET( socket_server_fd, &set );
	      tv.tv_sec = 0;
	      tv.tv_usec = 500000;
	}

	// Turn back the socket as blocking
#ifndef HAVE_WINSOCK2
	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) & ~O_NONBLOCK );
#else
	val = 0;
	ioctlsocket( socket_server_fd, FIONBIO, &val );
#endif
	// Check if there were any error
	err_len = sizeof(int);
	ret =  getsockopt(socket_server_fd,SOL_SOCKET,SO_ERROR,&err,&err_len);
	if(ret < 0) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"getsockopt failed : %s\n",strerror(errno));
		return -2;
	}
	if(err > 0) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Connect error : %s\n",strerror(err));
		return -1;
	}
	
	return socket_server_fd;
}

// Connect to a server using a TCP connection
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port


int
connect2Server(char *host, int  port, int verb) {
#ifdef HAVE_AF_INET6
	int r;
	int s = -2;

	r = connect2Server_with_af(host, port, network_prefer_ipv4 ? AF_INET:AF_INET6,verb);	
	if (r > -1) return r;

	s = connect2Server_with_af(host, port, network_prefer_ipv4 ? AF_INET6:AF_INET,verb);
	if (s == -2) return r;
	return s;
#else	
	return connect2Server_with_af(host, port, AF_INET,verb);
#endif

	
}

URL_t*
check4proxies( URL_t *url ) {
	URL_t *url_out = NULL;
	if( url==NULL ) return NULL;
	url_out = url_new( url->url );
	if( !strcasecmp(url->protocol, "http_proxy") ) {
		mp_msg(MSGT_NETWORK,MSGL_V,"Using HTTP proxy: http://%s:%d\n", url->hostname, url->port );
		return url_out;
	}
	// Check if the http_proxy environment variable is set.
	if( !strcasecmp(url->protocol, "http") ) {
		char *proxy;
		proxy = getenv("http_proxy");
		if( proxy!=NULL ) {
			// We got a proxy, build the URL to use it
			int len;
			char *new_url;
			URL_t *tmp_url;
			URL_t *proxy_url = url_new( proxy );

			if( proxy_url==NULL ) {
				mp_msg(MSGT_NETWORK,MSGL_WARN,"Invalid proxy setting...Trying without proxy.\n");
				return url_out;
			}
			
#ifdef HAVE_AF_INET6
			if (network_ipv4_only_proxy && (gethostbyname(url->hostname)==NULL)) {
				mp_msg(MSGT_NETWORK,MSGL_WARN,
					"Could not find resolve remote hostname for AF_INET. Trying without proxy.\n");
				url_free(proxy_url);
				return url_out;
			}
#endif

			mp_msg(MSGT_NETWORK,MSGL_V,"Using HTTP proxy: %s\n", proxy_url->url );
			len = strlen( proxy_url->hostname ) + strlen( url->url ) + 20;	// 20 = http_proxy:// + port
			new_url = malloc( len+1 );
			if( new_url==NULL ) {
				mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
				url_free(proxy_url);
				return url_out;
			}
			sprintf(new_url, "http_proxy://%s:%d/%s", proxy_url->hostname, proxy_url->port, url->url );
			tmp_url = url_new( new_url );
			if( tmp_url==NULL ) {
				free( new_url );
				url_free( proxy_url );
				return url_out;
			}
			url_free( url_out );
			url_out = tmp_url;
			free( new_url );
			url_free( proxy_url );
		}
	}
	return url_out;
}

int
http_send_request( URL_t *url, off_t pos ) {
	HTTP_header_t *http_hdr;
	URL_t *server_url;
	char str[256];
	int fd;
	int ret;
	int proxy = 0;		// Boolean

	http_hdr = http_new_header();

	if( !strcasecmp(url->protocol, "http_proxy") ) {
		proxy = 1;
		server_url = url_new( (url->file)+1 );
		http_set_uri( http_hdr, server_url->url );
	} else {
		server_url = url;
		http_set_uri( http_hdr, server_url->file );
	}
	if (server_url->port && server_url->port != 80)
	    snprintf(str, 256, "Host: %s:%d", server_url->hostname, server_url->port );
	else
	    snprintf(str, 256, "Host: %s", server_url->hostname );
	http_set_field( http_hdr, str);
	if (network_useragent)
	{
	    snprintf(str, 256, "User-Agent: %s", network_useragent);
	    http_set_field(http_hdr, str);
	}
	else
	    http_set_field( http_hdr, "User-Agent: MPlayer/"VERSION);

	http_set_field(http_hdr, "Icy-MetaData: 1");

	if(pos>0) { 
	// Extend http_send_request with possibility to do partial content retrieval
#ifdef __MINGW32__
	    snprintf(str, 256, "Range: bytes=%I64d-", (int64_t)pos);
#else
	    snprintf(str, 256, "Range: bytes=%lld-", (long long)pos);
#endif
	    http_set_field(http_hdr, str);
	}
	    
	if (network_cookies_enabled) cookies_set( http_hdr, server_url->hostname, server_url->url );
	
	http_set_field( http_hdr, "Connection: close");
	http_add_basic_authentication( http_hdr, url->username, url->password );
	if( http_build_request( http_hdr )==NULL ) {
		goto err_out;
	}

	if( proxy ) {
		if( url->port==0 ) url->port = 8080;			// Default port for the proxy server
		fd = connect2Server( url->hostname, url->port,1 );
		url_free( server_url );
	} else {
		if( server_url->port==0 ) server_url->port = 80;	// Default port for the web server
		fd = connect2Server( server_url->hostname, server_url->port,1 );
	}
	if( fd<0 ) {
		goto err_out;
	}
	mp_msg(MSGT_NETWORK,MSGL_DBG2,"Request: [%s]\n", http_hdr->buffer );
	
	ret = send( fd, http_hdr->buffer, http_hdr->buffer_size, 0 );
	if( ret!=(int)http_hdr->buffer_size ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Error while sending HTTP request: didn't sent all the request\n");
		goto err_out;
	}
	
	http_free( http_hdr );

	return fd;
err_out:
	http_free(http_hdr);
	return -1;
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
		i = recv( fd, response, BUFFER_SIZE, 0 ); 
		if( i<0 ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Read failed\n");
			http_free( http_hdr );
			return NULL;
		}
		if( i==0 ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"http_read_response read 0 -ie- EOF\n");
			http_free( http_hdr );
			return NULL;
		}
		http_response_append( http_hdr, response, i );
	} while( !http_is_header_entire( http_hdr ) ); 
	http_response_parse( http_hdr );
	return http_hdr;
}

int
http_authenticate(HTTP_header_t *http_hdr, URL_t *url, int *auth_retry) {
	char *aut;

	if( *auth_retry==1 ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Authentication failed\n");
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Please use the option -user and -passwd to provide your username/password for a list of URLs,\n");
		mp_msg(MSGT_NETWORK,MSGL_ERR,"or form an URL like: http://username:password@hostname/file\n");
		return -1;
	}
	if( *auth_retry>0 ) {
		if( url->username ) {
			free( url->username );
			url->username = NULL;
		}
		if( url->password ) {
			free( url->password );
			url->password = NULL;
		}
	}

	aut = http_get_field(http_hdr, "WWW-Authenticate");
	if( aut!=NULL ) {
		char *aut_space;
		aut_space = strstr(aut, "realm=");
		if( aut_space!=NULL ) aut_space += 6;
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Authentication required for %s\n", aut_space);
	} else {
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Authentication required\n");
	}
	if( network_username ) {
		url->username = strdup(network_username);
		if( url->username==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
			return -1;
		}
	} else {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Unable to read the username\n");
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Please use the option -user and -passwd to provide your username/password for a list of URLs,\n");
		mp_msg(MSGT_NETWORK,MSGL_ERR,"or form an URL like: http://username:password@hostname/file\n");
		return -1;
	}
	if( network_password ) {
		url->password = strdup(network_password);
		if( url->password==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
			return -1;
		}
	} else {
		mp_msg(MSGT_NETWORK,MSGL_INFO,"No password provided, trying blank password\n");
	}
	(*auth_retry)++;
	return 0;
}

int
http_seek( stream_t *stream, off_t pos ) {
	HTTP_header_t *http_hdr = NULL;
	int fd;
	if( stream==NULL ) return 0;

	if( stream->fd>0 ) closesocket(stream->fd); // need to reconnect to seek in http-stream
	fd = http_send_request( stream->streaming_ctrl->url, pos ); 
	if( fd<0 ) return 0;

	http_hdr = http_read_response( fd );

	if( http_hdr==NULL ) return 0;

	switch( http_hdr->status_code ) {
		case 200:
		case 206: // OK
			mp_msg(MSGT_NETWORK,MSGL_V,"Content-Type: [%s]\n", http_get_field(http_hdr, "Content-Type") );
			mp_msg(MSGT_NETWORK,MSGL_V,"Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length") );
			if( http_hdr->body_size>0 ) {
				if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
					http_free( http_hdr );
					return -1;
				}
			}
			break;
		default:
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Server return %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
			close( fd );
			fd = -1;
	}
	stream->fd = fd;

	if( http_hdr ) {
		http_free( http_hdr );
		stream->streaming_ctrl->data = NULL;
	}

	stream->pos=pos;

	return 1;
}


int
streaming_bufferize( streaming_ctrl_t *streaming_ctrl, char *buffer, int size) {
//printf("streaming_bufferize\n");
	streaming_ctrl->buffer = (char*)malloc(size);
	if( streaming_ctrl->buffer==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return -1;
	}
	memcpy( streaming_ctrl->buffer, buffer, size );
	streaming_ctrl->buffer_size = size;
	return size;
}

int
nop_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl ) {
	int len=0;
//printf("nop_streaming_read\n");
	if( stream_ctrl->buffer_size!=0 ) {
		int buffer_len = stream_ctrl->buffer_size-stream_ctrl->buffer_pos;
//printf("%d bytes in buffer\n", stream_ctrl->buffer_size);
		len = (size<buffer_len)?size:buffer_len;
		memcpy( buffer, (stream_ctrl->buffer)+(stream_ctrl->buffer_pos), len );
		stream_ctrl->buffer_pos += len;
//printf("buffer_pos = %d\n", stream_ctrl->buffer_pos );
		if( stream_ctrl->buffer_pos>=stream_ctrl->buffer_size ) {
			free( stream_ctrl->buffer );
			stream_ctrl->buffer = NULL;
			stream_ctrl->buffer_size = 0;
			stream_ctrl->buffer_pos = 0;
//printf("buffer cleaned\n");
		}
//printf("read %d bytes from buffer\n", len );
	}

	if( len<size ) {
		int ret;
		ret = recv( fd, buffer+len, size-len, 0 );
		if( ret<0 ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"nop_streaming_read error : %s\n",strerror(errno));
		}
		len += ret;
//printf("read %d bytes from network\n", len );
	}
	
	return len;
}

int
nop_streaming_seek( int fd, off_t pos, streaming_ctrl_t *stream_ctrl ) {
	return -1;
	// To shut up gcc warning
	fd++;
	pos++;
	stream_ctrl=NULL;
}


void fixup_network_stream_cache(stream_t *stream) {
  if(stream->streaming_ctrl->buffering) {
    if(stream_cache_size<0) {
      // cache option not set, will use our computed value.
      // buffer in KBytes, *5 because the prefill is 20% of the buffer.
      stream_cache_size = (stream->streaming_ctrl->prebuffer_size/1024)*5;
      if( stream_cache_size<64 ) stream_cache_size = 64;	// 16KBytes min buffer
    }
    mp_msg(MSGT_NETWORK,MSGL_INFO,"Cache size set to %d KBytes\n", stream_cache_size);
  }
}


int
streaming_stop( stream_t *stream ) {
	stream->streaming_ctrl->status = streaming_stopped_e;
	return 0;
}
