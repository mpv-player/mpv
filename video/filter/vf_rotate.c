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
#include <inttypes.h>

#include "common/msg.h"
#include "options/m_option.h"

#include "vf.h"
#include "vf_lavfi.h"

struct vf_priv_s {
    int angle;
    struct vf_lw_opts *lw_opts;
};

static const char *const rot[] = {
    "null",
    "transpose=clock",
    "vflip,hflip",
    "transpose=cclock",
    "null", // actually set in lavfi_recreate()
};

static int lavfi_reconfig(struct vf_instance *vf,
                          struct mp_image_params *in,
                          struct mp_image_params *out)
{
    struct vf_priv_s *p = vf_lw_old_priv(vf);
    if (p->angle == 4) { // "auto"
        int r = in->rotate;
        if (r < 0 || r >= 360 || (r % 90) != 0) {
            MP_ERR(vf, "Can't apply rotation of %d degrees.\n", r);
            return -1;
        }
        vf_lw_update_graph(vf, NULL, "%s", rot[(r / 90) % 360]);
        out->rotate = 0;
    }
    return 0;
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    if (vf_lw_set_graph(vf, p->lw_opts, NULL, "%s", rot[p->angle]) >= 0) {
        vf_lw_set_reconfig_cb(vf, lavfi_reconfig);
        return 1;
    }

    return 0;
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
                    {"270", 3},
                    {"auto", 4})),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};

//===========================================================================//
