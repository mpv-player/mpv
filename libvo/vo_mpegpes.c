
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN (mpegpes)

int vo_mpegpes_fd=-1;

static vo_info_t vo_info = 
{
	"Mpeg-PES file",
	"mpgpes",
	"A'rpi",
	""
};

static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    vo_mpegpes_fd=open("grab.mpg","ab");
    if(vo_mpegpes_fd<0){	
	perror("vo_mpegpes");
	return -1;
    }
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

static void my_write(unsigned char* data,int len){
    while(len>0){
	int ret=write(vo_mpegpes_fd,data,len);
	if(ret<=0) break; // error
	len-=ret; data+=ret;
    }
}

static uint32_t draw_frame(uint8_t * src[])
{
    vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
    int payload_size=p->size+5;
    int x;
    unsigned char pes_header[4+2+5];

    // construct PES header:
	      // startcode:
	      pes_header[0]=pes_header[1]=0;
	      pes_header[2]=p->id>>8; pes_header[3]=p->id&255;
	      // packetsize:
	      pes_header[4]=payload_size>>8;
	      pes_header[5]=payload_size&255;
	      // stuffing:
	      // presentation time stamp:
	      x=(0x02 << 4) | (((p->timestamp >> 30) & 0x07) << 1) | 1;
	      pes_header[6]=x;
	      x=((((p->timestamp >> 15) & 0x7fff) << 1) | 1);
	      pes_header[7]=x>>8; pes_header[8]=x&255;
	      x=((((p->timestamp) & 0x7fff) << 1) | 1);
	      pes_header[9]=x>>8; pes_header[10]=x&255;
    my_write(pes_header,4+2+5);
    // data:
    my_write(p->data,p->size);

//    printf("PES: draw frame!  pts=%d   size=%d  \n",p->timestamp,p->size);

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
    close(vo_mpegpes_fd);
    vo_mpegpes_fd=-1;
}


static void check_events(void)
{
}

