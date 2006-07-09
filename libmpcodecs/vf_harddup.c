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
	mp_image_t *last_mpi;
};

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
	mp_image_t *dmpi;

	vf->priv->last_mpi = mpi;
	
	dmpi = vf_get_image(vf->next, mpi->imgfmt,
		MP_IMGTYPE_EXPORT, 0, mpi->width, mpi->height);

	dmpi->planes[0] = mpi->planes[0];
	dmpi->stride[0] = mpi->stride[0];
	if (dmpi->flags&MP_IMGFLAG_PLANAR) {
		dmpi->planes[1] = mpi->planes[1];
		dmpi->stride[1] = mpi->stride[1];
		dmpi->planes[2] = mpi->planes[2];
		dmpi->stride[2] = mpi->stride[2];
	}
	
	return vf_next_put_image(vf, dmpi, pts);
}

static int control(struct vf_instance_s* vf, int request, void* data)
{
	switch (request) {
	case VFCTRL_DUPLICATE_FRAME:
		if (!vf->priv->last_mpi) break;
		// This is a huge hack. We assume nothing
		// has been called earlier in the filter chain
		// since the last put_image. This is reasonable
		// because we're handling a duplicate frame!
		if (put_image(vf, vf->priv->last_mpi, MP_NOPTS_VALUE))
			return CONTROL_TRUE;
		break;
	}
	return vf_next_control(vf, request, data);
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

static void uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args)
{
	vf->put_image = put_image;
	vf->control = control;
	vf->uninit = uninit;
	vf->priv = calloc(1, sizeof(struct vf_priv_s));
	return 1;
}

vf_info_t vf_info_harddup = {
    "resubmit duplicate frames for encoding",
    "harddup",
    "Rich Felker",
    "",
    open,
    NULL
};


