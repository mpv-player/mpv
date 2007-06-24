#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

#define TEMP_BUF_SIZE (720*576)

static vd_info_t info = {
	"Hauppauge Macroblock/NV12/NV21 Decoder",
	"hmblck",
	"Alex <d18c7db@hotmail.com>, A'rpi, Alex Beregszaszi",
	"Alex <d18c7db@hotmail.com>",
	"uncompressed"
};

LIBVD_EXTERN(hmblck)

static void de_macro_y(unsigned char* dst,unsigned char* src,int dstride,int w,int h){
    unsigned int y;
    // descramble Y plane
    for (y=0; y<h; y+=16) {
	unsigned int x;
        for (x=0; x<w; x+=16) {
	    unsigned int i;
            for (i=0; i<16; i++) {
                memcpy(dst + x + (y+i)*dstride, src, 16);
                src+=16;
            }
        }
    }
}

static void de_macro_uv(unsigned char* dstu,unsigned char* dstv,unsigned char* src,int dstride,int w,int h){
    unsigned int y;
    // descramble U/V plane
    for (y=0; y<h; y+=16) {
	unsigned int x;
        for (x=0; x<w; x+=8) {
	    unsigned int i;
            for (i=0; i<16; i++) {
		int idx=x + (y+i)*dstride;
		dstu[idx+0]=src[0]; dstv[idx+0]=src[1];
		dstu[idx+1]=src[2]; dstv[idx+1]=src[3];
		dstu[idx+2]=src[4]; dstv[idx+2]=src[5];
		dstu[idx+3]=src[6]; dstv[idx+3]=src[7];
		dstu[idx+4]=src[8]; dstv[idx+4]=src[9];
		dstu[idx+5]=src[10]; dstv[idx+5]=src[11];
		dstu[idx+6]=src[12]; dstv[idx+6]=src[13];
		dstu[idx+7]=src[14]; dstv[idx+7]=src[15];
                src+=16;
            }
        }
    }
}

/*************************************************************************
 * convert a nv12 buffer to yv12
 */
static int nv12_to_yv12(unsigned char *data, int len, mp_image_t* mpi, int swapped) {
    unsigned int Y_size  = mpi->width * mpi->height;
    unsigned int UV_size = mpi->chroma_width * mpi->chroma_height;
    unsigned int idx;
    unsigned char *dst_Y = mpi->planes[0];
    unsigned char *dst_U = mpi->planes[1];
    unsigned char *dst_V = mpi->planes[2];
    unsigned char *src   = data + Y_size;

    // sanity check raw stream
    if ( (len != (Y_size + (UV_size<<1))) ) {
        mp_msg(MSGT_DECVIDEO, MSGL_ERR,
               "hmblck: Image size inconsistent with data size.\n");
        return 0;
    }
    if ( (mpi->width > 720) || (mpi->height > 576) ) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,
               "hmblck: Image size is too big.\n");
        return 0;
    }
    if (mpi->num_planes != 3) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,
               "hmblck: Incorrect number of image planes.\n");
        return 0;
    }

    // luma data is easy, just copy it
    memcpy(dst_Y, data, Y_size);

    // chroma data is interlaced UVUV... so deinterlace it
    for(idx=0; idx<UV_size; idx++ ) {
        *(dst_U + idx) = *(src + (idx<<1) + (swapped ? 1 : 0)); 
        *(dst_V + idx) = *(src + (idx<<1) + (swapped ? 0 : 1));
    }
    return 1;
}

/*************************************************************************
 * set/get/query special features/parameters
 */
static int control(sh_video_t *sh,int cmd, void *arg,...){
    return CONTROL_UNKNOWN;
}
/*************************************************************************
 * init driver
 */
static int init(sh_video_t *sh){
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,sh->format);
}
/*************************************************************************
 * uninit driver
 */
static void uninit(sh_video_t *sh){
}
/*************************************************************************
 * decode a frame
 */
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;

    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
        sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    if(sh->format == IMGFMT_HM12) {
        //if(!de_macro(sh, data, len, flags, mpi)) return NULL;
	de_macro_y(mpi->planes[0],data,mpi->stride[0],mpi->w,mpi->h);
	de_macro_uv(mpi->planes[1],mpi->planes[2],
                    (unsigned char *)data+mpi->w*mpi->h,mpi->stride[1],
                    mpi->w/2,mpi->h/2);
    } else {
	if(!nv12_to_yv12(data, len, mpi,(sh->format == IMGFMT_NV21))) return NULL;
    }

    return mpi;
}
/*************************************************************************/
