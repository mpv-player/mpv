/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <libavutil/common.h>

#include "config.h"
#include "common/msg.h"
#include "options/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "sub/osd.h"
#include "sub/dec_sub.h"

#include "video/sws_utils.h"

#include "options/m_option.h"

struct vf_priv_s {
    int opt_top_margin, opt_bottom_margin;

    int outh, outw;

    struct osd_state *osd;
    struct mp_osd_res dim;
};

static int config(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt)
{
    vf->priv->outh = height + vf->priv->opt_top_margin +
                     vf->priv->opt_bottom_margin;
    vf->priv->outw = width;

    double dar = (double)d_width / d_height;
    double sar = (double)width / height;

    vf_rescale_dsize(&d_width, &d_height, width, height,
                     vf->priv->outw, vf->priv->outh);

    vf->priv->dim = (struct mp_osd_res) {
        .w = vf->priv->outw,
        .h = vf->priv->outh,
        .mt = vf->priv->opt_top_margin,
        .mb = vf->priv->opt_bottom_margin,
        .display_par = sar / dar,
    };

    return vf_next_config(vf, vf->priv->outw, vf->priv->outh, d_width,
                          d_height, flags, outfmt);
}

static void prepare_image(struct vf_instance *vf, struct mp_image *dmpi,
                          struct mp_image *mpi)
{
    int y1 = MP_ALIGN_DOWN(vf->priv->opt_top_margin, mpi->fmt.align_y);
    int y2 = MP_ALIGN_DOWN(y1 + mpi->h, mpi->fmt.align_y);
    struct mp_image cropped = *dmpi;
    mp_image_crop(&cropped, 0, y1, mpi->w, y1 + mpi->h);
    mp_image_copy(&cropped, mpi);
    mp_image_clear(dmpi, 0, 0, dmpi->w, y1);
    mp_image_clear(dmpi, 0, y2, dmpi->w, vf->priv->outh);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *priv = vf->priv;
    struct osd_state *osd = priv->osd;

    if (vf->priv->opt_top_margin || vf->priv->opt_bottom_margin) {
        struct mp_image *dmpi = vf_alloc_out_image(vf);
        if (!dmpi)
            return NULL;
        mp_image_copy_attributes(dmpi, mpi);
        prepare_image(vf, dmpi, mpi);
        talloc_free(mpi);
        mpi = dmpi;
    }

    if (!osd)
        return mpi;

    osd_draw_on_image_p(osd, priv->dim, mpi->pts, OSD_DRAW_SUB_FILTER,
                        vf->out_pool, mpi);

    return mpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (!mp_sws_supported_format(fmt))
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int control(vf_instance_t *vf, int request, void *data)
{
    switch (request) {
    case VFCTRL_SET_OSD_OBJ:
        vf->priv->osd = data;
        return CONTROL_TRUE;
    case VFCTRL_INIT_OSD:
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static int vf_open(vf_instance_t *vf)
{
    vf->config = config;
    vf->query_format = query_format;
    vf->control   = control;
    vf->filter    = filter;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_INTRANGE("bottom-margin", opt_bottom_margin, 0, 0, 2000),
    OPT_INTRANGE("top-margin", opt_top_margin, 0, 0, 2000),
    {0}
};

const vf_info_t vf_info_sub = {
    .description = "Render subtitles",
    .name = "sub",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};
