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
#include "mpvcore/mp_msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "video/memcpy_pic.h"

struct vf_priv_s {
	int state;
	long long in;
	long long out;
	struct vf_detc_pts_buf ptsbuf;
	int last_frame_duration;
        struct mp_image *buffer;
};

static void copy_pic_field(struct mp_image *dmpi, struct mp_image *mpi, int f)
{
    for (int p = 0; p < dmpi->num_planes; p++) {
        my_memcpy_pic(dmpi->planes[p] + dmpi->stride[p] * f,
                      mpi->planes[p] + mpi->stride[p] * f,
                      mpi->plane_w[p], mpi->plane_h[p] / 2,
                      dmpi->stride[p] * 2, mpi->stride[p] * 2);
    }
}

static int filter(struct vf_instance *vf, struct mp_image *mpi)
{
	int flags = mpi->fields;
	int state = vf->priv->state;
        struct vf_priv_s *p = vf->priv;

        if (!p->buffer || p->buffer->w != mpi->w || p->buffer->h != mpi->h ||
            p->buffer->imgfmt != mpi->imgfmt)
        {
            mp_image_unrefp(&p->buffer);
            p->buffer = mp_image_alloc(mpi->imgfmt, mpi->w, mpi->h);
            talloc_steal(vf, p->buffer);
        }
        mp_image_copy_attributes(p->buffer, mpi);

        struct mp_image *dmpi = p->buffer;

	vf->priv->in++;

	if ((state == 0 &&
	     !(flags & MP_IMGFIELD_TOP_FIRST)) ||
	    (state == 1 &&
	     flags & MP_IMGFIELD_TOP_FIRST)) {
		mp_msg(MSGT_VFILTER, MSGL_WARN,
		       "softpulldown: Unexpected field flags: state=%d top_field_first=%d repeat_first_field=%d\n",
		       state,
		       (flags & MP_IMGFIELD_TOP_FIRST) != 0,
		       (flags & MP_IMGFIELD_REPEAT_FIRST) != 0);
		state ^= 1;
	}

	if (state == 0) {
                struct mp_image *new = mp_image_new_ref(mpi);
                new->pts = vf_softpulldown_adjust_pts(&vf->priv->ptsbuf, mpi->pts, 0, 0, vf->priv->last_frame_duration);
                vf_add_output_frame(vf, new);
		vf->priv->out++;
		if (flags & MP_IMGFIELD_REPEAT_FIRST) {
			copy_pic_field(dmpi, mpi, 0);
			state=1;
		}
	} else {
		copy_pic_field(dmpi, mpi, 1);
		struct mp_image *new = mp_image_new_ref(mpi);
                new->pts = vf_softpulldown_adjust_pts(&vf->priv->ptsbuf, mpi->pts, 0, 0, vf->priv->last_frame_duration);
		vf_add_output_frame(vf, new);
		vf->priv->out++;
		if (flags & MP_IMGFIELD_REPEAT_FIRST) {
                        struct mp_image *new2 = mp_image_new_ref(mpi);
                        new2->pts = vf_softpulldown_adjust_pts(&vf->priv->ptsbuf, mpi->pts, 0, 0, 3);
                        vf_add_output_frame(vf, new2);
                        vf->priv->out++;
                        vf->priv->state=0;
		} else {
                        copy_pic_field(dmpi, mpi, 0);
		}
	}

	vf->priv->state = state;
	if (flags & MP_IMGFIELD_REPEAT_FIRST)
		vf->priv->last_frame_duration = 3;
	else
		vf->priv->last_frame_duration = 2;

        talloc_free(mpi);

        return 0;
}

static int control(vf_instance_t *vf, int request, void *data)
{
    switch (request) {
    case VFCTRL_SEEK_RESET:
        vf_detc_init_pts_buf(&vf->priv->ptsbuf);
        return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(struct vf_instance *vf)
{
	mp_msg(MSGT_VFILTER, MSGL_INFO, "softpulldown: %lld frames in, %lld frames out\n", vf->priv->in, vf->priv->out);
	free(vf->priv);
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!(desc.flags & MP_IMGFLAG_YUV_P))
        return 0;
    if (desc.plane_bits != 8)
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int vf_open(vf_instance_t *vf)
{
    vf->filter_ext = filter;
    vf->control = control;
    vf->uninit = uninit;
    vf->query_format = query_format;
    vf->priv = calloc(1, sizeof(struct vf_priv_s));
    vf->priv->last_frame_duration = 2;
    vf_detc_init_pts_buf(&vf->priv->ptsbuf);
    return 1;
}

const vf_info_t vf_info_softpulldown = {
    .description = "mpeg2 soft 3:2 pulldown",
    .name = "softpulldown",
    .open = vf_open,
};
