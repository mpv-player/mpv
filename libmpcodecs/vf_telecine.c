#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"

struct vf_priv_s {
	int frame;
};

static inline void *my_memcpy_pic(void * dst, void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	for(i=0; i<height; i++)
	{
		memcpy(dst, src, bytesPerLine);
		src+= srcStride;
		dst+= dstStride;
	}

	return retval;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
	mp_image_t *dmpi;
	int ret;

	vf->priv->frame = (vf->priv->frame+1)%4;
	
	dmpi = vf_get_image(vf->next, mpi->imgfmt,
		MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE |
		MP_IMGFLAG_PRESERVE, mpi->width, mpi->height);

	ret = 0;
	//    0/0  1/1  2/2  2/3  3/0
	switch (vf->priv->frame) {
	case 0:
		my_memcpy_pic(dmpi->planes[0]+dmpi->stride[0],
			mpi->planes[0]+mpi->stride[0], mpi->w, mpi->h/2,
			dmpi->stride[0]*2, mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1]+dmpi->stride[1],
				mpi->planes[1]+mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2]+dmpi->stride[2],
				mpi->planes[2]+mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
		}
		ret = vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
	case 1:
	case 2:
		memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h,
			dmpi->stride[0], mpi->stride[0]);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			memcpy_pic(dmpi->planes[1], mpi->planes[1],
				mpi->chroma_width, mpi->chroma_height,
				dmpi->stride[1], mpi->stride[1]);
			memcpy_pic(dmpi->planes[2], mpi->planes[2],
				mpi->chroma_width, mpi->chroma_height,
				dmpi->stride[2], mpi->stride[2]);
		}
		return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE) || ret;
	case 3:
		my_memcpy_pic(dmpi->planes[0]+dmpi->stride[0],
			mpi->planes[0]+mpi->stride[0], mpi->w, mpi->h/2,
			dmpi->stride[0]*2, mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1]+dmpi->stride[1],
				mpi->planes[1]+mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2]+dmpi->stride[2],
				mpi->planes[2]+mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
		}
		ret = vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
		my_memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h/2,
			dmpi->stride[0]*2, mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1], mpi->planes[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2], mpi->planes[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
		}
		return ret;
	}
	return 0;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
	/* FIXME - figure out which other formats work */
	switch (fmt) {
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
		return vf_next_query_format(vf, fmt);
	}
	return 0;
}

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args)
{
	//vf->config = config;
	vf->put_image = put_image;
	//vf->query_format = query_format;
	vf->uninit = uninit;
	vf->default_reqs = VFCAP_ACCEPT_STRIDE;
	vf->priv = calloc(1, sizeof(struct vf_priv_s));
	vf->priv->frame = 1;
	if (args) sscanf(args, "%d", &vf->priv->frame);
	vf->priv->frame--;
	return 1;
}

vf_info_t vf_info_telecine = {
    "telecine filter",
    "telecine",
    "Rich Felker",
    "",
    open,
    NULL
};


