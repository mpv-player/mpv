/* Imported from the dvbstream-0.2 project */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* MPEG-2 TS RTP stack */

#define DEBUG        1
#include "rtp.h"

void initrtp(struct rtpheader *foo) { /* fill in the MPEG-2 TS deefaults */
  /* Note: MPEG-2 TS defines a timestamping base frequency of 90000 Hz. */
  foo->b.v=2;
  foo->b.p=0;
  foo->b.x=0;
  foo->b.cc=0;
  foo->b.m=0;
  foo->b.pt=33;                     /* MPEG-2 TS */
  foo->b.sequence=rand() & 65535;
  foo->timestamp=rand();
  foo->ssrc=rand();
}

/* Send a single RTP packet, converting the RTP header to network byte order. */
int sendrtp(int fd, struct sockaddr_in *sSockAddr, struct rtpheader *foo, char *data, int len) {
  char *buf=(char*)alloca(len+sizeof(struct rtpheader));
  int *cast=(int *)foo;
  int *outcast=(int *)buf;
  outcast[0]=htonl(cast[0]);
  outcast[1]=htonl(cast[1]);
  memmove(outcast+2,data,len);
  fprintf(stderr,"v=%x %x\n",foo->b.v,buf[0]);
  return sendto(fd,buf,len+3,0,(struct sockaddr *)sSockAddr,sizeof(*sSockAddr));
}

int getrtp2(int fd, struct rtpheader *rh, char** data, int* lengthData) {
  static char buf[1600];
  unsigned int intP;
  char* charP = (char*) &intP;
  int headerSize;
  int lengthPacket;
  lengthPacket=recv(fd,buf,1590,0);
  if (lengthPacket==0)
    exit(1);
  if (lengthPacket<0) {
    fprintf(stderr,"socket read error\n");
    exit(2);
  }
  if (lengthPacket<12) {
    fprintf(stderr,"packet too small (%d) to be an rtp frame (>12bytes)\n", lengthPacket);
    exit(3);
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

  //  fprintf(stderr,"Reading rtp: v=%x p=%x x=%x cc=%x m=%x pt=%x seq=%x ts=%x lgth=%d\n",rh->b.v,rh->b.p,rh->b.x,rh->b.cc,rh->b.m,rh->b.pt,rh->b.sequence,rh->timestamp,lengthPacket);

  return(0);
}

/* Send a single RTP packet, converting the RTP header to network byte order. */
int sendrtp2(int fd, struct sockaddr_in *sSockAddr, struct rtpheader *foo, char *data, int len) {
  char *buf=(char*)alloca(len+72);
  unsigned int intP;
  char* charP = (char*) &intP;
  int headerSize;
  buf[0]  = 0x00;
  buf[0] |= ((((char) foo->b.v)<<6)&0xc0);
  buf[0] |= ((((char) foo->b.p)<<5)&0x20);
  buf[0] |= ((((char) foo->b.x)<<4)&0x10);
  buf[0] |= ((((char) foo->b.cc)<<0)&0x0f);
  buf[1]  = 0x00;
  buf[1] |= ((((char) foo->b.m)<<7)&0x80);
  buf[1] |= ((((char) foo->b.pt)<<0)&0x7f);
  intP = htonl(foo->b.sequence);
  memcpy(&buf[2],charP+2,2);
  intP = htonl(foo->timestamp);
  memcpy(&buf[4],&intP,4);
  /* SSRC: not implemented */
  buf[8]  = 0x0f;
  buf[9]  = 0x0f;
  buf[10] = 0x0f;
  buf[11] = 0x0f;
  headerSize = 12 + 4*foo->b.cc; /* in bytes */
  memcpy(buf+headerSize,data,len);

  //  fprintf(stderr,"Sending rtp: v=%x p=%x x=%x cc=%x m=%x pt=%x seq=%x ts=%x lgth=%d\n",foo->b.v,foo->b.p,foo->b.x,foo->b.cc,foo->b.m,foo->b.pt,foo->b.sequence,foo->timestamp,len+headerSize);

  foo->b.sequence++;
  return sendto(fd,buf,len+headerSize,0,(struct sockaddr *)sSockAddr,sizeof(*sSockAddr));
}


int getrtp(int fd, struct rtpheader *rh, char** data, int* lengthData) {
  static char buf[1600];
  int headerSize;
  int lengthPacket;

  lengthPacket=recv(fd,buf,1590,0);
  // FIXME: error handling to write here
  headerSize = 3;
  *lengthData = lengthPacket - headerSize;
  *data = (char*) buf + headerSize;
  fprintf(stderr,"[%d] %02x %x\n",lengthPacket,buf[8],buf[0]);
  return(0);
}

/* create a sender socket. */
int makesocket(char *szAddr,unsigned short port,int TTL,struct sockaddr_in *sSockAddr) {
  int          iRet, iLoop = 1;
  struct       sockaddr_in sin;
  char         cTtl = (char)TTL;
  char         cLoop=0;

  int iSocket = socket( AF_INET, SOCK_DGRAM, 0 );

  if (iSocket < 0) {
    fprintf(stderr,"socket() failed.\n");
    exit(1);
  }

  sSockAddr->sin_family = sin.sin_family = AF_INET;
  sSockAddr->sin_port = sin.sin_port = htons(port);
  sSockAddr->sin_addr.s_addr = inet_addr(szAddr);

  iRet = setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, &iLoop, sizeof(int));
  if (iRet < 0) {
    fprintf(stderr,"setsockopt SO_REUSEADDR failed\n");
    exit(1);
  }

  iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_TTL, &cTtl, sizeof(char));
  if (iRet < 0) {
    fprintf(stderr,"setsockopt IP_MULTICAST_TTL failed.  multicast in kernel?\n");
    exit(1);
  }

  cLoop = 1;	/* !? */
  iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
                    &cLoop, sizeof(char));
  if (iRet < 0) {
    fprintf(stderr,"setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel?\n");
    exit(1);
  }

  return iSocket;
}

/* create a receiver socket, i.e. join the multicast group. */
int makeclientsocket(char *szAddr,unsigned short port,int TTL,struct sockaddr_in *sSockAddr) {
  int socket=makesocket(szAddr,port,TTL,sSockAddr);
  struct ip_mreq blub;
  struct sockaddr_in sin;
  unsigned int tempaddr;
  sin.sin_family=AF_INET;
  sin.sin_port=htons(port);
  sin.sin_addr.s_addr=inet_addr(szAddr);
  if (bind(socket,(struct sockaddr *) &sin,sizeof(sin))) {
    perror("bind failed");
    exit(1);
  }
  tempaddr=inet_addr(szAddr);
  if ((ntohl(tempaddr) >> 28) == 0xe) {
    blub.imr_multiaddr.s_addr = inet_addr(szAddr);
    blub.imr_interface.s_addr = 0;
    if (setsockopt(socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&blub,sizeof(blub))) {
      perror("setsockopt IP_ADD_MEMBERSHIP failed (multicast kernel?)");
      exit(1);
    }
  }
  return socket;
}

