#ifndef MPLAYER_MP_IMAGE_H
#define MPLAYER_MP_IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp_msg.h"

//--------- codec's requirements (filled by the codec/vf) ---------

//--- buffer content restrictions:
// set if buffer content shouldn't be modified:
#define MP_IMGFLAG_PRESERVE 0x01
// set if buffer content will be READ for next frame's MC: (I/P mpeg frames)
#define MP_IMGFLAG_READABLE 0x02

//--- buffer width/stride/plane restrictions: (used for direct rendering)
// stride _have_to_ be aligned to MB boundary:  [for DR restrictions]
#define MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE 0x4
// stride should be aligned to MB boundary:     [for buffer allocation]
#define MP_IMGFLAG_PREFER_ALIGNED_STRIDE 0x8
// codec accept any stride (>=width):
#define MP_IMGFLAG_ACCEPT_STRIDE 0x10
// codec accept any width (width*bpp=stride -> stride%bpp==0) (>=width):
#define MP_IMGFLAG_ACCEPT_WIDTH 0x20
//--- for planar formats only:
// uses only stride[0], and stride[1]=stride[2]=stride[0]>>mpi->chroma_x_shift
#define MP_IMGFLAG_COMMON_STRIDE 0x40
// uses only planes[0], and calculates planes[1,2] from width,height,imgfmt
#define MP_IMGFLAG_COMMON_PLANE 0x80

#define MP_IMGFLAGMASK_RESTRICTIONS 0xFF

//--------- color info (filled by mp_image_setfmt() ) -----------
// set if number of planes > 1
#define MP_IMGFLAG_PLANAR 0x100
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x200
// set if it's swapped (BGR or YVU) plane/byteorder
#define MP_IMGFLAG_SWAPPED 0x400
// set if you want memory for palette allocated and managed by vf_get_image etc.
#define MP_IMGFLAG_RGB_PALETTE 0x800

#define MP_IMGFLAGMASK_COLORS 0xF00

// codec uses drawing/rendering callbacks (draw_slice()-like thing, DR method 2)
// [the codec will set this flag if it supports callbacks, and the vo _may_
//  clear it in get_image() if draw_slice() not implemented]
#define MP_IMGFLAG_DRAW_CALLBACK 0x1000
// set if it's in video buffer/memory: [set by vo/vf's get_image() !!!]
#define MP_IMGFLAG_DIRECT 0x2000
// set if buffer is allocated (used in destination images):
#define MP_IMGFLAG_ALLOCATED 0x4000

// buffer type was printed (do NOT set this flag - it's for INTERNAL USE!!!)
#define MP_IMGFLAG_TYPE_DISPLAYED 0x8000

// codec doesn't support any form of direct rendering - it has own buffer
// allocation. so we just export its buffer pointers:
#define MP_IMGTYPE_EXPORT 0
// codec requires a static WO buffer, but it does only partial updates later:
#define MP_IMGTYPE_STATIC 1
// codec just needs some WO memory, where it writes/copies the whole frame to:
#define MP_IMGTYPE_TEMP 2
// I+P type, requires 2+ independent static R/W buffers
#define MP_IMGTYPE_IP 3
// I+P+B type, requires 2+ independent static R/W and 1+ temp WO buffers
#define MP_IMGTYPE_IPB 4
// Upper 16 bits give desired buffer number, -1 means get next available
#define MP_IMGTYPE_NUMBERED 5

#define MP_MAX_PLANES	4

#define MP_IMGFIELD_ORDERED 0x01
#define MP_IMGFIELD_TOP_FIRST 0x02
#define MP_IMGFIELD_REPEAT_FIRST 0x04
#define MP_IMGFIELD_TOP 0x08
#define MP_IMGFIELD_BOTTOM 0x10
#define MP_IMGFIELD_INTERLACED 0x20

typedef struct mp_image_s {
    unsigned int flags;
    unsigned char type;
    int number;
    unsigned char bpp;  // bits/pixel. NOT depth! for RGB it will be n*8
    unsigned int imgfmt;
    int width,height;  // stored dimensions
    int x,y,w,h;  // visible dimensions
    unsigned char* planes[MP_MAX_PLANES];
    int stride[MP_MAX_PLANES];
    char * qscale;
    int qstride;
    int pict_type; // 0->unknown, 1->I, 2->P, 3->B
    int fields;
    int qscale_type; // 0->mpeg1/4/h263, 1->mpeg2
    int num_planes;
    /* these are only used by planar formats Y,U(Cb),V(Cr) */
    int chroma_width;
    int chroma_height;
    int chroma_x_shift; // horizontal
    int chroma_y_shift; // vertical
    int usage_count;
    /* for private use by filter or vo driver (to store buffer id or dmpi) */
    void* priv;
} mp_image_t;

