// Don't change for DVB card, it must be 2048
#define PES_MAX_SIZE 2048

/* 
 * Based on:
 *
 * test_av.c - Test program for new API
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * libav - MPEG-PS multiplexer, part of ffmpeg
 * Copyright Gerard Lantau  (see http://ffmpeg.sf.net)
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "mp_msg.h"

#ifdef HAVE_DVB

#include <sys/poll.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <time.h>

#include <ost/dmx.h>
#include <ost/frontend.h>
#include <ost/sec.h>
#include <ost/video.h>
#include <ost/audio.h>

#endif

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN (mpegpes)

int vo_mpegpes_fd=-1;
int vo_mpegpes_fd2=-1;

static vo_info_t vo_info = 
{
#ifdef HAVE_DVB
	"Mpeg-PES to DVB card",
#else
	"Mpeg-PES file",
#endif
	"mpegpes",
	"A'rpi",
	""
};

static uint32_t
config(uint32_t s_width, uint32_t s_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
#ifdef HAVE_DVB
    switch(s_height){
    case 288:
    case 576:
    case 240:
    case 480:
	break;
    default:
	mp_msg(MSGT_VO,MSGL_ERR,"DVB: height=%d not supported (try 240/480 (ntsc) or 288/576 (pal)\n",s_height);
	return -1;
    }
#endif
    return 0;
}

static uint32_t preinit(const char *arg){
#ifdef HAVE_DVB
    //|O_NONBLOCK
	if((vo_mpegpes_fd = open("/dev/ost/video",O_RDWR)) < 0){
		perror("DVB VIDEO DEVICE: ");
		return -1;
	}
	if((vo_mpegpes_fd2 = open("/dev/ost/audio",O_RDWR|O_NONBLOCK)) < 0){
		perror("DVB AUDIO DEVICE: ");
		return -1;
	}
	if ( (ioctl(vo_mpegpes_fd,VIDEO_SET_BLANK, false) < 0)){
		perror("DVB VIDEO SET BLANK: ");
		return -1;
	}
	if ( (ioctl(vo_mpegpes_fd,VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY) < 0)){
		perror("DVB VIDEO SELECT SOURCE: ");
		return -1;
	}
#if 1
	if ( (ioctl(vo_mpegpes_fd2,AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY) < 0)){
		perror("DVB AUDIO SELECT SOURCE: ");
		return -1;
	}
	if ( (ioctl(vo_mpegpes_fd2,AUDIO_PLAY) < 0)){
		perror("DVB AUDIO PLAY: ");
		return -1;
	}
#else
	if ( (ioctl(vo_mpegpes_fd2,AUDIO_STOP,0) < 0)){
		perror("DVB AUDIO STOP: ");
		return -1;
	}
#endif
	if ( (ioctl(vo_mpegpes_fd,VIDEO_PLAY) < 0)){
		perror("DVB VIDEO PLAY: ");
		return -1;
	}
	if ( (ioctl(vo_mpegpes_fd2,AUDIO_SET_AV_SYNC, false) < 0)){
		perror("DVB AUDIO SET AV SYNC: ");
		return -1;
	}
	if ( (ioctl(vo_mpegpes_fd2,AUDIO_SET_MUTE, false) < 0)){
		perror("DVB AUDIO SET MUTE: ");
		return -1;
	}

#else
    vo_mpegpes_fd=open("grab.mpg",O_WRONLY|O_CREAT,0666);
    if(vo_mpegpes_fd<0){	
	perror("vo_mpegpes");
	return -1;
    }
#endif
    return 0;
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
}

static void draw_osd(void)
{
}


static void my_write(unsigned char* data,int len){
#ifdef HAVE_DVB
#define NFD   2
    struct pollfd pfd[NFD];

//    printf("write %d bytes  \n",len);

	pfd[0].fd = vo_mpegpes_fd;
	pfd[0].events = POLLOUT;
	
	pfd[1].fd = vo_mpegpes_fd2;
	pfd[1].events = POLLOUT;

    while(len>0){
	if (poll(pfd,NFD,1)){
	    if (pfd[0].revents & POLLOUT){
		int ret=write(vo_mpegpes_fd,data,len);
//		printf("ret=%d  \n",ret); 
		if(ret<=0){
		    perror("write");
		    usleep(0);
		} else {
		    len-=ret; data+=ret;
		}
	    } else usleep(1000);
	}
    }

#else
    write(vo_mpegpes_fd,data,len); // write to file
#endif
}

static unsigned char pes_header[PES_MAX_SIZE];

void send_pes_packet(unsigned char* data,int len,int id,int timestamp){
    int ptslen=timestamp?5:1;

	      // startcode:
	      pes_header[0]=pes_header[1]=0;
	      pes_header[2]=id>>8; pes_header[3]=id&255;
    
    while(len>0){
	    int payload_size=len;  // data + PTS
	    if(6+ptslen+payload_size>PES_MAX_SIZE) payload_size=PES_MAX_SIZE-(6+ptslen);
	    
    // construct PES header:  (code from ffmpeg's libav)
	      // packetsize:
	      pes_header[4]=(ptslen+payload_size)>>8;
	      pes_header[5]=(ptslen+payload_size)&255;

	if(ptslen==5){
	      int x;
	      // presentation time stamp:
	      x=(0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
	      pes_header[6]=x;
	      x=((((timestamp >> 15) & 0x7fff) << 1) | 1);
	      pes_header[7]=x>>8; pes_header[8]=x&255;
	      x=((((timestamp) & 0x7fff) << 1) | 1);
	      pes_header[9]=x>>8; pes_header[10]=x&255;
	} else {
	      // stuffing and header bits:
	      pes_header[6]=0x0f;
	}

	memcpy(&pes_header[6+ptslen],data,payload_size);
	my_write(pes_header,6+ptslen+payload_size);

	len-=payload_size; data+=payload_size;
	ptslen=1; // store PTS only once, at first packet!
    }

//    printf("PES: draw frame!  pts=%d   size=%d  \n",timestamp,len);

}

void send_lpcm_packet(unsigned char* data,int len,int id,unsigned int timestamp,int freq_id){

    int ptslen=timestamp?5:0;

	      // startcode:
	      pes_header[0]=pes_header[1]=0;
	      pes_header[2]=1; pes_header[3]=0xBD;
    
    while(len>=4){
	    int payload_size;
	    
	    payload_size=PES_MAX_SIZE-6-20; // max possible data len
	    if(payload_size>len) payload_size=len;
	    payload_size&=(~3); // align!

	    //if(6+payload_size>PES_MAX_SIZE) payload_size=PES_MAX_SIZE-6;
	    
	      // packetsize:
	      pes_header[4]=(payload_size+3+ptslen+7)>>8;
	      pes_header[5]=(payload_size+3+ptslen+7)&255;

	      // stuffing:
	      pes_header[6]=0x81;
	      pes_header[7]=0x80;

	      // hdrlen:
	      pes_header[8]=ptslen;
	      
	  if(ptslen){
	      int x;
	      // presentation time stamp:
	      x=(0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
	      pes_header[9]=x;
	      x=((((timestamp >> 15) & 0x7fff) << 1) | 1);
	      pes_header[10]=x>>8; pes_header[11]=x&255;
	      x=((((timestamp) & 0x7fff) << 1) | 1);
	      pes_header[12]=x>>8; pes_header[13]=x&255;
	  }
	      
// ============ LPCM header: (7 bytes) =================
// Info by mocm@convergence.de

//	   ID:
	      pes_header[ptslen+9]=id;

//	   number of frames:
	      pes_header[ptslen+10]=0x07;

//	   first acces unit pointer, i.e. start of audio frame:
	      pes_header[ptslen+11]=0x00;
	      pes_header[ptslen+12]=0x04;

//	   audio emphasis on-off                                  1 bit
//	   audio mute on-off                                      1 bit
//	   reserved                                               1 bit
//	   audio frame number                                     5 bit
	      pes_header[ptslen+13]=0x0C;

//	   quantization word length                               2 bit
//	   audio sampling frequency (48khz = 0, 96khz = 1)        2 bit
//	   reserved                                               1 bit
//	   number of audio channels - 1 (e.g. stereo = 1)         3 bit
	      pes_header[ptslen+14]=1|(freq_id<<4);

//	   dynamic range control (0x80 if off)
	      pes_header[ptslen+15]=0x80;

	memcpy(&pes_header[6+3+ptslen+7],data,payload_size);
	my_write(pes_header,6+3+ptslen+7+payload_size);

	len-=payload_size; data+=payload_size;
	ptslen=0; // store PTS only once, at first packet!
    }

//    printf("PES: draw frame!  pts=%d   size=%d  \n",timestamp,len);

}


static uint32_t draw_frame(uint8_t * src[])
{
    vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
    send_pes_packet(p->data,p->size,p->id,(p->timestamp>0)?p->timestamp:vo_pts);  // video data
    return 0;
}

static void flip_page (void)
{
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x0,int y0)
{
    return 0;
}


static uint32_t
query_format(uint32_t format)
{
    if(format==IMGFMT_MPEGPES) return 3|VFCAP_TIMER;
    return 0;
}

static void
uninit(void)
{
    if(vo_mpegpes_fd>=0){ close(vo_mpegpes_fd);vo_mpegpes_fd=-1;}
    if(vo_mpegpes_fd2>=0){ close(vo_mpegpes_fd2);vo_mpegpes_fd2=-1;}
}


static void check_events(void)
{
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
