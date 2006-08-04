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
#include "mp_msg.h"
#include "rtp.h"

// RTP reorder routines
// Also handling of repeated UDP packets (a bug of ExtremeNetworks switches firmware)
// rtpreord procedures
// write rtp packets in cache
// get rtp packets reordered

#define MAXRTPPACKETSIN 32   // The number of max packets being reordered

struct rtpbits {
  unsigned int v:2;           /* version: 2 */
  unsigned int p:1;           /* is there padding appended: 0 */
  unsigned int x:1;           /* number of extension headers: 0 */
  unsigned int cc:4;          /* number of CSRC identifiers: 0 */
  unsigned int m:1;           /* marker: 0 */
  unsigned int pt:7;          /* payload type: 33 for MPEG2 TS - RFC 1890 */
  unsigned int sequence:16;   /* sequence number: random */
};

struct rtpheader {	/* in network byte order */
  struct rtpbits b;
  int timestamp;	/* start: random */
  int ssrc;		/* random */
};

struct rtpbuffer
{
	unsigned char  data[MAXRTPPACKETSIN][STREAM_BUFFER_SIZE];
	unsigned short  seq[MAXRTPPACKETSIN];
	unsigned short  len[MAXRTPPACKETSIN];
	unsigned short first;
};
static struct rtpbuffer rtpbuf;

static int getrtp2(int fd, struct rtpheader *rh, char** data, int* lengthData);

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
