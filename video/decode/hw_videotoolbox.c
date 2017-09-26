/*
 * This file is part of mpv.
 *
 * Copyright (c) 2015 Sebastien Zwickert
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

#include <libavutil/hwcontext.h>

#include "video/decode/lavc.h"

static void vt_dummy_destroy(struct mp_hwdec_ctx *ctx)
{
    av_buffer_unref(&ctx->av_device_ref);
    talloc_free(ctx);
}

static struct mp_hwdec_ctx *vt_create_dummy(struct mpv_global *global,
                                            struct mp_log *plog, bool probing)
{
    struct mp_hwdec_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mp_hwdec_ctx) {
        .type = HWDEC_VIDEOTOOLBOX_COPY,
        .ctx = "dummy",
        .destroy = vt_dummy_destroy,
    };

    if (av_hwdevice_ctx_create(&ctx->av_device_ref, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                               NULL, NULL, 0) < 0)
    {
        vt_dummy_destroy(ctx);
        return NULL;
    }

    return ctx;
}

const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox = {
    .type = HWDEC_VIDEOTOOLBOX,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .generic_hwaccel = true,
    .set_hwframes = true,
    .pixfmt_map = (const enum AVPixelFormat[][2]) {
        {AV_PIX_FMT_NONE}
    },
};

const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox_copy = {
    .type = HWDEC_VIDEOTOOLBOX_COPY,
    .copying = true,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .generic_hwaccel = true,
    .create_dev = vt_create_dummy,
    .set_hwframes = true,
    .pixfmt_map = (const enum AVPixelFormat[][2]) {
        {AV_PIX_FMT_NONE}
    },
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
