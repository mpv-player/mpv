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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "talloc.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/out/vo.h"

#include "vf.h"

struct vf_priv_s {
    struct mp_image *current;
};

static int config(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt)
{
    mp_image_unrefp(&vf->priv->current);
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_unrefp(&vf->priv->current);
    vf->priv->current = talloc_steal(vf, mp_image_new_ref(mpi));
    return mpi;
}

static int control (vf_instance_t *vf, int request, void *data)
{
    if (request == VFCTRL_SCREENSHOT && vf->priv->current) {
        struct voctrl_screenshot_args *args = data;
        args->out_image = mp_image_new_ref(vf->priv->current);
        return CONTROL_TRUE;
    }
    return vf_next_control (vf, request, data);
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (mp_sws_supported_format(fmt))
        return vf_next_query_format(vf, fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf)
{
    vf->config = config;
    vf->control = control;
    vf->filter = filter;
    vf->query_format = query_format;
    vf->priv = talloc_zero(vf, struct vf_priv_s);
    return 1;
}

const vf_info_t vf_info_screenshot = {
    .description = "screenshot to file",
    .name = "screenshot",
    .open = vf_open,
};
