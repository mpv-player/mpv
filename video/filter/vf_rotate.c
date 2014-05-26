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

#include "common/msg.h"
#include "options/m_option.h"

#include "vf.h"
#include "vf_lavfi.h"

struct vf_priv_s {
    int angle;
    struct vf_lw_opts *lw_opts;
};

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    static const char *rot[] = {
        "null",
        "transpose=clock",
        "vflip,hflip",
        "transpose=cclock",
    };

    if (vf_lw_set_graph(vf, p->lw_opts, NULL, "%s", rot[p->angle]) >= 0)
        return 1;

    MP_FATAL(vf, "Requires libavfilter.\n");
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_rotate = {
    .description = "rotate",
    .name = "rotate",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = (const struct m_option[]){
        OPT_CHOICE("angle", angle, 0,
                   ({"0", 0},
                    {"90", 1},
                    {"180", 2},
                    {"270", 3})),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};

//===========================================================================//
