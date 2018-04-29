/*
 * video encoding using libavformat
 *
 * Copyright (C) 2010 Nicolas George <george@nsup.org>
 * Copyright (C) 2011-2012 Rudolf Polzer <divVerent@xonotic.org>
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

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "common/common.h"
#include "options/options.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "mpv_talloc.h"
#include "vo.h"

#include "common/encode_lavc.h"

#include "sub/osd.h"

struct priv {
    struct encoder_context *enc;

    bool shutdown;
};

static int preinit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    vc->enc = encoder_context_alloc(vo->encode_lavc_ctx, STREAM_VIDEO, vo->log);
    if (!vc->enc)
        return -1;
    talloc_steal(vc, vc->enc);
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    struct encoder_context *enc = vc->enc;

    if (!vc->shutdown)
        encoder_encode(enc, NULL); // finish encoding
}

static void on_ready(void *ptr)
{
    struct vo *vo = ptr;

    vo_event(vo, VO_EVENT_INITIAL_UNBLOCK);
}

static int reconfig2(struct vo *vo, struct mp_image *img)
{
    struct priv *vc = vo->priv;
    AVCodecContext *encoder = vc->enc->encoder;

    struct mp_image_params *params = &img->params;
    enum AVPixelFormat pix_fmt = imgfmt2pixfmt(params->imgfmt);
    AVRational aspect = {params->p_w, params->p_h};
    int width = params->w;
    int height = params->h;

    if (vc->shutdown)
        return -1;

    if (avcodec_is_open(encoder)) {
        if (width == encoder->width && height == encoder->height &&
            pix_fmt == encoder->pix_fmt)
        {
            // consider these changes not critical
            MP_ERR(vo, "Ignoring mid-stream parameter changes!\n");
            return 0;
        }

        /* FIXME Is it possible with raw video? */
        MP_ERR(vo, "resolution changes not supported.\n");
        goto error;
    }

    // When we get here, this must be the first call to reconfigure(). Thus, we
    // can rely on no existing data in vc having been allocated yet.
    // Reason:
    // - Second calls after reconfigure() already failed once fail (due to the
    //   vc->shutdown check above).
    // - Second calls after reconfigure() already succeeded once return early
    //   (due to the avcodec_is_open() check above).

    if (pix_fmt == AV_PIX_FMT_NONE) {
        MP_FATAL(vo, "Format %s not supported by lavc.\n",
                 mp_imgfmt_to_name(params->imgfmt));
        goto error;
    }

    encoder->sample_aspect_ratio = aspect;
    encoder->width = width;
    encoder->height = height;
    encoder->pix_fmt = pix_fmt;
    encoder->colorspace = mp_csp_to_avcol_spc(params->color.space);
    encoder->color_range = mp_csp_levels_to_avcol_range(params->color.levels);

    AVRational tb;

    // we want to handle:
    //      1/25
    //   1001/24000
    //   1001/30000
    // for this we would need 120000fps...
    // however, mpeg-4 only allows 16bit values
    // so let's take 1001/30000 out
    tb.num = 24000;
    tb.den = 1;

    const AVRational *rates = encoder->codec->supported_framerates;
    if (rates && rates[0].den)
        tb = rates[av_find_nearest_q_idx(tb, rates)];

    encoder->time_base = av_inv_q(tb);

    if (!encoder_init_codec_and_muxer(vc->enc, on_ready, vo))
        goto error;

    return 0;

error:
    vc->shutdown = true;
    return -1;
}

static int query_format(struct vo *vo, int format)
{
    struct priv *vc = vo->priv;

    enum AVPixelFormat pix_fmt = imgfmt2pixfmt(format);
    const enum AVPixelFormat *p = vc->enc->encoder->codec->pix_fmts;

    if (!p)
        return 1;

    while (*p != AV_PIX_FMT_NONE) {
        if (*p == pix_fmt)
            return 1;
        p++;
    }

    return 0;
}

static void draw_frame(struct vo *vo, struct vo_frame *voframe)
{
    struct priv *vc = vo->priv;
    struct encoder_context *enc = vc->enc;
    struct encode_lavc_context *ectx = enc->encode_lavc_ctx;
    AVCodecContext *avc = enc->encoder;

    if (voframe->redraw || voframe->repeat || voframe->num_frames < 1)
        return;

    struct mp_image *mpi = voframe->frames[0];

    struct mp_osd_res dim = osd_res_from_image_params(vo->params);
    osd_draw_on_image(vo->osd, dim, mpi->pts, OSD_DRAW_SUB_ONLY, mpi);

    if (vc->shutdown)
        return;

    // Lock for shared timestamp fields.
    pthread_mutex_lock(&ectx->lock);

    double pts = mpi->pts;
    double outpts = pts;
    if (!enc->options->rawts) {
        // fix the discontinuity pts offset
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - pts;
        } else if (fabs(pts + ectx->discontinuity_pts_offset -
                        ectx->next_in_pts) > 30)
        {
            MP_WARN(vo, "detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    pts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - pts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    }

    outpts += encoder_get_offset(enc);

    if (!enc->options->rawts) {
        // calculate expected pts of next video frame
        double timeunit = av_q2d(avc->time_base);
        double expected_next_pts = pts + timeunit;
        // set next allowed output pts value
        double nextpts = expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    pthread_mutex_unlock(&ectx->lock);

    AVFrame *frame = mp_image_to_av_frame(mpi);
    if (!frame)
        abort();

    frame->pts = rint(outpts * av_q2d(av_inv_q(avc->time_base)));
    frame->pict_type = 0; // keep this at unknown/undefined
    frame->quality = avc->global_quality;
    encoder_encode(enc, frame);
    av_frame_free(&frame);
}

static void flip_page(struct vo *vo)
{
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_lavc = {
    .encode = true,
    .description = "video encoding using libavcodec",
    .name = "lavc",
    .initially_blocked = true,
    .untimed = true,
    .priv_size = sizeof(struct priv),
    .preinit = preinit,
    .query_format = query_format,
    .reconfig2 = reconfig2,
    .control = control,
    .uninit = uninit,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
};

// vim: sw=4 ts=4 et tw=80
