/*
 * Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
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

#include <stdlib.h>

#include "common/msg.h"
#include "options/m_option.h"
#include "vf.h"

#include "vf_lavfi.h"

struct vf_priv_s {
    double strength[4];
    struct vf_lw_opts *lw_opts;
};

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *s = vf->priv;

    if (vf_lw_set_graph(vf, s->lw_opts, "hqdn3d", "%f:%f:%f:%f",
                        s->strength[0], s->strength[1],
                        s->strength[2], s->strength[3]) >= 0)
    {
        return 1;
    }

    MP_FATAL(vf, "Requires libavfilter.\n");
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_hqdn3d = {
    .description = "High Quality 3D Denoiser",
    .name = "hqdn3d",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = (const struct m_option[]){
        OPT_DOUBLE("luma_spatial", strength[0], 0),
        OPT_DOUBLE("chroma_spatial", strength[1], 0),
        OPT_DOUBLE("luma_tmp", strength[2], 0),
        OPT_DOUBLE("chroma_tmp", strength[3], 0),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};

//===========================================================================//
