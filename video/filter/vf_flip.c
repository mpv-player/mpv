/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common/msg.h"

#include "video/mp_image.h"
#include "vf.h"

#include "video/out/vo.h"

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_vflip(mpi);
    return mpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (!IMGFMT_IS_HWACCEL(fmt))
        return vf_next_query_format(vf, fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf){
    vf->filter=filter;
    vf->query_format = query_format;
    return 1;
}

const vf_info_t vf_info_flip = {
    .description = "flip image upside-down",
    .name = "flip",
    .open = vf_open,
};
