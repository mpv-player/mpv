#include "../config.h"
#ifdef HAVE_PNG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <png.h>

#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "vf_scale.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/swscale.h"
#include "../postproc/rgb2rgb.h"

struct vf_priv_s {
    int frameno;
    char fname[102];
    int shot, store_slices;
    int dw, dh, stride;
    uint8_t *buffer;
    struct SwsContext *ctx;
    mp_image_t *dmpi;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
		  int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt)
{
    int int_sws_flags=0;
    SwsFilter *srcFilter, *dstFilter;
    
    sws_getFlagsAndFilterFromCmdLine(&int_sws_flags, &srcFilter, &dstFilter);

    vf->priv->ctx=sws_getContext(width, height, outfmt,
				 d_width, d_height, IMGFMT_BGR24,
				 int_sws_flags | get_sws_cpuflags(), srcFilter, dstFilter, NULL);

    vf->priv->dw = d_width;
    vf->priv->dh = d_height;
    vf->priv->stride = (3*vf->priv->dw+15)&~15;

    if (vf->priv->buffer) free(vf->priv->buffer); // probably reconfigured
    vf->priv->dmpi = NULL;

    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int write_png(char *fname, unsigned char *buffer, int width, int height, int stride)
{
    FILE * fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_byte **row_pointers;
    int k;

    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    fp = NULL;

    if (setjmp(png_ptr->jmpbuf)) {
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return 0;
    }

    fp = fopen (fname, "wb");
    if (fp == NULL) {
	mp_msg(MSGT_VFILTER,MSGL_ERR,"\nPNG Error opening %s for writing!\n", fname);
	return 0;
    }
        
    png_init_io(png_ptr, fp);
    png_set_compression_level(png_ptr, 0);

    png_set_IHDR(png_ptr, info_ptr, width, height,
		 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);
        
    png_set_bgr(png_ptr);

    row_pointers = (png_byte**)malloc(height*sizeof(png_byte*));
    for (k = 0; k < height; k++) {
	unsigned char* s=buffer + stride*k;
	row_pointers[k] = s;
    }

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    free(row_pointers);

    fclose (fp);
}

static int fexists(char *fname)
{
    struct stat dummy;
    if (stat(fname, &dummy) == 0) return 1;
    else return 0;
}

static void gen_fname(struct vf_priv_s* priv)
{
    do {
	snprintf (priv->fname, 100, "shot%04d.png", ++priv->frameno);
    } while (fexists(priv->fname) && priv->frameno < 100000);
    if (fexists(priv->fname)) return;

    mp_msg(MSGT_VFILTER,MSGL_INFO,"*** screenshot '%s' ***\n",priv->fname);

}

static void scale_image(struct vf_priv_s* priv)
{
    uint8_t *dst[3];
    int dst_stride[3];
	
    dst_stride[0] = priv->stride;
    dst_stride[1] = dst_stride[2] = 0;
    if (!priv->buffer)
	priv->buffer = (uint8_t*)memalign(16, dst_stride[0]*priv->dh);

    dst[0] = priv->buffer;
    dst[1] = dst[2] = 0;
    sws_scale_ordered(priv->ctx, priv->dmpi->planes, priv->dmpi->stride, 0, priv->dmpi->height, dst, dst_stride);
}

static void start_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    if(!vf->next->draw_slice) {
	mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
	return;
    }
    if(!(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)) return; // shouldn't happen
    mpi->priv=vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	mpi->type, mpi->flags, mpi->width, mpi->height);
    if (vf->priv->shot) {
	vf->priv->store_slices = 1;
	vf->priv->shot = 0;
	if (!vf->priv->buffer)
	    vf->priv->buffer = (uint8_t*)memalign(16, vf->priv->stride*vf->priv->dh);
    }
    
}

