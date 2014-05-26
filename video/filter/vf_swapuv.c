/*
 * Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>
 *
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
#include <assert.h>

#include "common/msg.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    struct mp_image old = *mpi;
    mpi->planes[1] = old.planes[2];
    mpi->stride[1] = old.stride[2];
    mpi->planes[2] = old.planes[1];
    mpi->stride[2] = old.stride[1];
    return mpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!(desc.flags & MP_IMGFLAG_BYTE_ALIGNED))
        return 0;
    if (!(desc.num_planes >= 3 && desc.bytes[1] == desc.bytes[2]))
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int vf_open(vf_instance_t *vf){
    vf->filter=filter;
    vf->query_format=query_format;
    return 1;
}

const vf_info_t vf_info_swapuv = {
    .description = "UV swapper",
    .name = "swapuv",
    .open = vf_open,
};

//===========================================================================//
