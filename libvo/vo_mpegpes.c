//#define HAVE_DVB
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>

#ifdef HAVE_DVB

#include <sys/ioctl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

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
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
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
    vo_mpegpes_fd=open("grab.mpg",O_WRONLY|O_CREAT);
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

static void flip_page (void)
{
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
    return 0;
}

#define NFD   2

static void my_write(unsigned char* data,int len){
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
}

static unsigned char pes_header[PES_MAX_SIZE];

static void send_pes_packet(unsigned char* data,int len,int id,int timestamp){
    int x;

	      pes_header[0]=pes_header[1]=0;
	      pes_header[2]=id>>8; pes_header[3]=id&255;
    
    while(1){
	    int payload_size=len+5;  // data + PTS
	    if(6+payload_size>PES_MAX_SIZE) payload_size=PES_MAX_SIZE-6;
	    
    // construct PES header:  (code from ffmpeg's libav)
	      // startcode:
	      // packetsize:
	      pes_header[4]=(payload_size)>>8;
	      pes_header[5]=(payload_size)&255;
	      // stuffing:
	      // presentation time stamp:
	      x=(0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
	      pes_header[6]=x;
	      x=((((timestamp >> 15) & 0x7fff) << 1) | 1);
	      pes_header[7]=x>>8; pes_header[8]=x&255;
	      x=((((timestamp) & 0x7fff) << 1) | 1);
	      pes_header[9]=x>>8; pes_header[10]=x&255;
	      
	payload_size-=5;
	memcpy(&pes_header[6+5],data,payload_size);
	my_write(pes_header,6+5+payload_size);

	len-=payload_size; data+=payload_size;
	if(len<=0) break;
    }

//    printf("PES: draw frame!  pts=%d   size=%d  \n",timestamp,len);

}

static uint32_t draw_frame(uint8_t * src[])
{
    vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
    unsigned char *data=p->data;
//    int tmp=-1;
    send_pes_packet(p->data,p->size,p->id,p->timestamp);  // video data
//    send_pes_packet(&tmp,0,0x1C0,p->timestamp+30000); // fake audio data

    return 0;
}

static uint32_t
query_format(uint32_t format)
{
    if(format==IMGFMT_MPEGPES) return 1;
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

