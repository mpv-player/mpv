/* 
 *  OpenDivX AVI file writer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(odivx)

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "../encore/encore.h"

#include "fastmemcpy.h"

static vo_info_t vo_info = 
{
	"OpenDivX AVI File writer",
	"odivx",
	"Arpad Gereoffy <arpi@esp-team.scene.hu>",
	""
};

static uint8_t *image=NULL;
static int image_width=0;
static int image_height=0;
static unsigned int image_format=0;
static char *buffer=NULL;
static int frameno=0;

extern char* encode_name;
extern char* encode_index_name;

//static uint32_t draw_slice(uint8_t *src[], uint32_t slice_num)
static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
    uint8_t *s;
    uint8_t *d;
    int i;
    int dstride=image_width;

    // copy Y
    d=image+dstride*y+x;
    s=src[0];
    for(i=0;i<h;i++){
        memcpy(d,s,w);
        s+=stride[0];
        d+=dstride;
    }
    
    w/=2;h/=2;x/=2;y/=2; dstride/=2;
    
    // copy U
    d=image+image_width*image_height + dstride*y+x;
    s=src[1];
    for(i=0;i<h;i++){
        memcpy(d,s,w);
        s+=stride[1];
        d+=dstride;
    }

    // copy V
    d=image+image_width*image_height +image_width*image_height/4 + dstride*y+x;
    s=src[2];
    for(i=0;i<h;i++){
        memcpy(d,s,w);
        s+=stride[2];
        d+=dstride;
    }
    
    return 0;
}

static uint32_t
draw_frame(uint8_t *src[])
{
  uint8_t *d=image;

  switch(image_format){
  case IMGFMT_YV12:
    // copy Y
    memcpy(d,src[0],image_width*image_height);
    // copy U
    d+=image_width*image_height;
    memcpy(d,src[1],image_width*image_height/4);
    // copy V
    d+=image_width*image_height/4;
    memcpy(d,src[2],image_width*image_height/4);
    break;
  case IMGFMT_YUY2: {
    uint8_t *dY=image;
    uint8_t *dU=image+image_width*image_height;
    uint8_t *dV=dU+image_width*image_height/4;
    uint8_t *s=src[0];
    int y;
    for(y=0;y<image_height;y+=2){
      uint8_t *e=s+image_width*2;
      while(s<e){
	*dY++=s[0];
	*dU++=s[1];
	*dY++=s[2];
	*dV++=s[3];
	s+=4;
      }
      e=s+image_width*2;
      while(s<e){
	*dY++=s[0];
	*dY++=s[2];
	s+=4;
      }
    }
    
//  case IMGFMT_BGR|24:
//    memcpy(d,src[0],image_width*image_height*2);
    break;
  }
  }

  return 0;
}

typedef unsigned int DWORD;

typedef struct
{
    DWORD               ckid;
    DWORD               dwFlags;
    DWORD               dwChunkOffset;          // Position of chunk
    DWORD               dwChunkLength;          // Length of chunk
} AVIINDEXENTRY;

static void draw_osd(void)
{
}

static void
flip_page(void)
{

// we are rady to encode this frame
ENC_FRAME enc_frame;
ENC_RESULT enc_result;

if(++frameno<10) return;

enc_frame.image=image;
enc_frame.bitstream=buffer;
enc_frame.length=0;
encore(0x123,0,&enc_frame,&enc_result);

printf("coded length: %ld  \n",enc_frame.length);

if(encode_name){
  AVIINDEXENTRY i;
  FILE *file;
  i.ckid=('c'<<24)|('d'<<16)|('0'<<8)|'0'; // "00dc"
  i.dwFlags=enc_result.isKeyFrame?0x10:0;
  i.dwChunkLength=enc_frame.length;
  // Write AVI chunk:
  if((file=fopen(encode_name,"ab"))){
    unsigned char zerobyte=0;
    i.dwChunkOffset=ftell(file);
    fwrite(&i.ckid,4,1,file);
    fwrite(&enc_frame.length,4,1,file);
    fwrite(buffer,enc_frame.length,1,file);
    if(enc_frame.length&1) fwrite(&zerobyte,1,1,file); // padding
    fclose(file);
  }
  // Write AVI index:
  if(encode_index_name && (file=fopen(encode_index_name,"ab"))){
    fwrite(&i,sizeof(i),1,file);
    fclose(file);
  }
}


}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_YUY2:
//    case IMGFMT_BGR|24:
        return 1;
    }
    return 0;
}

extern int encode_bitrate;

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
	uint32_t frame_size;
        ENC_PARAM enc_param;

//	file = fopen("encoded.odvx","wb");
//        if(!file) return -1;
        
    switch(format){
    case IMGFMT_YV12:
        frame_size=width*height+width*height/2;
//        enc_param.flip=2; // 0=RGB  1=flippedRGB  2=planarYUV format
        break;
    case IMGFMT_YUY2:
//    case IMGFMT_BGR|24:
//        enc_param.flip=0; // 0=RGB  1=flippedRGB  2=planarYUV format
//        frame_size=width*height*2;
        frame_size=width*height+width*height/2;
        break;
    default: return -1; // invalid format
    }

    enc_param.x_dim=width;
    enc_param.y_dim=height;

    image_width=width;
    image_height=height;
    image_format=format;
    image=malloc(frame_size);

	//clear the buffer
	memset(image,0x80,frame_size);

    // buffer for encoded video data:
    buffer=malloc(0x100000);
    if(!buffer) return -1;

    // encoding parameters:
    enc_param.framerate=25.0;
    enc_param.bitrate=encode_bitrate?encode_bitrate:780000;
    enc_param.rc_period=300;
    enc_param.max_quantizer=15;
    enc_param.min_quantizer=1;
    enc_param.search_range=128;

    // init codec:
    encore(0x123,ENC_OPT_INIT,&enc_param,NULL);

  return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

static void
uninit(void)
{
}

static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_odivx: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
