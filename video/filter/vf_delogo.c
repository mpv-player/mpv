/*
 * Copyright (C) 2002 Jindrich Makovicka <makovick@gmail.com>
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
#include "vf.h"
#include "vf_lavfi.h"

#include "options/m_option.h"

static struct vf_priv_s {
    int xoff, yoff, lw, lh, band, show;
    struct vf_lw_opts *lw_opts;
} const vf_priv_dflt = {
    .band = 1,
};

static int vf_open(vf_instance_t *vf){
    struct vf_priv_s *p = vf->priv;

    int band = p->band;
    int show = p->show;
    if (band < 0) {
        band = 4;
        show = 1;
    }
    if (vf_lw_set_graph(vf, p->lw_opts, "delogo", "%d:%d:%d:%d:%d:%d",
                        p->xoff, p->yoff, p->lw, p->lh, band, show) >= 0)
    {
        return 1;
    }

    MP_FATAL(vf, "Requires libavfilter.\n");
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_INT("x", xoff, 0),
    OPT_INT("y", yoff, 0),
    OPT_INT("w", lw, 0),
    OPT_INT("h", lh, 0),
    OPT_INT("t", band, 0),
    OPT_INT("band", band, 0), // alias
    OPT_FLAG("show", show, 0),
    OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
    {0}
};

const vf_info_t vf_info_delogo = {
    .description = "simple logo remover",
    .name = "delogo",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_dflt,
    .options = vf_opts_fields,
};

//===========================================================================//
