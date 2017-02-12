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
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>

#include "mpv_talloc.h"

#include "config.h"
#include "common/av_common.h"
#include "common/codecs.h"
#include "common/msg.h"
#include "options/options.h"

#include "ad.h"
#include "audio/fmt-conversion.h"

struct priv {
    AVCodecContext *avctx;
    AVFrame *avframe;
    struct mp_audio frame;
    bool force_channel_map;
    uint32_t skip_samples, trim_samples;
    bool preroll_done;
    double next_pts;
    AVRational codec_timebase;
};

static void uninit(struct dec_audio *da);

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

static int init(struct dec_audio *da, const char *decoder)
{
    struct MPOpts *mpopts = da->opts;
    struct ad_lavc_params *opts = mpopts->ad_lavc_params;
    AVCodecContext *lavc_context;
    AVCodec *lavc_codec;
    struct mp_codec_params *c = da->codec;

    struct priv *ctx = talloc_zero(NULL, struct priv);
    da->priv = ctx;

    ctx->codec_timebase = mp_get_codec_timebase(da->codec);

    ctx->force_channel_map = c->force_channels;

    lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec) {
        MP_ERR(da, "Cannot find codec '%s' in libavcodec...\n", decoder);
        uninit(da);
        return 0;
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

    if (mp_set_avctx_codec_headers(lavc_context, c) < 0) {
        MP_ERR(da, "Could not set decoder parameters.\n");
        uninit(da);
        return 0;
    }

    mp_set_avcodec_threads(da->log, lavc_context, opts->threads);

    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, NULL) < 0) {
        MP_ERR(da, "Could not open codec.\n");
        uninit(da);
        return 0;
    }

    ctx->next_pts = MP_NOPTS_VALUE;

    return 1;
}

static void uninit(struct dec_audio *da)
{
    struct priv *ctx = da->priv;
    if (!ctx)
        return;
    AVCodecContext *lavc_context = ctx->avctx;

    if (lavc_context) {
        if (avcodec_close(lavc_context) < 0)
            MP_ERR(da, "Could not close codec.\n");
        av_freep(&lavc_context->extradata);
        av_freep(&lavc_context);
    }
    av_frame_free(&ctx->avframe);
}

static int control(struct dec_audio *da, int cmd, void *arg)
{
    struct priv *ctx = da->priv;
    switch (cmd) {
    case ADCTRL_RESET:
        avcodec_flush_buffers(ctx->avctx);
        ctx->skip_samples = 0;
        ctx->trim_samples = 0;
        ctx->preroll_done = false;
        ctx->next_pts = MP_NOPTS_VALUE;
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static bool send_packet(struct dec_audio *da, struct demux_packet *mpkt)
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

static bool receive_frame(struct dec_audio *da, struct mp_audio **out)
{
    struct priv *priv = da->priv;
    AVCodecContext *avctx = priv->avctx;

    int ret = avcodec_receive_frame(avctx, priv->avframe);

    if (ret == AVERROR_EOF) {
        // If flushing was initialized earlier and has ended now, make it start
        // over in case we get new packets at some point in the future.
        control(da, ADCTRL_RESET, NULL);
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

    struct mp_audio *mpframe = mp_audio_from_avframe(priv->avframe);
    if (!mpframe)
        return true;

    struct mp_chmap lavc_chmap = mpframe->channels;
    if (lavc_chmap.num != avctx->channels)
        mp_chmap_from_channels(&lavc_chmap, avctx->channels);
    if (priv->force_channel_map) {
        if (lavc_chmap.num == da->codec->channels.num)
            lavc_chmap = da->codec->channels;
    }
    mp_audio_set_channels(mpframe, &lavc_chmap);

    mpframe->pts = out_pts;

    if (mpframe->pts == MP_NOPTS_VALUE)
        mpframe->pts = priv->next_pts;
    if (mpframe->pts != MP_NOPTS_VALUE)
        priv->next_pts = mpframe->pts + mpframe->samples / (double)mpframe->rate;

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

    uint32_t skip = MPMIN(priv->skip_samples, mpframe->samples);
    if (skip) {
        mp_audio_skip_samples(mpframe, skip);
        priv->skip_samples -= skip;
    }
    uint32_t trim = MPMIN(priv->trim_samples, mpframe->samples);
    if (trim) {
        mpframe->samples -= trim;
        priv->trim_samples -= trim;
    }

    *out = mpframe;

    av_frame_unref(priv->avframe);

    MP_DBG(da, "Decoded %d samples\n", mpframe->samples);
    return true;
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_lavc_decoders(list, AVMEDIA_TYPE_AUDIO);
}

const struct ad_functions ad_lavc = {
    .name = "lavc",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .send_packet = send_packet,
    .receive_frame = receive_frame,
};
