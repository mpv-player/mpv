/* Imported from the dvbstream-0.2 project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include "config.h"
#ifndef HAVE_WINSOCK2
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <errno.h>
#include "stream.h"

/* MPEG-2 TS RTP stack */

#define DEBUG        1
#include "rtp.h"

extern int network_bandwidth;


#define DEBUG        1
#include "../mp_msg.h"
#include "rtp.h"

// RTP reorder routines
// Also handling of repeated UDP packets (a bug of ExtremeNetworks switches firmware)
// rtpreord procedures
// write rtp packets in cache
// get rtp packets reordered

#define MAXRTPPACKETSIN 32   // The number of max packets being reordered

struct rtpbuffer
{
	unsigned char  data[MAXRTPPACKETSIN][STREAM_BUFFER_SIZE];
	unsigned short  seq[MAXRTPPACKETSIN];
	unsigned short  len[MAXRTPPACKETSIN];
	unsigned short first;
};
static struct rtpbuffer rtpbuf;

// RTP Reordering functions
// Algorithm works as follows:
// If next packet is in sequence just copy it to buffer
// Otherwise copy it in cache according to its sequence number
// Cache is a circular array where "rtpbuf.first" points to next sequence slot
// and keeps track of expected sequence

// Initialize rtp cache
static void rtp_cache_reset(unsigned short seq)
{
	int i;
	
	rtpbuf.first = 0;
	rtpbuf.seq[0] = ++seq;
	
	for (i=0; i<MAXRTPPACKETSIN; i++) {
		rtpbuf.len[i] = 0;
	}
}

