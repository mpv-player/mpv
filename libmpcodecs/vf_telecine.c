/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
	double pts;
	double lastpts;
	mp_image_t *buffered_mpi;
};

static int continue_buffered_image_fullframe(struct vf_instance *vf)
{
	mp_image_t *mpi = vf->priv->buffered_mpi;
	mp_image_t *dmpi = vf_get_image(vf->next, mpi->imgfmt,
		MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE |
		MP_IMGFLAG_PRESERVE, mpi->width, mpi->height);

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
	return vf_next_put_image(vf, dmpi, vf->priv->pts);
}

static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts)
{
	mp_image_t *dmpi;
	int ret;

	vf->priv->frame = (vf->priv->frame+1)%4;

	if (pts != MP_NOPTS_VALUE) {
		if (vf->priv->lastpts == MP_NOPTS_VALUE) {
			vf->priv->pts = pts;
			vf->priv->lastpts = pts;
		} else {
			// we only increase by 80% of input pts at each frame; in the case
			// in which we render two frames, we jump back
			// this turns 23.98fps perfectly into 29.97fps
			vf->priv->pts += 0.8 * (pts - vf->priv->lastpts);
			vf->priv->lastpts = pts;
		}
	}

	ret = 0;
	//    0/0  1/1  2/2  2/3  3/0
	switch (vf->priv->frame) {
	case 0:
		dmpi = vf_get_image(vf->next, mpi->imgfmt,
			MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE |
			MP_IMGFLAG_PRESERVE, mpi->width, mpi->height);
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
		ret = vf_next_put_image(vf, dmpi, vf->priv->pts);
		vf->priv->pts = pts;
		vf->priv->buffered_mpi = mpi;
		vf_queue_frame(vf, continue_buffered_image_fullframe);
		return ret;
	case 1:
	case 2:
		vf->priv->buffered_mpi = mpi;
		return continue_buffered_image_fullframe(vf);
	case 3:
		dmpi = vf_get_image(vf->next, mpi->imgfmt,
			MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE |
			MP_IMGFLAG_PRESERVE, mpi->width, mpi->height);
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
		ret = vf_next_put_image(vf, dmpi, vf->priv->pts);
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

#if 0
static int query_format(struct vf_instance *vf, unsigned int fmt)
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

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}
#endif

static void uninit(struct vf_instance *vf)
{
	free(vf->priv);
}

static int vf_open(vf_instance_t *vf, char *args)
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
	vf->priv->pts = MP_NOPTS_VALUE;
	vf->priv->lastpts = MP_NOPTS_VALUE;
	return 1;
}

const vf_info_t vf_info_telecine = {
    "telecine filter",
    "telecine",
    "Rich Felker",
    "",
    vf_open,
    NULL
};
