#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Cinepak Video decoder",
	"cinepak",
	"A'rpi",
	"Dr. Tim Ferguson, http://www.csse.monash.edu.au/~timf/videocodec.html",
	"native codec"
};

LIBVD_EXTERN(cinepak)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

void *decode_cinepak_init(void);

// init driver
static int init(sh_video_t *sh){
    sh->context = decode_cinepak_init();
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

//void decode_cinepak(void *context, unsigned char *buf, int size, unsigned char *frame, int width, int height, int bit_per_pixel, int stride_);
void decode_cinepak(void *context, unsigned char *buf, int size, mp_image_t* mpi);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_ACCEPT_STRIDE, 
	(sh->disp_w+3)&(~3),
	(sh->disp_h+3)&(~3));
    
    if(!mpi){	// temporary!
	printf("couldn't allocate image for cinepak codec\n");
	return NULL;
    }

#if 0    
    printf("mpi: %p/%d %p/%d %p/%d (%d) (%d)  \n",
	mpi->planes[0], mpi->stride[0],
	mpi->planes[1], mpi->stride[1],
	mpi->planes[2], mpi->stride[2],
	mpi->planes[1]-mpi->planes[0],
	mpi->planes[2]-mpi->planes[1]);
#endif

//    decode_cinepak(sh->context, data, len, mpi->planes[0], sh->disp_w, sh->disp_h,
//	(mpi->flags&MP_IMGFLAG_YUV)?16:(mpi->imgfmt&255), mpi->stride[0]);

    decode_cinepak(sh->context, data, len, mpi);
    
    return mpi;
}

