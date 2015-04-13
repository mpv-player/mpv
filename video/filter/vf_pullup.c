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

#include "common/msg.h"
#include "options/m_option.h"

#include "vf.h"

#include "vf_lavfi.h"

struct vf_priv_s {
        struct pullup_context *ctx;
        int init;
        int fakecount;
        double lastpts;
        int junk_left, junk_right, junk_top, junk_bottom;
        int strict_breaks, metric_plane;
        struct vf_lw_opts *lw_opts;
};

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;
    const char *pname[3] = {"y", "u", "v"};
    if (vf_lw_set_graph(vf, p->lw_opts, "pullup", "%d:%d:%d:%d:%d:%s",
                        p->junk_left, p->junk_right, p->junk_top, p->junk_bottom,
                        p->strict_breaks, pname[p->metric_plane]) >= 0)
    {
        return 1;
    }

    MP_FATAL(vf, "This version of libavfilter has no 'pullup' filter.\n");
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_pullup = {
    .description = "pullup (from field sequence to frames)",
    .name = "pullup",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &(const struct vf_priv_s){
        .junk_left = 1,
        .junk_right = 1,
        .junk_top = 4,
        .junk_bottom = 4,
    },
    .options = (const struct m_option[]){
        OPT_INT("jl", junk_left, 0),
        OPT_INT("jr", junk_right, 0),
        OPT_INT("jt", junk_top, 0),
        OPT_INT("jb", junk_bottom, 0),
        OPT_INT("sb", strict_breaks, 0),
        OPT_CHOICE("mp", metric_plane, 0, ({"y", 0}, {"u", 1}, {"v", 2})),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};
