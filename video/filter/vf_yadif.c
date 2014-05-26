/*
 * Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>
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

#include "options/options.h"

#include "common/msg.h"
#include "vf.h"

#include "vf_lavfi.h"

struct vf_priv_s {
    int mode;
    int do_deinterlace;
    struct vf_lw_opts *lw_opts;
};

static const struct vf_priv_s vf_priv_default = {
    .do_deinterlace = 1,
};

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    // Earlier libavfilter yadif versions used pure integers for the first
    // option. We can't/don't handle this, but at least allow usage of the
    // filter with default settings. So use an empty string for "send_frame".
    const char *mode[] = {"", "send_field", "send_frame_nospatial",
                          "send_field_nospatial"};

    if (vf_lw_set_graph(vf, p->lw_opts, "yadif", "%s", mode[p->mode]) >= 0)
    {
        return 1;
    }

    MP_FATAL(vf, "Requires libavfilter.\n");
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_CHOICE("mode", mode, 0,
               ({"frame", 0},
                {"field", 1},
                {"frame-nospatial", 2},
                {"field-nospatial", 3})),
    OPT_FLAG("enabled", do_deinterlace, 0),
    OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
    {0}
};

const vf_info_t vf_info_yadif = {
    .description = "Yet Another DeInterlacing Filter",
    .name = "yadif",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_default,
    .options = vf_opts_fields,
};
