#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"

struct vf_priv_s {
	int mode;
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

static void deint(unsigned char *dest, int ds, unsigned char *src, int ss, int w, int h, int field)
{
	int x, y;
	src += ss;
	dest += ds;
	if (field) {
		src += ss;
		dest += ds;
		h -= 2;
	}
	for (y=h/2; y; y--) {
		for (x=0; x<w; x++) {
			if (((src[x-ss] < src[x]) && (src[x+ss] < src[x])) ||
				((src[x-ss] > src[x]) && (src[x+ss] > src[x]))) {
				//dest[x] = (src[x+ss] + src[x-ss])>>1;
				dest[x] = ((src[x+ss]<<1) + (src[x-ss]<<1)
					+ src[x+ss+1] + src[x-ss+1]
					+ src[x+ss-1] + src[x-ss-1])>>3;
			}
			else dest[x] = src[x];
		}
		dest += ds<<1;
		src += ss<<1;
	}
}



static int put_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
	int ret;
	mp_image_t *dmpi;

	switch (vf->priv->mode) {
	case 0:
		dmpi = vf_get_image(vf->next, mpi->imgfmt,
			MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			mpi->width, mpi->height/2);
		memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h/2,
			dmpi->stride[0], mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			memcpy_pic(dmpi->planes[1], mpi->planes[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1], mpi->stride[1]*2);
			memcpy_pic(dmpi->planes[2], mpi->planes[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2], mpi->stride[2]*2);
		}
		ret = vf_next_put_image(vf, dmpi);
		
		memcpy_pic(dmpi->planes[0], mpi->planes[0] + mpi->stride[0],
			mpi->w, mpi->h/2, dmpi->stride[0], mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			memcpy_pic(dmpi->planes[1], mpi->planes[1] + mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1], mpi->stride[1]*2);
			memcpy_pic(dmpi->planes[2], mpi->planes[2] + mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2], mpi->stride[2]*2);
		}
		return vf_next_put_image(vf, dmpi) || ret;
	case 1:
		dmpi = vf_get_image(vf->next, mpi->imgfmt,
			MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			mpi->width, mpi->height);
		my_memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h/2,
			dmpi->stride[0]*2, mpi->stride[0]*2);
		deint(dmpi->planes[0], dmpi->stride[0], mpi->planes[0], mpi->stride[0], mpi->w, mpi->h, 0);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1], mpi->planes[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2], mpi->planes[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
			deint(dmpi->planes[1], dmpi->stride[1], mpi->planes[1], mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height, 0);
			deint(dmpi->planes[2], dmpi->stride[2], mpi->planes[2], mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height, 0);
		}
		ret = vf_next_put_image(vf, dmpi);
		
		my_memcpy_pic(dmpi->planes[0] + dmpi->stride[0], mpi->planes[0] + mpi->stride[0],
			mpi->w, mpi->h/2, dmpi->stride[0]*2, mpi->stride[0]*2);
		deint(dmpi->planes[0], dmpi->stride[0], mpi->planes[0], mpi->stride[0], mpi->w, mpi->h, 1);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1] + dmpi->stride[1], mpi->planes[1] + mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2] + dmpi->stride[2], mpi->planes[2] + mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
			deint(dmpi->planes[1], dmpi->stride[1], mpi->planes[1], mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height, 1);
			deint(dmpi->planes[2], dmpi->stride[2], mpi->planes[2], mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height, 1);
		}
		return vf_next_put_image(vf, dmpi) || ret;
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
	switch (vf->priv->mode) {
	case 0:
		return vf_next_config(vf,width,height/2,d_width,d_height,flags,outfmt);
	case 1:
		return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
	}
	return 0;
}

static void uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args)
{
	struct vf_priv_s *p;
	vf->config = config;
	vf->put_image = put_image;
	vf->query_format = query_format;
	vf->uninit = uninit;
	vf->default_reqs = VFCAP_ACCEPT_STRIDE;
	vf->priv = p = calloc(1, sizeof(struct vf_priv_s));
	vf->priv->mode = 0;
	if (args) sscanf(args, "%d", &vf->priv->mode);
	return 1;
}

vf_info_t vf_info_tfields = {
    "temporal field separation",
    "tfields",
    "Rich Felker",
    "",
    open,
    NULL
};


