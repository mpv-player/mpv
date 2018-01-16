/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
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
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/options.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "sub/osd.h"
#include "sub/dec_sub.h"

#include "video/sws_utils.h"

#include "options/m_option.h"

struct vf_sub_opts {
    int top_margin, bottom_margin;
};

struct priv {
    struct vf_sub_opts *opts;
    struct mp_image_pool *pool;
};

static void vf_sub_process(struct mp_filter *f)
{
    struct priv *priv = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (mp_frame_is_signaling(frame)) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    struct mp_stream_info *info = mp_filter_find_stream_info(f);
    struct osd_state *osd = info ? info->osd : NULL;

    if (!osd)
        goto error;

    osd_set_render_subs_in_filter(osd, true);

    if (frame.type != MP_FRAME_VIDEO)
        goto error;

    struct mp_image *mpi = frame.data;

    if (!mp_sws_supported_format(mpi->imgfmt))
        goto error;

    struct mp_osd_res dim = {
        .w = mpi->w,
        .h = mpi->h + priv->opts->top_margin + priv->opts->bottom_margin,
        .mt = priv->opts->top_margin,
        .mb = priv->opts->bottom_margin,
        .display_par = mpi->params.p_w / (double)mpi->params.p_h,
    };

    if (dim.w != mpi->w || dim.h != mpi->h) {
        struct mp_image *dmpi =
            mp_image_pool_get(priv->pool, mpi->imgfmt, dim.w, dim.h);
        if (!dmpi)
            goto error;
        mp_image_copy_attributes(dmpi, mpi);
        int y1 = MP_ALIGN_DOWN(priv->opts->top_margin, mpi->fmt.align_y);
        int y2 = MP_ALIGN_DOWN(y1 + mpi->h, mpi->fmt.align_y);
        struct mp_image cropped = *dmpi;
        mp_image_crop(&cropped, 0, y1, mpi->w, y1 + mpi->h);
        mp_image_copy(&cropped, mpi);
        mp_image_clear(dmpi, 0, 0, dmpi->w, y1);
        mp_image_clear(dmpi, 0, y2, dmpi->w, dim.h);
        mp_frame_unref(&frame);
        mpi = dmpi;
        frame = (struct mp_frame){MP_FRAME_VIDEO, mpi};
    }

    osd_draw_on_image_p(osd, dim, mpi->pts, OSD_DRAW_SUB_FILTER, priv->pool, mpi);

    mp_pin_in_write(f->ppins[1], frame);
    return;

error:
    MP_ERR(f, "unsupported format, missing OSD, or failed allocation\n");
    mp_frame_unref(&frame);
    mp_filter_internal_mark_failed(f);
}

static const struct mp_filter_info vf_sub_filter = {
    .name = "sub",
    .process = vf_sub_process,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_sub_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_sub_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    MP_WARN(f, "This filter is deprecated and will be removed (no replacement)\n");

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *priv = f->priv;
    priv->opts = talloc_steal(priv, options);
    priv->pool = mp_image_pool_new(priv);

    return f;
}

#define OPT_BASE_STRUCT struct vf_sub_opts
static const m_option_t vf_opts_fields[] = {
    OPT_INTRANGE("bottom-margin", bottom_margin, 0, 0, 2000),
    OPT_INTRANGE("top-margin", top_margin, 0, 0, 2000),
    {0}
};

const struct mp_user_filter_entry vf_sub = {
    .desc = {
        .description = "Render subtitles",
        .name = "sub",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = vf_opts_fields,
    },
    .create = vf_sub_create,
};