static void draw_slice(struct vf_instance_s* vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
    mp_image_t *dmpi=vf->dmpi;
    if(!dmpi){
	mp_msg(MSGT_VFILTER,MSGL_FATAL,"vf_screenshot: draw_slice() called with dmpi=NULL (no get_image??)\n");
	return;
    }
    if (vf->priv->store_slices) {
	uint8_t *dst[3];
	int dst_stride[3];
	dst_stride[0] = vf->priv->stride;
	dst_stride[1] = dst_stride[2] = 0;
	dst[0] = vf->priv->buffer;
	dst[1] = dst[2] = 0;
	sws_scale_ordered(vf->priv->ctx, src, stride, y, h, dst, dst_stride);
    }
    vf_next_draw_slice(vf,src,stride,w,h,x,y);
}

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    vf->dmpi= vf_get_image(vf->next, mpi->imgfmt, 
			   mpi->type, mpi->flags | MP_IMGFLAG_READABLE, mpi->width, mpi->height);

    mpi->planes[0]=vf->dmpi->planes[0];
    mpi->stride[0]=vf->dmpi->stride[0];
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	mpi->planes[1]=vf->dmpi->planes[1];
	mpi->planes[2]=vf->dmpi->planes[2];
	mpi->stride[1]=vf->dmpi->stride[1];
	mpi->stride[2]=vf->dmpi->stride[2];
    }
    mpi->width=vf->dmpi->width;

    mpi->flags|=MP_IMGFLAG_DIRECT;
    mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
    
    mpi->priv=(void*)vf->dmpi;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
    if (mpi->flags&MP_IMGFLAG_DRAW_CALLBACK) {
	if(vf->priv->store_slices) {
	    vf->priv->store_slices = 0;
	    gen_fname(vf->priv);
	    write_png(vf->priv->fname, vf->priv->buffer, vf->priv->dw, vf->priv->dh, vf->priv->stride);
	}
	return vf_next_put_image(vf,vf->dmpi);
    }
    
    if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
	vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
				    MP_IMGTYPE_EXPORT, 0,
				    mpi->width, mpi->height);
	vf_clone_mpi_attributes(vf->priv->dmpi, mpi);
    } else {
	vf->priv->dmpi=vf->dmpi;
    }
    
    vf->priv->dmpi->planes[0]=mpi->planes[0];
    vf->priv->dmpi->planes[1]=mpi->planes[1];
    vf->priv->dmpi->planes[2]=mpi->planes[2];
    vf->priv->dmpi->stride[0]=mpi->stride[0];
    vf->priv->dmpi->stride[1]=mpi->stride[1];
    vf->priv->dmpi->stride[2]=mpi->stride[2];
    vf->priv->dmpi->width=mpi->width;
    vf->priv->dmpi->height=mpi->height;

    if(vf->priv->shot) {
	vf->priv->shot=0;
	gen_fname(vf->priv);
	scale_image(vf->priv);
	write_png(vf->priv->fname, vf->priv->buffer, vf->priv->dw, vf->priv->dh, vf->priv->stride);
    }

    return vf_next_put_image(vf, vf->priv->dmpi);
}

int control (vf_instance_t *vf, int request, void *data)
{
    if(request==VFCTRL_SCREENSHOT) {
	vf->priv->shot=1;
        return CONTROL_TRUE;
    }
    return vf_next_control (vf, request, data);
}


//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_UYVY:
    case IMGFMT_YUY2:
    case IMGFMT_BGR32:
    case IMGFMT_BGR24:
    case IMGFMT_BGR16:
    case IMGFMT_BGR15:
    case IMGFMT_RGB32:
    case IMGFMT_RGB24:
    case IMGFMT_Y800: 
    case IMGFMT_Y8: 
    case IMGFMT_YVU9: 
    case IMGFMT_IF09: 
    case IMGFMT_444P: 
    case IMGFMT_422P: 
    case IMGFMT_411P: 
	return vf_next_query_format(vf, fmt);
    }
    return 0;
}

static int open(vf_instance_t *vf, char* args)
{
    vf->config=config;
    vf->control=control;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->get_image=get_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->frameno=0;
    vf->priv->shot=0;
    vf->priv->store_slices=0;
    vf->priv->buffer=0;
    vf->priv->ctx=0;
    return 1;
}

static void uninit(vf_instance_t *vf)
{
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    if (vf->priv->buffer) free(vf->priv->buffer);
    free(vf->priv);
}


vf_info_t vf_info_screenshot = {
    "screenshot to file",
    "screenshot",
    "A'rpi, Jindrich Makovicka",
    "",
    open,
    NULL
};

//===========================================================================//

#endif /* HAVE_PNG */
