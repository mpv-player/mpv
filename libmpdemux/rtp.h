#ifndef _RTP_H
#define _RTP_H

#include <sys/socket.h>

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


void initrtp(struct rtpheader *foo); /* fill in the MPEG-2 TS deefaults */
int sendrtp(int fd, struct sockaddr_in *sSockAddr, struct rtpheader *foo, char *data, int len);
int getrtp2(int fd, struct rtpheader *rh, char** data, int* lengthData);
int sendrtp2(int fd, struct sockaddr_in *sSockAddr, struct rtpheader *foo, char *data, int len);
int getrtp(int fd, struct rtpheader *rh, char** data, int* lengthData);
int makesocket(char *szAddr,unsigned short port,int TTL,struct sockaddr_in *sSockAddr);
int makeclientsocket(char *szAddr,unsigned short port,int TTL,struct sockaddr_in *sSockAddr);

#endif
