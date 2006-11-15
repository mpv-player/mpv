#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <png.h>

#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "vf_scale.h"

#include "libvo/fastmemcpy.h"
#include "libswscale/swscale.h"

struct vf_priv_s {
    int frameno;
    char fname[102];
    /// shot stores current screenshot mode:
    /// 0: don't take screenshots
    /// 1: take single screenshot, reset to 0 afterwards
    /// 2: take screenshots of each frame
    int shot, store_slices;
    int dw, dh, stride;
    uint8_t *buffer;
    struct SwsContext *ctx;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
		  int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt)
{
    vf->priv->ctx=sws_getContextFromCmdLine(width, height, outfmt,
				 d_width, d_height, IMGFMT_BGR24);

    vf->priv->dw = d_width;
    vf->priv->dh = d_height;
    vf->priv->stride = (3*vf->priv->dw+15)&~15;

    if (vf->priv->buffer) free(vf->priv->buffer); // probably reconfigured
    vf->priv->buffer = NULL;

    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void write_png(char *fname, unsigned char *buffer, int width, int height, int stride)
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
	return;
    }

    fp = fopen (fname, "wb");
    if (fp == NULL) {
	mp_msg(MSGT_VFILTER,MSGL_ERR,"\nPNG Error opening %s for writing!\n", fname);
	return;
    }
        
    png_init_io(png_ptr, fp);
    png_set_compression_level(png_ptr, 0);

    png_set_IHDR(png_ptr, info_ptr, width, height,
		 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);
        
    png_set_bgr(png_ptr);

    row_pointers = malloc(height*sizeof(png_byte*));
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
    if (fexists(priv->fname)) {
	priv->fname[0] = '\0';
	return;
    }

    mp_msg(MSGT_VFILTER,MSGL_INFO,"*** screenshot '%s' ***\n",priv->fname);

}

static void scale_image(struct vf_priv_s* priv, mp_image_t *mpi)
{
    uint8_t *dst[3];
    int dst_stride[3];
	
    dst_stride[0] = priv->stride;
    dst_stride[1] = dst_stride[2] = 0;
    if (!priv->buffer)
	priv->buffer = (uint8_t*)memalign(16, dst_stride[0]*priv->dh);

    dst[0] = priv->buffer;
    dst[1] = dst[2] = 0;
    sws_scale_ordered(priv->ctx, mpi->planes, mpi->stride, 0, priv->dh, dst, dst_stride);
}

static void start_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	mpi->type, mpi->flags, mpi->width, mpi->height);
    if (vf->priv->shot) {
	vf->priv->store_slices = 1;
	if (!vf->priv->buffer)
	    vf->priv->buffer = (uint8_t*)memalign(16, vf->priv->stride*vf->priv->dh);
    }
    
}

static void draw_slice(struct vf_instance_s* vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
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
    // FIXME: should vf.c really call get_image when using slices??
    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
      return;
    vf->dmpi= vf_get_image(vf->next, mpi->imgfmt, 
			   mpi->type, mpi->flags/* | MP_IMGFLAG_READABLE*/, mpi->width, mpi->height);

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
    
    mpi->priv=(void*)vf->dmpi;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
    mp_image_t *dmpi = (mp_image_t *)mpi->priv;
    
    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
      dmpi = vf->dmpi;
    else
    if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
	dmpi=vf_get_image(vf->next,mpi->imgfmt,
				    MP_IMGTYPE_EXPORT, 0,
				    mpi->width, mpi->height);
	vf_clone_mpi_attributes(dmpi, mpi);
	dmpi->planes[0]=mpi->planes[0];
	dmpi->planes[1]=mpi->planes[1];
	dmpi->planes[2]=mpi->planes[2];
	dmpi->stride[0]=mpi->stride[0];
	dmpi->stride[1]=mpi->stride[1];
	dmpi->stride[2]=mpi->stride[2];
	dmpi->width=mpi->width;
	dmpi->height=mpi->height;
    }

    if(vf->priv->shot) {
	if (vf->priv->shot==1)
	    vf->priv->shot=0;
	gen_fname(vf->priv);
	if (vf->priv->fname[0]) {
	    if (!vf->priv->store_slices)
	      scale_image(vf->priv, dmpi);
	    write_png(vf->priv->fname, vf->priv->buffer, vf->priv->dw, vf->priv->dh, vf->priv->stride);
	}
	vf->priv->store_slices = 0;
    }

    return vf_next_put_image(vf, dmpi, pts);
}

int control (vf_instance_t *vf, int request, void *data)
{
    /** data contains an integer argument
     * 0: take screenshot with the next frame
     * 1: take screenshots with each frame until the same command is given once again
     **/
    if(request==VFCTRL_SCREENSHOT) {
	if (data && *(int*)data) { // repeated screenshot mode
	    if (vf->priv->shot==2)
		vf->priv->shot=0;
	    else
		vf->priv->shot=2;
	} else { // single screenshot
	    if (!vf->priv->shot)
		vf->priv->shot=1;
	}
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

static void uninit(vf_instance_t *vf);
// open conflicts with stdio.h at least under MinGW
static int screenshot_open(vf_instance_t *vf, char* args)
{
    vf->config=config;
    vf->control=control;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->get_image=get_image;
    vf->uninit=uninit;
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
    screenshot_open,
    NULL
};

//===========================================================================//
