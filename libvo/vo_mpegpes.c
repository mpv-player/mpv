/*
 * based on: test_av.c - test program for new API
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * MPEG-PS multiplexer, part of FFmpeg
 * Copyright Gerard Lantau (see http://ffmpeg.org)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#ifdef CONFIG_DVB
#define true 1
#define false 0
#include <poll.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <time.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#endif

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "libmpdemux/mpeg_packetizer.h"

int vo_mpegpes_fd=-1;
extern int ao_mpegpes_fd;

static const vo_info_t info =
{
#ifdef CONFIG_DVB
	"MPEG-PES to DVB card",
#else
	"MPEG-PES file",
#endif
	"mpegpes",
	"A'rpi",
	""
};

const LIBVO_EXTERN (mpegpes)

static int
config(uint32_t s_width, uint32_t s_height, uint32_t width, uint32_t height, uint32_t flags, char *title, uint32_t format)
{
#ifdef CONFIG_DVB
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

static int preinit(const char *arg){
#ifdef CONFIG_DVB
    int card = -1;
    char vo_file[30], ao_file[30], *tmp;

    if(arg != NULL){
	if((tmp = strstr(arg, "card=")) != NULL) {
	    card = atoi(&tmp[5]);
	    if((card < 1) || (card > 4)) {
		mp_msg(MSGT_VO, MSGL_ERR, "DVB card number must be between 1 and 4\n");
		return -1;
	    }
	    card--;
	    arg = NULL;
	}
    }

    if(!arg){
    //|O_NONBLOCK
        //search the first usable card
        if(card==-1) {
          int n;
          for(n=0; n<4; n++) {
            sprintf(vo_file, "/dev/dvb/adapter%d/video0", n);
            if(access(vo_file, F_OK | W_OK)==0) {
              card = n;
              break;
            }
          }
        }
        if(card==-1) {
          mp_msg(MSGT_VO,MSGL_INFO, "Couldn't find a usable dvb video device, exiting\n");
          return -1;
        }
	mp_msg(MSGT_VO,MSGL_INFO, "Opening /dev/dvb/adapter%d/video0+audio0\n", card);
	sprintf(vo_file, "/dev/dvb/adapter%d/video0", card);
	sprintf(ao_file, "/dev/dvb/adapter%d/audio0", card);
	if((vo_mpegpes_fd = open(vo_file,O_RDWR)) < 0){
        	perror("DVB VIDEO DEVICE: ");
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
	if ( (ioctl(vo_mpegpes_fd,VIDEO_PLAY) < 0)){
		perror("DVB VIDEO PLAY: ");
		return -1;
	}
	return 0;
    }
#endif
    arg = (arg ? arg : "grab.mpg");
    mp_msg(MSGT_VO,MSGL_INFO, "Saving PES stream to %s\n", arg);
    vo_mpegpes_fd=open(arg,O_WRONLY|O_CREAT,0666);
    if(vo_mpegpes_fd<0){
	perror("vo_mpegpes");
	return -1;
    }
    return 0;
}


static void draw_osd(void)
{
}


static int my_write(const unsigned char* data,int len){
    int orig_len = len;
#ifdef CONFIG_DVB
#define NFD   2
    struct pollfd pfd[NFD];

//    printf("write %d bytes  \n",len);

	pfd[0].fd = vo_mpegpes_fd;
	pfd[0].events = POLLOUT;

	pfd[1].fd = ao_mpegpes_fd;
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
    return orig_len;
}

static void send_pes_packet(unsigned char* data, int len, int id, int timestamp)
{
    send_mpeg_pes_packet (data, len, id, timestamp, 1, my_write);
}

static int draw_frame(uint8_t * src[])
{
    vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
    send_pes_packet(p->data,p->size,p->id,(p->timestamp>0)?p->timestamp:vo_pts);  // video data
    return 0;
}

static void flip_page (void)
{
}

static int draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x0,int y0)
{
    return 0;
}


static int
query_format(uint32_t format)
{
    if(format==IMGFMT_MPEGPES) return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_TIMER;
    return 0;
}

static void
uninit(void)
{
    if(ao_mpegpes_fd >= 0 && ao_mpegpes_fd != vo_mpegpes_fd) close(ao_mpegpes_fd);
    ao_mpegpes_fd = -1;
    if(vo_mpegpes_fd>=0){ close(vo_mpegpes_fd);vo_mpegpes_fd=-1;}
}


static void check_events(void)
{
}

static int control(uint32_t request, void *data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
