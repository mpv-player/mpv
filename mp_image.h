
// set if it's internal buffer of the codec, and shouldn't be modified:
#define MP_IMGFLAG_READONLY 0x01
// set if buffer is allocated (used in destination images):
#define MP_IMGFLAG_ALLOCATED 0x02
// set if it's in video buffer/memory:
#define MP_IMGFLAG_DIRECT 0x04

// set if number of planes > 1
#define MP_IMGFLAG_PLANAR 0x10
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x20
// set if it's swapped plane/byteorder
#define MP_IMGFLAG_SWAPPED 0x40

#define MP_IMGTYPE_EXPORT 0
#define MP_IMGTYPE_STATIC 1
#define MP_IMGTYPE_TEMP 2

typedef struct mp_image_s {
    unsigned short flags;
    unsigned char type;
    unsigned char bpp;  // bits/pixel. NOT depth! for RGB it will be n*8
    unsigned int imgfmt;
    int width,height;  // stored dimensions
    int x,y,w,h;  // visible dimensions
    unsigned char* planes[3];
    unsigned int stride[3];
    int* qscale;
    int qstride;
} mp_image_t;

#ifdef IMGFMT_YUY2
static inline void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt){
    mpi->flags&=~(MP_IMGFLAG_PLANAR|MP_IMGFLAG_YUV|MP_IMGFLAG_SWAPPED);
    mpi->imgfmt=out_fmt;
    if( (out_fmt&IMGFMT_RGB_MASK) == IMGFMT_RGB ){
	mpi->bpp=((out_fmt&255)+7)&(~7);
	return;
    }
    if( (out_fmt&IMGFMT_BGR_MASK) == IMGFMT_BGR ){
	mpi->bpp=((out_fmt&255)+7)&(~7);
	mpi->flags|=MP_IMGFLAG_SWAPPED;
	return;
    }
    mpi->flags|=MP_IMGFLAG_YUV;
    switch(out_fmt){
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YV12:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=12;
	return;
    case IMGFMT_UYVY:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YUY2:
	mpi->bpp=16;
	return;
    }
    printf("mp_image: Unknown out_fmt: 0x%X\n",out_fmt);
    mpi->bpp=0;
}
#endif

static inline mp_image_t* new_mp_image(int w,int h){
    mp_image_t* mpi=malloc(sizeof(mp_image_t));
    if(!mpi) return NULL; // error!
    memset(mpi,0,sizeof(mp_image_t));
    mpi->width=mpi->w=w;
    mpi->height=mpi->h=h;
    return mpi;
}
