/*
 * Copyright (C) 2009 Loren Merritt <lorenm@u.washignton.edu>
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
#include <inttypes.h>
#include <math.h>

#include "vf.h"

#include "common/common.h"
#include "options/m_option.h"

#include "vf_lavfi.h"

struct vf_priv_s {
    float cfg_thresh;
    int cfg_radius;
    float cfg_size;
    int radius;

    struct vf_lw_opts *lw_opts;
} const vf_priv_dflt = {
  .cfg_thresh = 1.5,
  .cfg_radius = -1,
  .cfg_size = -1,
};

static void lavfi_recreate(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf_lw_old_priv(vf);
    int w = vf->fmt_in.w;
    int h = vf->fmt_in.h;
    p->radius = p->cfg_radius;
    if (p->cfg_size > -1)
        p->radius = (p->cfg_size / 100.0f) * sqrtf(w * w + h * h);
    p->radius = MPCLAMP((p->radius+1)&~1, 4, 32);
    vf_lw_update_graph(vf, "gradfun", "%f:%d", p->cfg_thresh, p->radius);
}

static int vf_open(vf_instance_t *vf)
{
    bool have_radius = vf->priv->cfg_radius > -1;
    bool have_size = vf->priv->cfg_size > -1;

    if (have_radius && have_size) {
        MP_ERR(vf, "scale: gradfun: only one of "
              "radius/size parameters allowed at the same time!\n");
        return 0;
    }

    if (!have_radius && !have_size)
        vf->priv->cfg_size = 1.0;

    if (vf_lw_set_graph(vf, vf->priv->lw_opts, "gradfun", "%f:4",
                        vf->priv->cfg_thresh) >= 0)
    {
        vf_lw_set_recreate_cb(vf, lavfi_recreate);
        return 1;
    }

    MP_FATAL(vf, "Requires libavfilter.\n");
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_FLOATRANGE("strength", cfg_thresh, 0, 0.51, 255),
    OPT_INTRANGE("radius", cfg_radius, 0, 4, 32),
    OPT_FLOATRANGE("size", cfg_size, 0, 0.1, 5.0),
    OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
    {0}
};

const vf_info_t vf_info_gradfun = {
    .description = "gradient deband",
    .name = "gradfun",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_dflt,
    .options = vf_opts_fields,
};
