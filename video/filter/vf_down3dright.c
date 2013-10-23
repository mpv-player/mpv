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
#include <inttypes.h>

#include "config.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/cpudetect.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "video/memcpy_pic.h"

struct vf_priv_s {
	int skipline;
	int scalew;
	int scaleh;
};

static void toright(unsigned char *adst[3], unsigned char *asrc[3],
		    int dststride[3], int srcstride[3],
		    int w, int h, struct vf_priv_s* p)
{
	int k;

	for (k = 0; k < 3; k++) {
		unsigned char* fromL = asrc[k];
		unsigned char* fromR = asrc[k];
		unsigned char* to = adst[k];
		int src = srcstride[k];
                int dst = dststride[k];
		int ss;
		unsigned int dd;
		int i;

		if (k > 0) {
			i = h / 4 - p->skipline / 2;
			ss = src * (h / 4 + p->skipline / 2);
			dd = w / 4;
		} else {
			i = h / 2 - p->skipline;
                        ss = src * (h / 2 + p->skipline);
			dd = w / 2;
		}
		fromR += ss;
		for ( ; i > 0; i--) {
                        int j;
			unsigned char* t = to;
			unsigned char* sL = fromL;
			unsigned char* sR = fromR;

			if (p->scalew == 1) {
				for (j = dd; j > 0; j--) {
					*t++ = (sL[0] + sL[1]) / 2;
					sL+=2;
				}
				for (j = dd ; j > 0; j--) {
					*t++ = (sR[0] + sR[1]) / 2;
					sR+=2;
				}
			} else {
				for (j = dd * 2 ; j > 0; j--)
					*t++ = *sL++;
				for (j = dd * 2 ; j > 0; j--)
					*t++ = *sR++;
			}
			if (p->scaleh == 1) {
				memcpy(to + dst, to, dst);
                                to += dst;
			}
			to += dst;
			fromL += src;
			fromR += src;
		}
		//printf("K %d  %d   %d   %d  %d \n", k, w, h,  src, dst);
	}
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
	mp_image_t *dmpi = vf_alloc_out_image(vf);
        mp_image_copy_attributes(dmpi, mpi);

	toright(dmpi->planes, mpi->planes, dmpi->stride,
		mpi->stride, mpi->w, mpi->h, vf->priv);

        talloc_free(mpi);
	return dmpi;
}

static int config(struct vf_instance *vf,
		  int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt)
{
	/* FIXME - also support UYVY output? */
	return vf_next_config(vf, width * vf->priv->scalew,
			      height / vf->priv->scaleh - vf->priv->skipline, d_width, d_height, flags, IMGFMT_420P);
}


static int query_format(struct vf_instance *vf, unsigned int fmt)
{
	/* FIXME - really any YUV 4:2:0 input format should work */
	switch (fmt) {
	case IMGFMT_420P:
		return vf_next_query_format(vf, IMGFMT_420P);
	}
	return 0;
}

static void uninit(struct vf_instance *vf)
{
	free(vf->priv);
}

static int vf_open(vf_instance_t *vf, char *args)
{
	vf->config=config;
	vf->query_format=query_format;
	vf->filter=filter;
	vf->uninit=uninit;

	vf->priv = calloc(1, sizeof (struct vf_priv_s));
	vf->priv->skipline = 0;
	vf->priv->scalew = 1;
	vf->priv->scaleh = 2;
	if (args) sscanf(args, "%d:%d:%d", &vf->priv->skipline, &vf->priv->scalew, &vf->priv->scaleh);

	return 1;
}

const vf_info_t vf_info_down3dright = {
    .description = "convert stereo movie from top-bottom to left-right field",
    .name = "down3dright",
    .open = vf_open,
};
