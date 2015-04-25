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
#include <stdbool.h>

#include <libavutil/common.h>

#include "config.h"
#include "common/msg.h"
#include "options/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "options/m_option.h"

static struct vf_priv_s {
    // These four values are a backup of the values parsed from the command line.
    // This is necessary so that we do not get a mess upon filter reinit due to
    // e.g. aspect changes and with only aspect specified on the command line,
    // where we would otherwise use the values calculated for a different aspect
    // instead of recalculating them again.
    int cfg_exp_w, cfg_exp_h;
    int cfg_exp_x, cfg_exp_y;
    int exp_w,exp_h;
    int exp_x,exp_y;
    double aspect;
    int round;
} const vf_priv_dflt = {
  -1,-1,
  -1,-1,
  -1,-1,
  -1,-1,
  0.,
  1,
};

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
        unsigned int flags, unsigned int outfmt)
{
    vf->priv->exp_x = vf->priv->cfg_exp_x;
    vf->priv->exp_y = vf->priv->cfg_exp_y;
    vf->priv->exp_w = vf->priv->cfg_exp_w;
    vf->priv->exp_h = vf->priv->cfg_exp_h;
    if ( vf->priv->exp_w == -1 ) vf->priv->exp_w=width;
      else if (vf->priv->exp_w < -1 ) vf->priv->exp_w=width - vf->priv->exp_w;
        else if ( vf->priv->exp_w<width ) vf->priv->exp_w=width;
    if ( vf->priv->exp_h == -1 ) vf->priv->exp_h=height;
      else if ( vf->priv->exp_h < -1 ) vf->priv->exp_h=height - vf->priv->exp_h;
        else if( vf->priv->exp_h<height ) vf->priv->exp_h=height;
    if (vf->priv->aspect) {
        float adjusted_aspect = vf->priv->aspect;
        adjusted_aspect *= ((double)width/height) / ((double)d_width/d_height);
        if (vf->priv->exp_h < vf->priv->exp_w / adjusted_aspect) {
            vf->priv->exp_h = vf->priv->exp_w / adjusted_aspect + 0.5;
        } else {
            vf->priv->exp_w = vf->priv->exp_h * adjusted_aspect + 0.5;
        }
    }
    if (vf->priv->round > 1) { // round up.
        vf->priv->exp_w = (1 + (vf->priv->exp_w - 1) / vf->priv->round) * vf->priv->round;
        vf->priv->exp_h = (1 + (vf->priv->exp_h - 1) / vf->priv->round) * vf->priv->round;
    }

    if(vf->priv->exp_x<0 || vf->priv->exp_x+width>vf->priv->exp_w) vf->priv->exp_x=(vf->priv->exp_w-width)/2;
    if(vf->priv->exp_y<0 || vf->priv->exp_y+height>vf->priv->exp_h) vf->priv->exp_y=(vf->priv->exp_h-height)/2;

    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(outfmt);

    vf->priv->exp_x = MP_ALIGN_DOWN(vf->priv->exp_x, fmt.align_x);
    vf->priv->exp_y = MP_ALIGN_DOWN(vf->priv->exp_y, fmt.align_y);

    vf_rescale_dsize(&d_width, &d_height, width, height,
                     vf->priv->exp_w, vf->priv->exp_h);

    return vf_next_config(vf,vf->priv->exp_w,vf->priv->exp_h,d_width,d_height,flags,outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    int e_x = vf->priv->exp_x, e_y = vf->priv->exp_y;
    int e_w = vf->priv->exp_w, e_h = vf->priv->exp_h;

    if (e_x == 0 && e_y == 0 && e_w == mpi->w && e_h == mpi->h)
        return mpi;

    struct mp_image *dmpi = vf_alloc_out_image(vf);
    if (!dmpi) {
        talloc_free(mpi);
        return NULL;
    }
    mp_image_copy_attributes(dmpi, mpi);

    struct mp_image cropped = *dmpi;
    mp_image_crop(&cropped, e_x, e_y, e_x + mpi->w, e_y + mpi->h);
    mp_image_copy(&cropped, mpi);

    int e_x2 = e_x + MP_ALIGN_DOWN(mpi->w, mpi->fmt.align_x);
    int e_y2 = e_y + MP_ALIGN_DOWN(mpi->h, mpi->fmt.align_y);

    // top border (over the full width)
    mp_image_clear(dmpi, 0, 0, e_w, e_y);
    // bottom border (over the full width)
    mp_image_clear(dmpi, 0, e_y2, e_w, e_h);
    // left
    mp_image_clear(dmpi, 0, e_y, e_x, e_y2);
    // right
    mp_image_clear(dmpi, e_x2, e_y, e_w, e_y2);

    talloc_free(mpi);
    return dmpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (!IMGFMT_IS_HWACCEL(fmt))
        return vf_next_query_format(vf, fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf){
    vf->config=config;
    vf->query_format=query_format;
    vf->filter=filter;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_INT("w", cfg_exp_w, 0),
    OPT_INT("h", cfg_exp_h, 0),
    OPT_INT("x", cfg_exp_x, M_OPT_MIN, .min = -1),
    OPT_INT("y", cfg_exp_y, M_OPT_MIN, .min = -1),
    OPT_DOUBLE("aspect", aspect, M_OPT_MIN, .min = 0),
    OPT_INT("round", round, M_OPT_MIN, .min = 1),
    {0}
};

const vf_info_t vf_info_expand = {
    .description = "expanding",
    .name = "expand",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_dflt,
    .options = vf_opts_fields,
};

//===========================================================================//
