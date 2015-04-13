/*
 * Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>
 *
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

#include "config.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "vf.h"

#include "vf_lavfi.h"

struct vf_priv_s {
    int strength;
    int averaged;
    int pattern;
    int temporal;
    int uniform;
    struct vf_lw_opts *lw_opts;
};

static int vf_open(vf_instance_t *vf){
#define CH(f) ((f) ? '+' : '-')
    struct vf_priv_s *p = vf->priv;
    if (vf_lw_set_graph(vf, p->lw_opts, "noise", "-1:%d:%ca%cp%ct%cu",
                        p->strength, CH(p->averaged), CH(p->pattern),
                        CH(p->temporal), CH(p->uniform)) >= 0)
    {
        return 1;
    }

    MP_FATAL(vf, "This version of libavfilter has no 'noise' filter.\n");
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_noise = {
    .description = "noise generator",
    .name = "noise",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = (const struct m_option[]){
        OPT_INTRANGE("strength", strength, 0, 0, 100, OPTDEF_INT(2)),
        OPT_FLAG("averaged", averaged, 0),
        OPT_FLAG("pattern", pattern, 0),
        OPT_FLAG("temporal", temporal, 0),
        OPT_FLAG("uniform", uniform, 0),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};

//===========================================================================//