// Write in a cache the rtp packet in right rtp sequence order
static int rtp_cache(int fd, char *buffer, int length)
{
	struct rtpheader rh;
	int newseq;
	char *data;
	unsigned short seq;
	static int is_first = 1;
	
	getrtp2(fd, &rh, &data, &length);
	if(!length)
		return 0;
	seq = rh.b.sequence;
	
	newseq = seq - rtpbuf.seq[rtpbuf.first];
	
	if ((newseq == 0) || is_first)
	{
		is_first = 0;
		
		//mp_msg(MSGT_NETWORK, MSGL_DBG4, "RTP (seq[%d]=%d seq=%d, newseq=%d)\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first], seq, newseq);
		rtpbuf.first = ( 1 + rtpbuf.first ) % MAXRTPPACKETSIN;
		rtpbuf.seq[rtpbuf.first] = ++seq;
		goto feed;
	}
	
	if (newseq > MAXRTPPACKETSIN)
	{
		mp_msg(MSGT_NETWORK, MSGL_DBG2, "Overrun(seq[%d]=%d seq=%d, newseq=%d)\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first], seq, newseq);
		rtp_cache_reset(seq);
		goto feed;
	}
	
	if (newseq < 0)
	{
		int i;
		
		// Is it a stray packet re-sent to network?
		for (i=0; i<MAXRTPPACKETSIN; i++) {
			if (rtpbuf.seq[i] == seq) {
				mp_msg(MSGT_NETWORK, MSGL_ERR, "Stray packet (seq[%d]=%d seq=%d, newseq=%d found at %d)\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first], seq, newseq, i);
				return  0; // Yes, it is!
			}
		}
		// Some heuristic to decide when to drop packet or to restart everything
		if (newseq > -(3 * MAXRTPPACKETSIN)) {
			mp_msg(MSGT_NETWORK, MSGL_ERR, "Too Old packet (seq[%d]=%d seq=%d, newseq=%d)\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first], seq, newseq);
			return  0; // Yes, it is!
		}
		
		mp_msg(MSGT_NETWORK, MSGL_ERR,  "Underrun(seq[%d]=%d seq=%d, newseq=%d)\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first], seq, newseq);
		
		rtp_cache_reset(seq);
		goto feed;
	}
	
	mp_msg(MSGT_NETWORK, MSGL_DBG4, "Out of Seq (seq[%d]=%d seq=%d, newseq=%d)\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first], seq, newseq);
	newseq = ( newseq + rtpbuf.first ) % MAXRTPPACKETSIN;
	memcpy (rtpbuf.data[newseq], data, length);
	rtpbuf.len[newseq] = length;
	rtpbuf.seq[newseq] = seq;
	
	return 0;

feed:
	memcpy (buffer, data, length);
	return length;
}

// Get next packet in cache
// Look in cache to get first packet in sequence
static int rtp_get_next(int fd, char *buffer, int length)
{
	int i;
	unsigned short nextseq;

	// If we have empty buffer we loop to fill it
	for (i=0; i < MAXRTPPACKETSIN -3; i++) {
		if (rtpbuf.len[rtpbuf.first] != 0) break;
		
		length = rtp_cache(fd, buffer, length) ;
		
		// returns on first packet in sequence 
		if (length > 0) {
			//mp_msg(MSGT_NETWORK, MSGL_DBG4, "Getting rtp [%d] %hu\n", i, rtpbuf.first);
			return length;
		} else if (length < 0) break;
		
		// Only if length == 0 loop continues!
	}
	
	i = rtpbuf.first;
	while (rtpbuf.len[i] == 0) {
		mp_msg(MSGT_NETWORK, MSGL_ERR,  "Lost packet %hu\n", rtpbuf.seq[i]);
		i = ( 1 + i ) % MAXRTPPACKETSIN;
		if (rtpbuf.first == i) break;
	}
	rtpbuf.first = i;
	
	// Copy next non empty packet from cache
	mp_msg(MSGT_NETWORK, MSGL_DBG4, "Getting rtp from cache [%d] %hu\n", rtpbuf.first, rtpbuf.seq[rtpbuf.first]);
	memcpy (buffer, rtpbuf.data[rtpbuf.first], rtpbuf.len[rtpbuf.first]);
	length = rtpbuf.len[rtpbuf.first]; // can be zero?
	
	// Reset fisrt slot and go next in cache
	rtpbuf.len[rtpbuf.first] = 0;
	nextseq = rtpbuf.seq[rtpbuf.first];
	rtpbuf.first = ( 1 + rtpbuf.first ) % MAXRTPPACKETSIN;
	rtpbuf.seq[rtpbuf.first] = nextseq + 1;
	
	return length;
}


// Read next rtp packet using cache 
int read_rtp_from_server(int fd, char *buffer, int length) {
	// Following test is ASSERT (i.e. uneuseful if code is correct)
	if(buffer==NULL || length<STREAM_BUFFER_SIZE) {
		mp_msg(MSGT_NETWORK, MSGL_ERR, "RTP buffer invalid; no data return from network\n");
		return 0;
	}
	
	// loop just to skip empty packets
	while ((length = rtp_get_next(fd, buffer, length)) == 0) {
		mp_msg(MSGT_NETWORK, MSGL_ERR, "Got empty packet from RTP cache!?\n");
	}
	
	return(length);
}

// Start listening on a UDP port. If multicast, join the group.
static int rtp_open_socket( URL_t *url ) {
	int socket_server_fd, rxsockbufsz;
	int err, err_len;
	fd_set set;
	struct sockaddr_in server_address;
	struct ip_mreq mcast;
        struct timeval tv;
	struct hostent *hp;

	mp_msg(MSGT_NETWORK,MSGL_V,"Listening for traffic on %s:%d ...\n", url->hostname, url->port );

	socket_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
//	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) | O_NONBLOCK );
	if( socket_server_fd==-1 ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Failed to create socket\n");
		return -1;
	}

	if( isalpha(url->hostname[0]) ) {
#ifndef HAVE_WINSOCK2
		hp =(struct hostent*)gethostbyname( url->hostname );
		if( hp==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Counldn't resolve name: %s\n", url->hostname);
			goto err_out;
		}
		memcpy( (void*)&server_address.sin_addr.s_addr, (void*)hp->h_addr_list[0], hp->h_length );
#else
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	} else {
#ifndef HAVE_WINSOCK2
#ifdef USE_ATON
		inet_aton(url->hostname, &server_address.sin_addr);
#else
		inet_pton(AF_INET, url->hostname, &server_address.sin_addr);
#endif
#else
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	}
	server_address.sin_family=AF_INET;
	server_address.sin_port=htons(url->port);

	if( bind( socket_server_fd, (struct sockaddr*)&server_address, sizeof(server_address) )==-1 ) {
#ifndef HAVE_WINSOCK2
		if( errno!=EINPROGRESS ) {
#else
		if( WSAGetLastError() != WSAEINPROGRESS ) {
#endif
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Failed to connect to server\n");
			goto err_out;
		}
	}
	
#ifdef HAVE_WINSOCK2
	if (isalpha(url->hostname[0])) {
		hp =(struct hostent*)gethostbyname( url->hostname );
		if( hp==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Counldn't resolve name: %s\n", url->hostname);
			goto err_out;
		}
		memcpy( (void*)&server_address.sin_addr.s_addr, (void*)hp->h_addr, hp->h_length );
	} else {
		unsigned int addr = inet_addr(url->hostname);
		memcpy( (void*)&server_address.sin_addr, (void*)&addr, sizeof(addr) );
	}
#endif

	// Increase the socket rx buffer size to maximum -- this is UDP
	rxsockbufsz = 240 * 1024;
	if( setsockopt( socket_server_fd, SOL_SOCKET, SO_RCVBUF, &rxsockbufsz, sizeof(rxsockbufsz))) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Couldn't set receive socket buffer size\n");
	}

	if((ntohl(server_address.sin_addr.s_addr) >> 28) == 0xe) {
		mcast.imr_multiaddr.s_addr = server_address.sin_addr.s_addr;
		//mcast.imr_interface.s_addr = inet_addr("10.1.1.2");
		mcast.imr_interface.s_addr = 0;
		if( setsockopt( socket_server_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcast, sizeof(mcast))) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"IP_ADD_MEMBERSHIP failed (do you have multicasting enabled in your kernel?)\n");
			goto err_out;
		}
	}

	tv.tv_sec = 0;
	tv.tv_usec = (1 * 1000000);	// 1 second timeout
	FD_ZERO( &set );
	FD_SET( socket_server_fd, &set );
	err = select(socket_server_fd+1, &set, NULL, NULL, &tv);
	if (err < 0) {
	  mp_msg(MSGT_NETWORK, MSGL_FATAL, "Select failed: %s\n", strerror(errno));
	  goto err_out;
	}
	if (err == 0) {
	  mp_msg(MSGT_NETWORK,MSGL_ERR,"Timeout! No data from host %s\n", url->hostname );
	  goto err_out;
	}
		err_len = sizeof( err );
		getsockopt( socket_server_fd, SOL_SOCKET, SO_ERROR, &err, &err_len );
		if( err ) {
			mp_msg(MSGT_NETWORK,MSGL_DBG2,"Socket error: %d\n", err );
			goto err_out;
		}
	return socket_server_fd;

err_out:
  closesocket(socket_server_fd);
  return -1;	
}

static int rtp_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *streaming_ctrl ) {
	return read_rtp_from_server( fd, buffer, size );
}

static int rtp_streaming_start( stream_t *stream, int raw_udp ) {
	streaming_ctrl_t *streaming_ctrl;
	int fd;

	if( stream==NULL ) return -1;
	streaming_ctrl = stream->streaming_ctrl;
	fd = stream->fd;
	
	if( fd<0 ) {
		fd = rtp_open_socket( (streaming_ctrl->url) ); 
		if( fd<0 ) return -1;
		stream->fd = fd;
	}

	if(raw_udp)
		streaming_ctrl->streaming_read = nop_streaming_read;
	else
		streaming_ctrl->streaming_read = rtp_streaming_read;
	streaming_ctrl->streaming_seek = nop_streaming_seek;
	streaming_ctrl->prebuffer_size = 64*1024;	// 64 KBytes	
	streaming_ctrl->buffering = 0;
	streaming_ctrl->status = streaming_playing_e;
	return 0;
}


static int getrtp2(int fd, struct rtpheader *rh, char** data, int* lengthData) {
  static char buf[1600];
  unsigned int intP;
  char* charP = (char*) &intP;
  int headerSize;
  int lengthPacket;
  lengthPacket=recv(fd,buf,1590,0);
  if (lengthPacket<0)
    mp_msg(MSGT_NETWORK,MSGL_ERR,"rtp: socket read error\n");
  else if (lengthPacket<12)
    mp_msg(MSGT_NETWORK,MSGL_ERR,"rtp: packet too small (%d) to be an rtp frame (>12bytes)\n", lengthPacket);
  if(lengthPacket<12) {
    *lengthData = 0;
    return 0;
  }
  rh->b.v  = (unsigned int) ((buf[0]>>6)&0x03);
  rh->b.p  = (unsigned int) ((buf[0]>>5)&0x01);
  rh->b.x  = (unsigned int) ((buf[0]>>4)&0x01);
  rh->b.cc = (unsigned int) ((buf[0]>>0)&0x0f);
  rh->b.m  = (unsigned int) ((buf[1]>>7)&0x01);
  rh->b.pt = (unsigned int) ((buf[1]>>0)&0x7f);
  intP = 0;
  memcpy(charP+2,&buf[2],2);
  rh->b.sequence = ntohl(intP);
  intP = 0;
  memcpy(charP,&buf[4],4);
  rh->timestamp = ntohl(intP);

  headerSize = 12 + 4*rh->b.cc; /* in bytes */

  *lengthData = lengthPacket - headerSize;
  *data = (char*) buf + headerSize;

  //  mp_msg(MSGT_NETWORK,MSGL_DBG2,"Reading rtp: v=%x p=%x x=%x cc=%x m=%x pt=%x seq=%x ts=%x lgth=%d\n",rh->b.v,rh->b.p,rh->b.x,rh->b.cc,rh->b.m,rh->b.pt,rh->b.sequence,rh->timestamp,lengthPacket);

  return(0);
}


static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  URL_t *url;
  int udp = 0;

  mp_msg(MSGT_OPEN, MSGL_INFO, "STREAM_RTP, URL: %s\n", stream->url);
  stream->streaming_ctrl = streaming_ctrl_new();
  if( stream->streaming_ctrl==NULL ) {
    return STREAM_ERROR;
  }
  stream->streaming_ctrl->bandwidth = network_bandwidth;
  url = url_new(stream->url);
  stream->streaming_ctrl->url = check4proxies(url);

  if( url->port==0 ) {
    mp_msg(MSGT_NETWORK,MSGL_ERR,"You must enter a port number for RTP and UDP streams!\n");
    goto fail;
  }
  if(!strncmp(stream->url, "udp", 3))
    udp = 1;

  if(rtp_streaming_start(stream, udp) < 0) {
    mp_msg(MSGT_NETWORK,MSGL_ERR,"rtp_streaming_start(rtp) failed\n");
    goto fail;
  }

  stream->type = STREAMTYPE_STREAM;
  fixup_network_stream_cache(stream);
  return STREAM_OK;

fail:
  streaming_ctrl_free( stream->streaming_ctrl );
  stream->streaming_ctrl = NULL;
  return STREAM_UNSUPORTED;
}


stream_info_t stream_info_rtp_udp = {
  "mpeg rtp and upd streaming",
  "rtp and udp",
  "Dave Chapman",
  "native rtp support",
  open_s,
  {"rtp", "udp", NULL},
  NULL,
  0 // Urls are an option string
};


