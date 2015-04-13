/*
 * Copyright (C) 2002 Remi Guyomarch <rguyom@pobox.com>
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

#include <stdlib.h>

#include "common/msg.h"
#include "options/m_option.h"

#include "vf.h"

#include "vf_lavfi.h"

typedef struct FilterParam {
    int msizeX, msizeY;
    double amount;
} FilterParam;

struct vf_priv_s {
    FilterParam lumaParam;
    FilterParam chromaParam;
    struct vf_lw_opts *lw_opts;
};

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    p->lumaParam.msizeX |= 1;
    p->lumaParam.msizeY |= 1;
    p->chromaParam.msizeX |= 1;
    p->chromaParam.msizeY |= 1;

    if (vf_lw_set_graph(vf, p->lw_opts, "unsharp", "%d:%d:%f:%d:%d:%f",
                        p->lumaParam.msizeX, p->lumaParam.msizeY, p->lumaParam.amount,
                        p->chromaParam.msizeX, p->chromaParam.msizeY, p->chromaParam.amount)
                       >= 0)
    {
        return 1;
    }

    MP_FATAL(vf, "This version of libavfilter has no 'unsharp' filter.\n");
    return 0;
}

// same as MIN_/MAX_MATRIX_SIZE
#define MIN_SIZE 3
#define MAX_SIZE 63

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_unsharp = {
    .description = "unsharp mask & gaussian blur",
    .name = "unsharp",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &(const struct vf_priv_s){
        .lumaParam = {5, 5, 1.0},
        .chromaParam = {5, 5, 0.0},
    },
    .options = (const struct m_option[]){
        OPT_INTRANGE("lx", lumaParam.msizeX, 0, MIN_SIZE, MAX_SIZE),
        OPT_INTRANGE("ly", lumaParam.msizeY, 0, MIN_SIZE, MAX_SIZE),
        OPT_DOUBLE("la", lumaParam.amount, CONF_RANGE, .min = -2, .max = 6),
        OPT_INTRANGE("cx", chromaParam.msizeX, 0, MIN_SIZE, MAX_SIZE),
        OPT_INTRANGE("cy", chromaParam.msizeY, 0, MIN_SIZE, MAX_SIZE),
        OPT_DOUBLE("ca", chromaParam.amount, CONF_RANGE, .min = -2, .max = 6),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};
