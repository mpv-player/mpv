#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/video_out.h"
#include "../libvo/fastmemcpy.h"
#include "../postproc/rgb2rgb.h"

struct vf_priv_s {
	unsigned char *buf;
	int brightness;
	int contrast;
};

static void process(unsigned char *dest, int dstride, unsigned char *src, int sstride,
		    int w, int h, int brightness, int contrast)
{
	int i;
	int pel;
	int dstep = dstride-w;
	int sstep = sstride-w;

	brightness = ((brightness+100)*511)/200-128;
	contrast = ((contrast+100)*512)/200;

	while (h--) {
		for (i = w; i; i--)
		{
			/* slow */
			pel = ((*src++ - 128) * contrast)/256 + brightness;
			*dest++ = pel > 255 ? 255 : (pel < 0 ? 0 : pel);
		}
		src += sstep;
		dest += dstep;
	}
}

/* FIXME: add packed yuv version of process, and optimized code! */

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
	mp_image_t *dmpi;

	dmpi=vf_get_image(vf->next, mpi->imgfmt,
			  MP_IMGTYPE_EXPORT, 0,
			  mpi->w, mpi->h);
	
	dmpi->stride[0] = mpi->stride[0];
	dmpi->planes[1] = mpi->planes[1];
	dmpi->planes[2] = mpi->planes[2];
	dmpi->stride[1] = mpi->stride[1];
	dmpi->stride[2] = mpi->stride[2];

	if (!vf->priv->buf) vf->priv->buf = malloc(mpi->stride[0]*mpi->h);
	
	if ((vf->priv->brightness == 0) && (vf->priv->contrast == 0))
		dmpi->planes[0] = mpi->planes[0];
	else {
		dmpi->planes[0] = vf->priv->buf;
		process(dmpi->planes[0], dmpi->stride[0],
			mpi->planes[0], mpi->stride[0],
			mpi->w, mpi->h, vf->priv->brightness,
			vf->priv->contrast);
	}

	vf_next_put_image(vf,dmpi);
}

static int control(struct vf_instance_s* vf, int request, void* data)
{
	vf_equalizer_t *eq;

	switch (request) {
	case VFCTRL_SET_EQUALIZER:
		eq = data;
		if (!strcmp(eq->item,"brightness")) {
			vf->priv->brightness = eq->value;
			return CONTROL_TRUE;
		}
		else if (!strcmp(eq->item,"contrast")) {
			vf->priv->contrast = eq->value;
			return CONTROL_TRUE;
		}
		break;
	case VFCTRL_GET_EQUALIZER:
		eq = data;
		if (!strcmp(eq->item,"brightness")) {
			eq->value = vf->priv->brightness;
			return CONTROL_TRUE;
		}
		else if (!strcmp(eq->item,"contrast")) {
			eq->value = vf->priv->contrast;
			return CONTROL_TRUE;
		}
		break;
	}
	return vf_next_control(vf, request, data);
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
	switch (fmt) {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_CLPL:
	case IMGFMT_Y800:
	case IMGFMT_Y8:
	case IMGFMT_NV12:
	case IMGFMT_444P:
	case IMGFMT_422P:
	case IMGFMT_411P:
		return vf_next_query_format(vf, fmt);
	}
	return 0;
}

static void uninit(struct vf_instance_s* vf)
{
	if (vf->priv->buf) free(vf->priv->buf);
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args)
{
	vf->control=control;
	vf->query_format=query_format;
	vf->put_image=put_image;
	vf->uninit=uninit;
	
	vf->priv = malloc(sizeof(struct vf_priv_s));
	memset(vf->priv, 0, sizeof(struct vf_priv_s));
	if (args) sscanf(args, "%d:%d", &vf->priv->brightness, &vf->priv->contrast);
	return 1;
}

vf_info_t vf_info_eq = {
	"soft video equalizer",
	"eq",
	"Richard Felker",
	"",
	open
};

