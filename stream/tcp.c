/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <ctype.h>

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#if !HAVE_WINSOCK2_H
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "network.h"
#include "stream.h"
#include "tcp.h"

/* IPv6 options */
int   network_prefer_ipv4 = 0;

// Converts an address family constant to a string

static const char *af2String(int af) {
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

static int
connect2Server_with_af(char *host, int port, int af,int verb) {
	int socket_server_fd;
	int err;
        socklen_t err_len;
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
	
#if HAVE_WINSOCK2_H
	unsigned long val;
	int to;
#else
	struct timeval to;
#endif
	
	socket_server_fd = socket(af, SOCK_STREAM, 0);
	
	
	if( socket_server_fd==-1 ) {
//		mp_msg(MSGT_NETWORK,MSGL_ERR,"Failed to create %s socket:\n", af2String(af));
		return TCP_ERROR_FATAL;
	}

#if defined(SO_RCVTIMEO) && defined(SO_SNDTIMEO)
#if HAVE_WINSOCK2_H
	/* timeout in milliseconds */
	to = 10 * 1000;
#else
	to.tv_sec = 10;
	to.tv_usec = 0;
#endif
	setsockopt(socket_server_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
	setsockopt(socket_server_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
#endif

	switch (af) {
		case AF_INET:  our_s_addr = (void *) &server_address.four.sin_addr; break;
#ifdef HAVE_AF_INET6
		case AF_INET6: our_s_addr = (void *) &server_address.six.sin6_addr; break;
#endif
		default:
			mp_msg(MSGT_NETWORK,MSGL_ERR, MSGTR_MPDEMUX_NW_UnknownAF, af);
			return TCP_ERROR_FATAL;
	}
	
	
	memset(&server_address, 0, sizeof(server_address));
	
#if HAVE_INET_PTON
	if (inet_pton(af, host, our_s_addr)!=1)
#elif HAVE_INET_ATON
	if (inet_aton(host, our_s_addr)!=1)
#elif HAVE_WINSOCK2_H
	if ( inet_addr(host)==INADDR_NONE )
#endif
	{
		if(verb) mp_msg(MSGT_NETWORK,MSGL_STATUS,MSGTR_MPDEMUX_NW_ResolvingHostForAF, host, af2String(af));
		
#ifdef HAVE_GETHOSTBYNAME2
		hp=(struct hostent*)gethostbyname2( host, af );
#else
		hp=(struct hostent*)gethostbyname( host );
#endif
		if( hp==NULL ) {
			if(verb) mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_NW_CantResolv, af2String(af), host);
			return TCP_ERROR_FATAL;
		}
		
		memcpy( our_s_addr, (void*)hp->h_addr_list[0], hp->h_length );
	}
#if HAVE_WINSOCK2_H
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
			mp_msg(MSGT_NETWORK,MSGL_ERR, MSGTR_MPDEMUX_NW_UnknownAF, af);
			return TCP_ERROR_FATAL;
	}

#if HAVE_INET_ATON || defined(HAVE_WINSOCK2_H)
	strncpy( buf, inet_ntoa( *((struct in_addr*)our_s_addr) ), 255);
#else
	inet_ntop(af, our_s_addr, buf, 255);
#endif
	if(verb) mp_msg(MSGT_NETWORK,MSGL_STATUS,MSGTR_MPDEMUX_NW_ConnectingToServer, host, buf , port );

	// Turn the socket as non blocking so we can timeout on the connection
#if !HAVE_WINSOCK2_H
	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) | O_NONBLOCK );
#else
	val = 1;
	ioctlsocket( socket_server_fd, FIONBIO, &val );
#endif
	if( connect( socket_server_fd, (struct sockaddr*)&server_address, server_address_size )==-1 ) {
#if !HAVE_WINSOCK2_H
		if( errno!=EINPROGRESS ) {
#else
		if( (WSAGetLastError() != WSAEINPROGRESS) && (WSAGetLastError() != WSAEWOULDBLOCK) ) {
#endif
			if(verb) mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_NW_CantConnect2Server, af2String(af));
			closesocket(socket_server_fd);
			return TCP_ERROR_PORT;
		}
	}
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	FD_ZERO( &set );
	FD_SET( socket_server_fd, &set );
	// When the connection will be made, we will have a writeable fd
	while((ret = select(socket_server_fd+1, NULL, &set, NULL, &tv)) == 0) {
	      if(count > 30 || stream_check_interrupt(500)) {
		if(count > 30)
		  mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_NW_ConnTimeout);
		else
		  mp_msg(MSGT_NETWORK,MSGL_V,"Connection interrupted by user\n");
		return TCP_ERROR_TIMEOUT;
	      }
	      count++;
	      FD_ZERO( &set );
	      FD_SET( socket_server_fd, &set );
	      tv.tv_sec = 0;
	      tv.tv_usec = 500000;
	}
	if (ret < 0) mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_NW_SelectFailed);

	// Turn back the socket as blocking
#if !HAVE_WINSOCK2_H
	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) & ~O_NONBLOCK );
#else
	val = 0;
	ioctlsocket( socket_server_fd, FIONBIO, &val );
#endif
	// Check if there were any errors
	err_len = sizeof(int);
	ret =  getsockopt(socket_server_fd,SOL_SOCKET,SO_ERROR,&err,&err_len);
	if(ret < 0) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_NW_GetSockOptFailed,strerror(errno));
		return TCP_ERROR_FATAL;
	}
	if(err > 0) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_NW_ConnectError,strerror(err));
		return TCP_ERROR_PORT;
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
	int s = TCP_ERROR_FATAL;

	r = connect2Server_with_af(host, port, network_prefer_ipv4 ? AF_INET:AF_INET6,verb);	
	if (r >= 0) return r;

	s = connect2Server_with_af(host, port, network_prefer_ipv4 ? AF_INET6:AF_INET,verb);
	if (s == TCP_ERROR_FATAL) return r;
	return s;
#else	
	return connect2Server_with_af(host, port, AF_INET,verb);
#endif

	
}
