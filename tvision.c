// testing only, not finished!!!!!!!

// little TeleVision program by A'rpi/ESP-team
// based on streamer-old.c video capture util (part of xawtv) by
//     (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>


#include <asm/types.h>		/* XXX glibc */
#include "videodev.h"

#include "libvo/video_out.h"

#define DEVNAME "/dev/video"

static struct video_mmap        gb1,gb2;
static struct video_capability  capability;
static struct video_channel     channel;
static struct video_mbuf        gb_buffers = { 2*0x151000, 0, {0,0x151000 }};
static unsigned char            *map = NULL;


int main(int argc,char* argv[]){
vo_functions_t *video_out=NULL;
char* video_driver=NULL; //"mga"; // default
int i;
int fd=-1;
char* frame=NULL;
int count=0;

  if(argc>1) video_driver=argv[1];

// check video_out driver name:
  if(!video_driver)
    video_out=video_out_drivers[0];
  else
  for (i=0; video_out_drivers[i] != NULL; i++){
    const vo_info_t *info = video_out_drivers[i]->get_info ();
    if(strcmp(info->short_name,video_driver) == 0){
      video_out = video_out_drivers[i];break;
    }
  }
  if(!video_out){
    printf("Invalid video output driver name: %s\n",video_driver);
    return 0;
  }


    /* open */
    if (-1 == fd && -1 == (fd = open(DEVNAME,O_RDWR))) {
	fprintf(stderr,"open %s: %s\n",DEVNAME,strerror(errno));
	exit(1);
    }

    /* get settings */
    if (-1 == ioctl(fd,VIDIOCGCAP,&capability)) {
	perror("ioctl VIDIOCGCAP");
	exit(1);
    }
    if (-1 == ioctl(fd,VIDIOCGCHAN,&channel))
	perror("ioctl VIDIOCGCHAN");

    /* mmap() buffer */
    if (-1 == ioctl(fd,VIDIOCGMBUF,&gb_buffers)) {
	perror("ioctl VIDIOCGMBUF");
    }
    map = mmap(0,gb_buffers.size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if ((unsigned char*)-1 == map) {
	perror("mmap");
    } else {
	    fprintf(stderr,"v4l: mmap()'ed buffer size = 0x%x\n",
		    gb_buffers.size);
    }

    /* prepare for grabbing */
    gb1.format = VIDEO_PALETTE_YUV422;
//    gb1.format = VIDEO_PALETTE_RGB24;
    gb1.frame  = 0;
    gb1.width  = 352;//720;//640;//320;
    gb1.height = 288;//576;//480;//240;

    gb2.format = gb1.format;
    gb2.frame  = 1;
    gb2.width  = gb1.width;
    gb2.height = gb1.height;

    video_out->init(gb1.width,gb1.height,1024,768,0,0,IMGFMT_YUY2);
//    video_out->init(gb1.width,gb1.height,1024,768,0,0,IMGFMT_RGB|24);

    if (-1 == ioctl(fd,VIDIOCMCAPTURE,&gb1)) {
	if (errno == EAGAIN)
	    fprintf(stderr,"grabber chip can't sync (no station tuned in?)\n");
	else
	    perror("ioctl VIDIOCMCAPTURE");
	exit(1);
    }
    count++;
    while(1){
    // MAIN LOOP
	if (-1 == ioctl(fd,VIDIOCMCAPTURE,(count%2) ? &gb2 : &gb1)) {
	    if (errno == EAGAIN)
		fprintf(stderr,"grabber chip can't sync (no station tuned in?)\n");
	    else
		perror("ioctl VIDIOCMCAPTURE");
	    exit(1);
	}

	if (-1 == ioctl(fd,VIDIOCSYNC,(count%2) ? &gb1.frame : &gb2.frame)) {
	    perror("ioctl VIDIOCSYNC");
	    exit(1);
	}
        frame=map + gb_buffers.offsets[(count%2) ? 0 : 1];
        video_out->draw_frame((unsigned char**)&frame);
        video_out->flip_page();
        
        count++;
    }
    
#if 0
    { FILE *f=fopen("frame.yuv","wb");
      fwrite(map,320*240*2,1,f);
      fclose(f);
    }
    video_out->init(320,240,800,600,0,0,IMGFMT_YUY2);
    video_out->draw_frame(count?map1:map2);
    video_out->flip_page();
    
    getchar();
#endif



}

