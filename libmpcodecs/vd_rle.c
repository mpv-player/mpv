#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"RLE Video decoder",
	"msrle",
	VFM_MSRLE,
	"A'rpi",
	"XAnim rip...",
	"native codec"
};

LIBVD_EXTERN(msrle)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24)) return 0;
    if( (((sh->codec->outfmt[sh->outfmtidx]&255)+7)/8)==2 ){
     unsigned int* pal=(unsigned int*)(((char*)sh->bih)+40);
     int cols=(sh->bih->biSize-40)/4;
     //int cols=1<<(sh_video->bih->biBitCount);
     int i;
     if(cols>256) cols=256;
     mp_msg(MSGT_DECVIDEO,MSGL_V,"RLE: converting palette for %d colors.\n",cols);
     for(i=0;i<cols;i++){
        unsigned int c=pal[i];
	unsigned int b=c&255;
	unsigned int g=(c>>8)&255;
	unsigned int r=(c>>16)&255;
	if((sh->codec->outfmt[sh->outfmtidx]&255)==15)
	  pal[i]=((r>>3)<<10)|((g>>3)<<5)|((b>>3));
	else
	  pal[i]=((r>>3)<<11)|((g>>2)<<5)|((b>>3));
     }
    }
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void AVI_Decode_RLE8(char *image,char *delta,int tdsize,
    unsigned int *map,int imagex,int imagey,unsigned char x11_bytes_pixel);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    AVI_Decode_RLE8(mpi->planes[0],data,len,
       (int*)(((char*)sh->bih)+40),
      sh->disp_w,sh->disp_h,((mpi->imgfmt&255)+7)/8);
    
    return mpi;
}