#ifdef IMGFMT_YUY2
static inline void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt){
    mpi->flags&=~(MP_IMGFLAG_PLANAR|MP_IMGFLAG_YUV|MP_IMGFLAG_SWAPPED);
    mpi->imgfmt=out_fmt;
    // compressed formats
    if(out_fmt == IMGFMT_MPEGPES ||
       out_fmt == IMGFMT_ZRMJPEGNI || out_fmt == IMGFMT_ZRMJPEGIT || out_fmt == IMGFMT_ZRMJPEGIB ||
       IMGFMT_IS_VDPAU(out_fmt) || IMGFMT_IS_XVMC(out_fmt)){
	mpi->bpp=0;
	return;
    }
    mpi->num_planes=1;
    if (IMGFMT_IS_RGB(out_fmt)) {
	if (IMGFMT_RGB_DEPTH(out_fmt) < 8 && !(out_fmt&128))
	    mpi->bpp = IMGFMT_RGB_DEPTH(out_fmt);
	else
	    mpi->bpp=(IMGFMT_RGB_DEPTH(out_fmt)+7)&(~7);
	return;
    }
    if (IMGFMT_IS_BGR(out_fmt)) {
	if (IMGFMT_BGR_DEPTH(out_fmt) < 8 && !(out_fmt&128))
	    mpi->bpp = IMGFMT_BGR_DEPTH(out_fmt);
	else
	    mpi->bpp=(IMGFMT_BGR_DEPTH(out_fmt)+7)&(~7);
	mpi->flags|=MP_IMGFLAG_SWAPPED;
	return;
    }
    mpi->flags|=MP_IMGFLAG_YUV;
    mpi->num_planes=3;
    if (mp_get_chroma_shift(out_fmt, NULL, NULL)) {
        mpi->flags|=MP_IMGFLAG_PLANAR;
        mpi->bpp = mp_get_chroma_shift(out_fmt, &mpi->chroma_x_shift, &mpi->chroma_y_shift);
        mpi->chroma_width  = mpi->width  >> mpi->chroma_x_shift;
        mpi->chroma_height = mpi->height >> mpi->chroma_y_shift;
    }
    switch(out_fmt){
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YV12:
	return;
    case IMGFMT_420A:
    case IMGFMT_IF09:
	mpi->num_planes=4;
    case IMGFMT_YVU9:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_411P:
    case IMGFMT_440P:
    case IMGFMT_444P16_LE:
    case IMGFMT_444P16_BE:
    case IMGFMT_422P16_LE:
    case IMGFMT_422P16_BE:
    case IMGFMT_420P16_LE:
    case IMGFMT_420P16_BE:
	return;
    case IMGFMT_Y800:
    case IMGFMT_Y8:
	/* they're planar ones, but for easier handling use them as packed */
//	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=8;
	mpi->num_planes=1;
	return;
    case IMGFMT_UYVY:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YUY2:
	mpi->bpp=16;
	mpi->num_planes=1;
	return;
    case IMGFMT_NV12:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_NV21:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=12;
	mpi->num_planes=2;
	mpi->chroma_width=(mpi->width>>0);
	mpi->chroma_height=(mpi->height>>1);
	mpi->chroma_x_shift=0;
	mpi->chroma_y_shift=1;
	return;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"mp_image: unknown out_fmt: 0x%X\n",out_fmt);
    mpi->bpp=0;
}
#endif

static inline mp_image_t* new_mp_image(int w,int h){
    mp_image_t* mpi=(mp_image_t*)malloc(sizeof(mp_image_t));
    if(!mpi) return NULL; // error!
    memset(mpi,0,sizeof(mp_image_t));
    mpi->width=mpi->w=w;
    mpi->height=mpi->h=h;
    return mpi;
}

static inline void free_mp_image(mp_image_t* mpi){
    if(!mpi) return;
    if(mpi->flags&MP_IMGFLAG_ALLOCATED){
	/* becouse we allocate the whole image in once */
	if(mpi->planes[0]) free(mpi->planes[0]);
	if (mpi->flags & MP_IMGFLAG_RGB_PALETTE)
	    free(mpi->planes[1]);
    }
    free(mpi);
}

mp_image_t* alloc_mpi(int w, int h, unsigned long int fmt);
void mp_image_alloc_planes(mp_image_t *mpi);
void copy_mpi(mp_image_t *dmpi, mp_image_t *mpi);

#endif /* MPLAYER_MP_IMAGE_H */
