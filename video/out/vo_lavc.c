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
    AVStream *stream;
    AVCodecContext *codec;
    int have_first_packet;

    int harddup;

    double lastpts;
    int64_t lastipts;
    int64_t lastframeipts;
    int64_t lastencodedipts;
    int64_t mindeltapts;
    double expected_next_pts;
    mp_image_t *lastimg;
    int lastdisplaycount;

    AVRational worst_time_base;
    int worst_time_base_is_stream;

    bool shutdown;
};

static int preinit(struct vo *vo)
{
    struct priv *vc;
    if (!encode_lavc_available(vo->encode_lavc_ctx)) {
        MP_ERR(vo, "the option --o (output file) must be specified\n");
        return -1;
    }
    vo->priv = talloc_zero(vo, struct priv);
    vc = vo->priv;
    vc->harddup = vo->encode_lavc_ctx->options->harddup;
    return 0;
}

static void draw_image_unlocked(struct vo *vo, mp_image_t *mpi);
static void uninit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    if (!vc || vc->shutdown)
        return;

    pthread_mutex_lock(&vo->encode_lavc_ctx->lock);

    if (vc->lastipts >= 0 && vc->stream)
        draw_image_unlocked(vo, NULL);

    mp_image_unrefp(&vc->lastimg);

    pthread_mutex_unlock(&vo->encode_lavc_ctx->lock);

    vc->shutdown = true;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *vc = vo->priv;
    enum AVPixelFormat pix_fmt = imgfmt2pixfmt(params->imgfmt);
    AVRational aspect = {params->p_w, params->p_h};
    uint32_t width = params->w;
    uint32_t height = params->h;

    if (!vc || vc->shutdown)
        return -1;

    pthread_mutex_lock(&vo->encode_lavc_ctx->lock);

    if (vc->stream) {
        /* NOTE:
         * in debug builds we get a "comparison between signed and unsigned"
         * warning here. We choose to ignore that; just because ffmpeg currently
         * uses a plain 'int' for these struct fields, it doesn't mean it always
         * will */
        if (width == vc->codec->width &&
                height == vc->codec->height) {
            if (aspect.num != vc->codec->sample_aspect_ratio.num ||
                    aspect.den != vc->codec->sample_aspect_ratio.den) {
                /* aspect-only changes are not critical */
                MP_WARN(vo, "unsupported pixel aspect ratio change from %d:%d to %d:%d\n",
                       vc->codec->sample_aspect_ratio.num,
                       vc->codec->sample_aspect_ratio.den,
                       aspect.num, aspect.den);
            }
            goto done;
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
    //   (due to the vc->stream check above).

    vc->lastipts = AV_NOPTS_VALUE;
    vc->lastframeipts = AV_NOPTS_VALUE;
    vc->lastencodedipts = AV_NOPTS_VALUE;

    if (pix_fmt == AV_PIX_FMT_NONE) {
        MP_FATAL(vo, "Format %s not supported by lavc.\n",
                 mp_imgfmt_to_name(params->imgfmt));
        goto error;
    }

    if (encode_lavc_alloc_stream(vo->encode_lavc_ctx,
                                 AVMEDIA_TYPE_VIDEO,
                                 &vc->stream, &vc->codec) < 0)
        goto error;
    vc->stream->sample_aspect_ratio = vc->codec->sample_aspect_ratio =
            aspect;
    vc->codec->width = width;
    vc->codec->height = height;
    vc->codec->pix_fmt = pix_fmt;

    encode_lavc_set_csp(vo->encode_lavc_ctx, vc->codec, params->color.space);
    encode_lavc_set_csp_levels(vo->encode_lavc_ctx, vc->codec, params->color.levels);

    if (encode_lavc_open_codec(vo->encode_lavc_ctx, vc->codec) < 0)
        goto error;

done:
    pthread_mutex_unlock(&vo->encode_lavc_ctx->lock);
    return 0;

error:
    pthread_mutex_unlock(&vo->encode_lavc_ctx->lock);
    vc->shutdown = true;
    return -1;
}

static int query_format(struct vo *vo, int format)
{
    enum AVPixelFormat pix_fmt = imgfmt2pixfmt(format);

    if (!vo->encode_lavc_ctx)
        return 0;

    pthread_mutex_lock(&vo->encode_lavc_ctx->lock);
    int flags = 0;
    if (encode_lavc_supports_pixfmt(vo->encode_lavc_ctx, pix_fmt))
        flags = 1;
    pthread_mutex_unlock(&vo->encode_lavc_ctx->lock);
    return flags;
}

static void write_packet(struct vo *vo, AVPacket *packet)
{
    struct priv *vc = vo->priv;

    packet->stream_index = vc->stream->index;
    if (packet->pts != AV_NOPTS_VALUE) {
        packet->pts = av_rescale_q(packet->pts,
                                   vc->codec->time_base,
                                   vc->stream->time_base);
    } else {
        MP_VERBOSE(vo, "codec did not provide pts\n");
        packet->pts = av_rescale_q(vc->lastipts,
                                   vc->worst_time_base,
                                   vc->stream->time_base);
    }
    if (packet->dts != AV_NOPTS_VALUE) {
        packet->dts = av_rescale_q(packet->dts,
                                   vc->codec->time_base,
                                   vc->stream->time_base);
    }
    if (packet->duration > 0) {
        packet->duration = av_rescale_q(packet->duration,
                                        vc->codec->time_base,
                                        vc->stream->time_base);
    } else {
        // HACK: libavformat calculates dts wrong if the initial packet
        // duration is not set, but ONLY if the time base is "high" and if we
        // have b-frames!
        if (!packet->duration)
            if (!vc->have_first_packet)
                if (vc->codec->has_b_frames
                        || vc->codec->max_b_frames)
                    if (vc->stream->time_base.num * 1000LL <=
                            vc->stream->time_base.den)
                        packet->duration = FFMAX(1, av_rescale_q(1,
                             vc->codec->time_base, vc->stream->time_base));
    }

    if (encode_lavc_write_frame(vo->encode_lavc_ctx,
                                vc->stream, packet) < 0) {
        MP_ERR(vo, "error writing at %d %d/%d\n",
               (int) packet->pts,
               vc->stream->time_base.num,
               vc->stream->time_base.den);
        return;
    }

    vc->have_first_packet = 1;
}

static void encode_video_and_write(struct vo *vo, AVFrame *frame)
{
    struct priv *vc = vo->priv;
    AVPacket packet = {0};

    int status = avcodec_send_frame(vc->codec, frame);
    if (status < 0) {
        MP_ERR(vo, "error encoding at %d %d/%d\n",
               frame ? (int) frame->pts : -1,
               vc->codec->time_base.num,
               vc->codec->time_base.den);
        return;
    }
    for (;;) {
        av_init_packet(&packet);
        status = avcodec_receive_packet(vc->codec, &packet);
        if (status == AVERROR(EAGAIN)) { // No more packets for now.
            if (frame == NULL) {
                MP_ERR(vo, "sent flush frame, got EAGAIN");
            }
            break;
        }
        if (status == AVERROR_EOF) { // No more packets, ever.
            if (frame != NULL) {
                MP_ERR(vo, "sent image frame, got EOF");
            }
            break;
        }
        if (status < 0) {
            MP_ERR(vo, "error encoding at %d %d/%d\n",
                   frame ? (int) frame->pts : -1,
                   vc->codec->time_base.num,
                   vc->codec->time_base.den);
            break;
        }
        encode_lavc_write_stats(vo->encode_lavc_ctx, vc->codec);
        write_packet(vo, &packet);
        av_packet_unref(&packet);
    }
}

static void draw_image_unlocked(struct vo *vo, mp_image_t *mpi)
{
    struct priv *vc = vo->priv;
    struct encode_lavc_context *ectx = vo->encode_lavc_ctx;
    AVCodecContext *avc;
    int64_t frameipts;
    double nextpts;

    double pts = mpi ? mpi->pts : MP_NOPTS_VALUE;

    if (mpi) {
        assert(vo->params);

        struct mp_osd_res dim = osd_res_from_image_params(vo->params);

        osd_draw_on_image(vo->osd, dim, mpi->pts, OSD_DRAW_SUB_ONLY, mpi);
    }

    if (!vc || vc->shutdown)
        goto done;
    if (!encode_lavc_start(ectx)) {
        MP_WARN(vo, "NOTE: skipped initial video frame (probably because audio is not there yet)\n");
        goto done;
    }
    if (pts == MP_NOPTS_VALUE) {
        if (mpi)
            MP_WARN(vo, "frame without pts, please report; synthesizing pts instead\n");
        pts = vc->expected_next_pts;
    }

    avc = vc->codec;

    if (vc->worst_time_base.den == 0) {
        //if (avc->time_base.num / avc->time_base.den >= vc->stream->time_base.num / vc->stream->time_base.den)
        if (avc->time_base.num * (double) vc->stream->time_base.den >=
                vc->stream->time_base.num * (double) avc->time_base.den) {
            MP_VERBOSE(vo, "NOTE: using codec time base "
                       "(%d/%d) for frame dropping; the stream base (%d/%d) is "
                       "not worse.\n", (int)avc->time_base.num,
                       (int)avc->time_base.den, (int)vc->stream->time_base.num,
                       (int)vc->stream->time_base.den);
            vc->worst_time_base = avc->time_base;
            vc->worst_time_base_is_stream = 0;
        } else {
            MP_WARN(vo, "NOTE: not using codec time base (%d/%d) for frame "
                    "dropping; the stream base (%d/%d) is worse.\n",
                    (int)avc->time_base.num, (int)avc->time_base.den,
                    (int)vc->stream->time_base.num, (int)vc->stream->time_base.den);
            vc->worst_time_base = vc->stream->time_base;
            vc->worst_time_base_is_stream = 1;
        }
        if (ectx->options->maxfps)
            vc->mindeltapts = ceil(vc->worst_time_base.den /
                    (vc->worst_time_base.num * ectx->options->maxfps));
        else
            vc->mindeltapts = 0;

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

    double outpts;
    if (ectx->options->rawts)
        outpts = pts;
    else if (ectx->options->copyts) {
        // fix the discontinuity pts offset
        nextpts = pts;
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }
        else if (fabs(nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts) > 30) {
            MP_WARN(vo, "detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    }
    else {
        // adjust pts by knowledge of audio pts vs audio playback time
        double duration = 0;
        if (ectx->last_video_in_pts != MP_NOPTS_VALUE)
            duration = pts - ectx->last_video_in_pts;
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
    ectx->last_video_in_pts = pts;
    frameipts = floor((outpts + encode_lavc_getoffset(ectx, vc->codec))
                      / timeunit + 0.5);

    // calculate expected pts of next video frame
    vc->expected_next_pts = pts + timeunit;

    if (!ectx->options->rawts && ectx->options->copyts) {
        // set next allowed output pts value
        nextpts = vc->expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    // never-drop mode
    if (ectx->options->neverdrop) {
        int64_t step = vc->mindeltapts ? vc->mindeltapts : 1;
        if (frameipts < vc->lastipts + step) {
            MP_INFO(vo, "--oneverdrop increased pts by %d\n",
                    (int) (vc->lastipts - frameipts + step));
            frameipts = vc->lastipts + step;
            vc->lastpts = frameipts * timeunit - encode_lavc_getoffset(ectx, vc->codec);
        }
    }

    if (vc->lastipts != AV_NOPTS_VALUE) {

        // we have a valid image in lastimg
        while (vc->lastimg && vc->lastipts < frameipts) {
            int64_t thisduration = vc->harddup ? 1 : (frameipts - vc->lastipts);

            // we will ONLY encode this frame if it can be encoded at at least
            // vc->mindeltapts after the last encoded frame!
            int64_t skipframes =
                (vc->lastencodedipts == AV_NOPTS_VALUE)
                    ? 0
                    : vc->lastencodedipts + vc->mindeltapts - vc->lastipts;
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
                encode_video_and_write(vo, frame);
                av_frame_free(&frame);

                ++vc->lastdisplaycount;
                vc->lastencodedipts = vc->lastipts + skipframes;
            }

            vc->lastipts += thisduration;
        }
    }

    if (!mpi) {
        // finish encoding
        encode_video_and_write(vo, NULL);
    } else {
        if (frameipts >= vc->lastframeipts) {
            if (vc->lastframeipts != AV_NOPTS_VALUE && vc->lastdisplaycount != 1)
                MP_INFO(vo, "Frame at pts %d got displayed %d times\n",
                        (int) vc->lastframeipts, vc->lastdisplaycount);
            talloc_free(vc->lastimg);
            vc->lastimg = mpi;
            mpi = NULL;

            vc->lastframeipts = vc->lastipts = frameipts;
            if (ectx->options->rawts && vc->lastipts < 0) {
                MP_ERR(vo, "why does this happen? DEBUG THIS! vc->lastipts = %lld\n", (long long) vc->lastipts);
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

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    pthread_mutex_lock(&vo->encode_lavc_ctx->lock);
    draw_image_unlocked(vo, mpi);
    pthread_mutex_unlock(&vo->encode_lavc_ctx->lock);
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
    .untimed = true,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .uninit = uninit,
    .draw_image = draw_image,
    .flip_page = flip_page,
};

// vim: sw=4 ts=4 et tw=80
