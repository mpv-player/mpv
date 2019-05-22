/*
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
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>

#include "mpv_talloc.h"
#include "audio/aframe.h"
#include "audio/fmt-conversion.h"
#include "common/av_common.h"
#include "common/codecs.h"
#include "common/global.h"
#include "common/msg.h"
#include "demux/packet.h"
#include "demux/stheader.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/filter_internal.h"
#include "options/m_config.h"
#include "options/options.h"

struct priv {
    AVCodecContext *avctx;
    AVFrame *avframe;
    struct mp_chmap force_channel_map;
    uint32_t skip_samples, trim_samples;
    bool preroll_done;
    double next_pts;
    AVRational codec_timebase;
    bool eof_returned;

    struct mp_decoder public;
};

#define OPT_BASE_STRUCT struct ad_lavc_params
struct ad_lavc_params {
    float ac3drc;
    int downmix;
    int threads;
    char **avopts;
};

const struct m_sub_options ad_lavc_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLOATRANGE("ac3drc", ac3drc, 0, 0, 6),
        OPT_FLAG("downmix", downmix, 0),
        OPT_INTRANGE("threads", threads, 0, 0, 16),
        OPT_KEYVALUELIST("o", avopts, 0),
        {0}
    },
    .size = sizeof(struct ad_lavc_params),
    .defaults = &(const struct ad_lavc_params){
        .ac3drc = 0,
        .downmix = 1,
        .threads = 1,
    },
};

static bool init(struct mp_filter *da, struct mp_codec_params *codec,
                 const char *decoder)
{
    struct priv *ctx = da->priv;
    struct MPOpts *mpopts = mp_get_config_group(ctx, da->global, GLOBAL_CONFIG);
    struct ad_lavc_params *opts =
        mp_get_config_group(ctx, da->global, &ad_lavc_conf);
    AVCodecContext *lavc_context;
    AVCodec *lavc_codec;

    ctx->codec_timebase = mp_get_codec_timebase(codec);

    if (codec->force_channels)
        ctx->force_channel_map = codec->channels;

    lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec) {
        MP_ERR(da, "Cannot find codec '%s' in libavcodec...\n", decoder);
        return false;
    }

    lavc_context = avcodec_alloc_context3(lavc_codec);
    ctx->avctx = lavc_context;
    ctx->avframe = av_frame_alloc();
    lavc_context->codec_type = AVMEDIA_TYPE_AUDIO;
    lavc_context->codec_id = lavc_codec->id;

#if LIBAVCODEC_VERSION_MICRO >= 100
    lavc_context->pkt_timebase = ctx->codec_timebase;
#endif

    if (opts->downmix && mpopts->audio_output_channels.num_chmaps == 1) {
        lavc_context->request_channel_layout =
            mp_chmap_to_lavc(&mpopts->audio_output_channels.chmaps[0]);
    }

    // Always try to set - option only exists for AC3 at the moment
    av_opt_set_double(lavc_context, "drc_scale", opts->ac3drc,
                      AV_OPT_SEARCH_CHILDREN);

#if LIBAVCODEC_VERSION_MICRO >= 100
    // Let decoder add AV_FRAME_DATA_SKIP_SAMPLES.
    av_opt_set(lavc_context, "flags2", "+skip_manual", AV_OPT_SEARCH_CHILDREN);
#endif

    mp_set_avopts(da->log, lavc_context, opts->avopts);

    if (mp_set_avctx_codec_headers(lavc_context, codec) < 0) {
        MP_ERR(da, "Could not set decoder parameters.\n");
        return false;
    }

    mp_set_avcodec_threads(da->log, lavc_context, opts->threads);

    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, NULL) < 0) {
        MP_ERR(da, "Could not open codec.\n");
        return false;
    }

    ctx->next_pts = MP_NOPTS_VALUE;

    return true;
}

static void destroy(struct mp_filter *da)
{
    struct priv *ctx = da->priv;

    avcodec_free_context(&ctx->avctx);
    av_frame_free(&ctx->avframe);
}

static void reset(struct mp_filter *da)
{
    struct priv *ctx = da->priv;

    avcodec_flush_buffers(ctx->avctx);
    ctx->skip_samples = 0;
    ctx->trim_samples = 0;
    ctx->preroll_done = false;
    ctx->next_pts = MP_NOPTS_VALUE;
    ctx->eof_returned = false;
}

static bool send_packet(struct mp_filter *da, struct demux_packet *mpkt)
{
    struct priv *priv = da->priv;
    AVCodecContext *avctx = priv->avctx;

    // If the decoder discards the timestamp for some reason, we use the
    // interpolated PTS. Initialize it so that it works for the initial
    // packet as well.
    if (mpkt && priv->next_pts == MP_NOPTS_VALUE)
        priv->next_pts = mpkt->pts;

    AVPacket pkt;
    mp_set_av_packet(&pkt, mpkt, &priv->codec_timebase);

    int ret = avcodec_send_packet(avctx, mpkt ? &pkt : NULL);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return false;

    if (ret < 0)
        MP_ERR(da, "Error decoding audio.\n");
    return true;
}

static bool receive_frame(struct mp_filter *da, struct mp_frame *out)
{
    struct priv *priv = da->priv;
    AVCodecContext *avctx = priv->avctx;

    int ret = avcodec_receive_frame(avctx, priv->avframe);

    if (ret == AVERROR_EOF) {
        // If flushing was initialized earlier and has ended now, make it start
        // over in case we get new packets at some point in the future.
        // (Dont' reset the filter itself, we want to keep other state.)
        avcodec_flush_buffers(priv->avctx);
        return false;
    } else if (ret < 0 && ret != AVERROR(EAGAIN)) {
        MP_ERR(da, "Error decoding audio.\n");
    }

#if LIBAVCODEC_VERSION_MICRO >= 100
    if (priv->avframe->flags & AV_FRAME_FLAG_DISCARD)
        av_frame_unref(priv->avframe);
#endif

    if (!priv->avframe->buf[0])
        return true;

    double out_pts = mp_pts_from_av(priv->avframe->pts, &priv->codec_timebase);

    struct mp_aframe *mpframe = mp_aframe_from_avframe(priv->avframe);
    if (!mpframe)
        return true;

    if (priv->force_channel_map.num)
        mp_aframe_set_chmap(mpframe, &priv->force_channel_map);

    if (out_pts == MP_NOPTS_VALUE)
        out_pts = priv->next_pts;
    mp_aframe_set_pts(mpframe, out_pts);

    priv->next_pts = mp_aframe_end_pts(mpframe);

#if LIBAVCODEC_VERSION_MICRO >= 100
    AVFrameSideData *sd =
        av_frame_get_side_data(priv->avframe, AV_FRAME_DATA_SKIP_SAMPLES);
    if (sd && sd->size >= 10) {
        char *d = sd->data;
        priv->skip_samples += AV_RL32(d + 0);
        priv->trim_samples += AV_RL32(d + 4);
    }
#endif

    if (!priv->preroll_done) {
        // Skip only if this isn't already handled by AV_FRAME_DATA_SKIP_SAMPLES.
        if (!priv->skip_samples)
            priv->skip_samples = avctx->delay;
        priv->preroll_done = true;
    }

    uint32_t skip = MPMIN(priv->skip_samples, mp_aframe_get_size(mpframe));
    if (skip) {
        mp_aframe_skip_samples(mpframe, skip);
        priv->skip_samples -= skip;
    }
    uint32_t trim = MPMIN(priv->trim_samples, mp_aframe_get_size(mpframe));
    if (trim) {
        mp_aframe_set_size(mpframe, mp_aframe_get_size(mpframe) - trim);
        priv->trim_samples -= trim;
    }

    if (mp_aframe_get_size(mpframe) > 0)
        *out = MAKE_FRAME(MP_FRAME_AUDIO, mpframe);

    av_frame_unref(priv->avframe);

    return true;
}

static void process(struct mp_filter *ad)
{
    struct priv *priv = ad->priv;

    lavc_process(ad, &priv->eof_returned, send_packet, receive_frame);
}

static const struct mp_filter_info ad_lavc_filter = {
    .name = "ad_lavc",
    .priv_size = sizeof(struct priv),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

static struct mp_decoder *create(struct mp_filter *parent,
                                 struct mp_codec_params *codec,
                                 const char *decoder)
{
    struct mp_filter *da = mp_filter_create(parent, &ad_lavc_filter);
    if (!da)
        return NULL;

    mp_filter_add_pin(da, MP_PIN_IN, "in");
    mp_filter_add_pin(da, MP_PIN_OUT, "out");

    da->log = mp_log_new(da, parent->log, NULL);

    struct priv *priv = da->priv;
    priv->public.f = da;

    if (!init(da, codec, decoder)) {
        talloc_free(da);
        return NULL;
    }
    return &priv->public;
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_lavc_decoders(list, AVMEDIA_TYPE_AUDIO);
}

const struct mp_decoder_fns ad_lavc = {
    .create = create,
    .add_decoders = add_decoders,
};
