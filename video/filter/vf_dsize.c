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
#include <limits.h>

#include "config.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

struct vf_priv_s {
    int w, h;
    int method; // aspect method, 0 -> downscale, 1-> upscale. +2 -> original aspect.
    int round;
    float aspect;
};

static int config(struct vf_instance *vf,
    int width, int height, int d_width, int d_height,
    unsigned int flags, unsigned int outfmt)
{
    int w = vf->priv->w;
    int h = vf->priv->h;
    if (vf->priv->aspect < 0.001) { // did the user input aspect or w,h params
        if (w == 0) w = d_width;
        if (h == 0) h = d_height;
        if (w == -1) w = width;
        if (h == -1) h = height;
        if (w == -2) w = h * (double)d_width / d_height;
        if (w == -3) w = h * (double)width / height;
        if (h == -2) h = w * (double)d_height / d_width;
        if (h == -3) h = w * (double)height / width;
        if (vf->priv->method > -1) {
            double aspect = (vf->priv->method & 2) ? ((double)height / width) : ((double)d_height / d_width);
            if ((h > w * aspect) ^ (vf->priv->method & 1)) {
                h = w * aspect;
            } else {
                w = h / aspect;
            }
        }
        if (vf->priv->round > 1) { // round up
            w += (vf->priv->round - 1 - (w - 1) % vf->priv->round);
            h += (vf->priv->round - 1 - (h - 1) % vf->priv->round);
        }
        d_width = w;
        d_height = h;
    } else {
        if (vf->priv->aspect * height > width) {
            d_width = height * vf->priv->aspect + .5;
            d_height = height;
        } else {
            d_height = width / vf->priv->aspect + .5;
            d_width = width;
        }
    }
    return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt);
}

static int vf_open(vf_instance_t *vf)
{
    vf->config = config;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_dsize = {
    .description = "reset displaysize/aspect",
    .name = "dsize",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &(const struct vf_priv_s){
        .aspect = 0.0,
        .w = -1,
        .h = -1,
        .method = -1,
        .round = 1,
    },
    .options = (const struct m_option[]){
        OPT_INTRANGE("w", w, 0, -3, INT_MAX),
        OPT_INTRANGE("h", h, 0, -3, INT_MAX),
        OPT_INTRANGE("method", method, 0, -1, 3),
        OPT_INTRANGE("round", round, 0, 0, 9999),
        OPT_FLOAT("aspect", aspect, CONF_RANGE, .min = 0, .max = 10),
        {0}
    },
};
