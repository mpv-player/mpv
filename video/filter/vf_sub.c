/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <libavutil/common.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"

#include "video/sws_utils.h"
#include "video/memcpy_pic.h"
#include "video/csputils.h"

#include "core/m_option.h"
#include "core/m_struct.h"

static const struct vf_priv_s {
    int opt_top_margin, opt_bottom_margin;

    int outh, outw;

    struct mp_csp_details csp;

    struct osd_state *osd;
    struct mp_osd_res dim;
} vf_priv_dflt = {
    .csp = MP_CSP_DETAILS_DEFAULTS,
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
        .video_par = dar / sar,
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
        mp_image_copy_attributes(dmpi, mpi);
        prepare_image(vf, dmpi, mpi);
        talloc_free(mpi);
        mpi = dmpi;
    }

    mp_image_set_colorspace_details(mpi, &priv->csp);

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
        break;
    case VFCTRL_INIT_OSD:
        return CONTROL_TRUE;
    case VFCTRL_SET_YUV_COLORSPACE: {
        struct mp_csp_details colorspace = *(struct mp_csp_details *)data;
        vf->priv->csp = colorspace;
        break;
    }
    }
    return vf_next_control(vf, request, data);
}

static int vf_open(vf_instance_t *vf, char *args)
{
    vf->config = config;
    vf->query_format = query_format;
    vf->control   = control;
    vf->filter    = filter;
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s, f)
static const m_option_t vf_opts_fields[] = {
    {"bottom-margin", ST_OFF(opt_bottom_margin),
     CONF_TYPE_INT, M_OPT_RANGE, 0, 2000},
    {"top-margin", ST_OFF(opt_top_margin),
     CONF_TYPE_INT, M_OPT_RANGE, 0, 2000},
    {0},
};

static const m_struct_t vf_opts = {
    "sub",
    sizeof(struct vf_priv_s),
    &vf_priv_dflt,
    vf_opts_fields
};

const vf_info_t vf_info_sub = {
    "Render subtitles",
    "sub",
    "Evgeniy Stepanov",
    "",
    vf_open,
    &vf_opts
};
