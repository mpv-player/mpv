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

    int harddup;

    double lastpts;
    int64_t lastipts;
    int64_t lastframeipts;
    int64_t lastencodedipts;
    int64_t mindeltapts;
    double expected_next_pts;
    mp_image_t *lastimg;
    int lastdisplaycount;

    double last_video_in_pts;

    AVRational worst_time_base;

    bool shutdown;
};

static void draw_image(struct vo *vo, mp_image_t *mpi);

static int preinit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    vc->enc = encoder_context_alloc(vo->encode_lavc_ctx, STREAM_VIDEO, vo->log);
    if (!vc->enc)
        return -1;
    talloc_steal(vc, vc->enc);
    vc->harddup = vc->enc->options->harddup;
    vc->last_video_in_pts = MP_NOPTS_VALUE;
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *vc = vo->priv;

    if (vc->lastipts >= 0 && !vc->shutdown)
        draw_image(vo, NULL);

    mp_image_unrefp(&vc->lastimg);
}

static int reconfig2(struct vo *vo, struct mp_image *img)
{
    struct priv *vc = vo->priv;
    struct encode_lavc_context *ctx = vo->encode_lavc_ctx;
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

    vc->lastipts = AV_NOPTS_VALUE;
    vc->lastframeipts = AV_NOPTS_VALUE;
    vc->lastencodedipts = AV_NOPTS_VALUE;

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

    if (ctx->options->fps > 0) {
        tb = av_d2q(ctx->options->fps, ctx->options->fps * 1001 + 2);
    } else if (ctx->options->autofps && img->nominal_fps > 0) {
        tb = av_d2q(img->nominal_fps, img->nominal_fps * 1001 + 2);
        MP_INFO(vo, "option --ofps not specified "
                "but --oautofps is active, using guess of %u/%u\n",
                (unsigned)tb.num, (unsigned)tb.den);
    } else {
        // we want to handle:
        //      1/25
        //   1001/24000
        //   1001/30000
        // for this we would need 120000fps...
        // however, mpeg-4 only allows 16bit values
        // so let's take 1001/30000 out
        tb.num = 24000;
        tb.den = 1;
        MP_INFO(vo, "option --ofps not specified "
                "and fps could not be inferred, using guess of %u/%u\n",
                (unsigned)tb.num, (unsigned)tb.den);
    }

    const AVRational *rates = encoder->codec->supported_framerates;
    if (rates && rates[0].den)
        tb = rates[av_find_nearest_q_idx(tb, rates)];

    encoder->time_base = av_inv_q(tb);

    if (!encoder_init_codec_and_muxer(vc->enc))
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

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *vc = vo->priv;
    struct encoder_context *enc = vc->enc;
    struct encode_lavc_context *ectx = enc->encode_lavc_ctx;
    AVCodecContext *avc = enc->encoder;
    int64_t frameipts;
    double nextpts;

    double pts = mpi ? mpi->pts : MP_NOPTS_VALUE;

    if (mpi) {
        assert(vo->params);

        struct mp_osd_res dim = osd_res_from_image_params(vo->params);

        osd_draw_on_image(vo->osd, dim, mpi->pts, OSD_DRAW_SUB_ONLY, mpi);
    }

    if (vc->shutdown)
        goto done;

    if (pts == MP_NOPTS_VALUE) {
        if (mpi)
            MP_WARN(vo, "frame without pts, please report; synthesizing pts instead\n");
        pts = vc->expected_next_pts;
    }

    if (vc->worst_time_base.den == 0) {
        // We don't know the muxer time_base anymore, and can't, because we
        // might start encoding before the muxer is opened. (The muxer decides
        // the final AVStream.time_base when opening the muxer.)
        vc->worst_time_base = avc->time_base;

        if (enc->options->maxfps) {
            vc->mindeltapts = ceil(vc->worst_time_base.den /
                    (vc->worst_time_base.num * enc->options->maxfps));
        } else {
            vc->mindeltapts = 0;
        }

        // NOTE: we use the following "axiom" of av_rescale_q:
        // if time base A is worse than time base B, then
        //   av_rescale_q(av_rescale_q(x, A, B), B, A) == x
        // this can be proven as long as av_rescale_q rounds to nearest, which
        // it currently does

        // av_rescale_q(x, A, B) * B = "round x*A to nearest multiple of B"
        // and:
        //    av_rescale_q(av_rescale_q(x, A, B), B, A) * A
        // == "round av_rescale_q(x, A, B)*B to nearest multiple of A"
        // == "round 'round x*A to nearest multiple of B' to nearest multiple of A"
        //
        // assume this fails. Then there is a value of x*A, for which the
        // nearest multiple of B is outside the range [(x-0.5)*A, (x+0.5)*A[.
        // Absurd, as this range MUST contain at least one multiple of B.
    }

    double timeunit = (double)vc->worst_time_base.num / vc->worst_time_base.den;

    // Lock for shared timestamp fields.
    pthread_mutex_lock(&ectx->lock);

    double outpts;
    if (enc->options->rawts) {
        outpts = pts;
    } else if (enc->options->copyts) {
        // fix the discontinuity pts offset
        nextpts = pts;
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        } else if (fabs(nextpts + ectx->discontinuity_pts_offset -
                        ectx->next_in_pts) > 30)
        {
            MP_WARN(vo, "detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    } else {
        // adjust pts by knowledge of audio pts vs audio playback time
        double duration = 0;
        if (vc->last_video_in_pts != MP_NOPTS_VALUE)
            duration = pts - vc->last_video_in_pts;
        if (duration < 0)
            duration = timeunit;   // XXX warn about discontinuity?
        outpts = vc->lastpts + duration;
        if (ectx->audio_pts_offset != MP_NOPTS_VALUE) {
            double adj = outpts - pts - ectx->audio_pts_offset;
            adj = FFMIN(adj, duration * 0.1);
            adj = FFMAX(adj, -duration * 0.1);
            outpts -= adj;
        }
    }
    vc->lastpts = outpts;
    vc->last_video_in_pts = pts;
    frameipts = floor((outpts + encoder_get_offset(enc)) / timeunit + 0.5);

    // calculate expected pts of next video frame
    vc->expected_next_pts = pts + timeunit;

    if (!enc->options->rawts && enc->options->copyts) {
        // set next allowed output pts value
        nextpts = vc->expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    pthread_mutex_unlock(&ectx->lock);

    // never-drop mode
    if (enc->options->neverdrop) {
        int64_t step = vc->mindeltapts ? vc->mindeltapts : 1;
        if (frameipts < vc->lastipts + step) {
            MP_INFO(vo, "--oneverdrop increased pts by %d\n",
                    (int) (vc->lastipts - frameipts + step));
            frameipts = vc->lastipts + step;
            vc->lastpts = frameipts * timeunit - encoder_get_offset(enc);
        }
    }

    if (vc->lastipts != AV_NOPTS_VALUE) {
        // we have a valid image in lastimg
        while (vc->lastimg && vc->lastipts < frameipts) {
            int64_t thisduration = vc->harddup ? 1 : (frameipts - vc->lastipts);

            // we will ONLY encode this frame if it can be encoded at at least
            // vc->mindeltapts after the last encoded frame!
            int64_t skipframes = (vc->lastencodedipts == AV_NOPTS_VALUE)
                    ? 0 : vc->lastencodedipts + vc->mindeltapts - vc->lastipts;
            if (skipframes < 0)
                skipframes = 0;

            if (thisduration > skipframes) {
                AVFrame *frame = mp_image_to_av_frame(vc->lastimg);
                if (!frame)
                    abort();

                // this is a nop, unless the worst time base is the STREAM time base
                frame->pts = av_rescale_q(vc->lastipts + skipframes,
                                          vc->worst_time_base, avc->time_base);
                frame->pict_type = 0; // keep this at unknown/undefined
                frame->quality = avc->global_quality;
                encoder_encode(enc, frame);
                av_frame_free(&frame);

                vc->lastdisplaycount += 1;
                vc->lastencodedipts = vc->lastipts + skipframes;
            }

            vc->lastipts += thisduration;
        }
    }

    if (!mpi) {
        // finish encoding
        encoder_encode(enc, NULL);
    } else {
        if (frameipts >= vc->lastframeipts) {
            if (vc->lastframeipts != AV_NOPTS_VALUE && vc->lastdisplaycount != 1)
                MP_INFO(vo, "Frame at pts %d got displayed %d times\n",
                        (int) vc->lastframeipts, vc->lastdisplaycount);
            talloc_free(vc->lastimg);
            vc->lastimg = mpi;
            mpi = NULL;

            vc->lastframeipts = vc->lastipts = frameipts;
            if (enc->options->rawts && vc->lastipts < 0) {
                MP_ERR(vo, "why does this happen? DEBUG THIS! vc->lastipts = %lld\n",
                       (long long) vc->lastipts);
                vc->lastipts = -1;
            }
            vc->lastdisplaycount = 0;
        } else {
            MP_INFO(vo, "Frame at pts %d got dropped "
                    "entirely because pts went backwards\n", (int) frameipts);
        }
    }

done:
    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *vc = vo->priv;

    switch (request) {
    case VOCTRL_RESET:
        vc->last_video_in_pts = MP_NOPTS_VALUE;
        break;
    }

    return VO_NOTIMPL;
}

const struct vo_driver video_out_lavc = {
    .encode = true,
    .description = "video encoding using libavcodec",
    .name = "lavc",
    .untimed = true,
    .priv_size = sizeof(struct priv),
    .preinit = preinit,
    .query_format = query_format,
    .reconfig2 = reconfig2,
    .control = control,
    .uninit = uninit,
    .draw_image = draw_image,
    .flip_page = flip_page,
};

// vim: sw=4 ts=4 et tw=80
