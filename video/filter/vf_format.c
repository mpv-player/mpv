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
#include "common/msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "options/m_option.h"

struct vf_priv_s {
    int fmt;
    int outfmt;
};

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (fmt == vf->priv->fmt || !vf->priv->fmt) {
        if (vf->priv->outfmt)
            fmt = vf->priv->outfmt;
        return vf_next_query_format(vf, fmt);
    }
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    *out = *in;
    if (vf->priv->outfmt)
        out->imgfmt = vf->priv->outfmt;
    return 0;
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    // As documented in the manpage, the user can easily provoke crashes
    if (vf->priv->outfmt)
        mp_image_setfmt(mpi, vf->priv->outfmt);
    return mpi;
}

static int vf_open(vf_instance_t *vf)
{
    vf->query_format = query_format;
    vf->reconfig = reconfig;
    vf->filter = filter;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_IMAGEFORMAT("fmt", fmt, 0),
    OPT_IMAGEFORMAT("outfmt", outfmt, 0),
    {0}
};

const vf_info_t vf_info_format = {
    .description = "force output format",
    .name = "format",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};
