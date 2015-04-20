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

#include "talloc.h"

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
    struct demux_packet *packet;
    uint32_t skip_samples;
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
        OPT_INTRANGE("threads", threads, 0, 1, 16),
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
    struct sh_stream *sh = da->header;
    struct sh_audio *sh_audio = sh->audio;

    struct priv *ctx = talloc_zero(NULL, struct priv);
    da->priv = ctx;

    ctx->force_channel_map = sh_audio->force_channels;

    lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec) {
        MP_ERR(da, "Cannot find codec '%s' in libavcodec...\n", decoder);
        uninit(da);
        return 0;
    }

    lavc_context = avcodec_alloc_context3(lavc_codec);
    ctx->avctx = lavc_context;
    ctx->avframe = av_frame_alloc();
    lavc_context->refcounted_frames = 1;
    lavc_context->codec_type = AVMEDIA_TYPE_AUDIO;
    lavc_context->codec_id = lavc_codec->id;

    if (opts->downmix) {
        lavc_context->request_channel_layout =
            mp_chmap_to_lavc(&mpopts->audio_output_channels);
    }

    // Always try to set - option only exists for AC3 at the moment
    av_opt_set_double(lavc_context, "drc_scale", opts->ac3drc,
                      AV_OPT_SEARCH_CHILDREN);

#if HAVE_AVFRAME_SKIP_SAMPLES
    // Let decoder add AV_FRAME_DATA_SKIP_SAMPLES.
    av_opt_set(lavc_context, "flags2", "+skip_manual", AV_OPT_SEARCH_CHILDREN);
#endif

    mp_set_avopts(da->log, lavc_context, opts->avopts);

    lavc_context->codec_tag = sh->format;
    lavc_context->sample_rate = sh_audio->samplerate;
    lavc_context->bit_rate = sh_audio->bitrate;
    lavc_context->block_align = sh_audio->block_align;
    lavc_context->bits_per_coded_sample = sh_audio->bits_per_coded_sample;
    lavc_context->channels = sh_audio->channels.num;
    if (!mp_chmap_is_unknown(&sh_audio->channels))
        lavc_context->channel_layout = mp_chmap_to_lavc(&sh_audio->channels);

    // demux_mkv
    if (sh_audio->codecdata_len && sh_audio->codecdata &&
            !lavc_context->extradata) {
        mp_lavc_set_extradata(lavc_context, sh_audio->codecdata,
                              sh_audio->codecdata_len);
    }

    if (sh->lav_headers)
        mp_copy_lav_codec_headers(lavc_context, sh->lav_headers);

    mp_set_avcodec_threads(da->log, lavc_context, opts->threads);

    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, NULL) < 0) {
        MP_ERR(da, "Could not open codec.\n");
        uninit(da);
        return 0;
    }

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
        talloc_free(ctx->packet);
        ctx->packet = NULL;
        ctx->skip_samples = 0;
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static int decode_packet(struct dec_audio *da, struct mp_audio **out)
{
    struct priv *priv = da->priv;
    AVCodecContext *avctx = priv->avctx;

    struct demux_packet *mpkt = priv->packet;
    if (!mpkt) {
        if (demux_read_packet_async(da->header, &mpkt) == 0)
            return AD_WAIT;
    }

    priv->packet = talloc_steal(priv, mpkt);

    int in_len = mpkt ? mpkt->len : 0;

    AVPacket pkt;
    mp_set_av_packet(&pkt, mpkt, NULL);

    // If we don't have a PTS yet, use the first packet PTS we can get.
    if (da->pts == MP_NOPTS_VALUE && mpkt && mpkt->pts != MP_NOPTS_VALUE) {
        da->pts = mpkt->pts;
        da->pts_offset = 0;
    }

    int got_frame = 0;
    av_frame_unref(priv->avframe);
    int ret = avcodec_decode_audio4(avctx, priv->avframe, &got_frame, &pkt);
    if (mpkt) {
        // At least "shorten" decodes sub-frames, instead of the whole packet.
        // At least "mpc8" can return 0 and wants the packet again next time.
        if (ret >= 0) {
            ret = FFMIN(ret, mpkt->len); // sanity check against decoder overreads
            mpkt->buffer += ret;
            mpkt->len    -= ret;
            mpkt->pts = MP_NOPTS_VALUE; // don't reset PTS next time
        }
        if (mpkt->len == 0 || ret < 0) {
            talloc_free(mpkt);
            priv->packet = NULL;
        }
        // LATM may need many packets to find mux info
        if (ret == AVERROR(EAGAIN))
            return AD_OK;
    }
    if (ret < 0) {
        MP_ERR(da, "Error decoding audio.\n");
        return AD_ERR;
    }
    if (!got_frame)
        return mpkt ? AD_OK : AD_EOF;

    double out_pts = mp_pts_from_av(priv->avframe->pkt_pts, NULL);
    if (out_pts != MP_NOPTS_VALUE) {
        da->pts = out_pts;
        da->pts_offset = 0;
    }

    struct mp_audio *mpframe = mp_audio_from_avframe(priv->avframe);
    if (!mpframe)
        return AD_ERR;

    struct mp_chmap lavc_chmap = mpframe->channels;
    if (lavc_chmap.num != avctx->channels)
        mp_chmap_from_channels(&lavc_chmap, avctx->channels);
    if (priv->force_channel_map) {
        struct sh_audio *sh_audio = da->header->audio;
        if (lavc_chmap.num == sh_audio->channels.num)
            lavc_chmap = sh_audio->channels;
    }
    mp_audio_set_channels(mpframe, &lavc_chmap);

#if HAVE_AVFRAME_SKIP_SAMPLES
    AVFrameSideData *sd =
        av_frame_get_side_data(priv->avframe, AV_FRAME_DATA_SKIP_SAMPLES);
    if (sd && sd->size >= 10) {
        char *d = sd->data;
        priv->skip_samples += AV_RL32(d + 0);
        uint32_t pad = AV_RL32(d + 4);
        uint32_t skip = MPMIN(priv->skip_samples, mpframe->samples);
        if (skip) {
            mp_audio_skip_samples(mpframe, skip);
            da->pts_offset += skip;
            priv->skip_samples -= skip;
        }
        if (pad <= mpframe->samples)
            mpframe->samples -= pad;
    }
#endif

    *out = mpframe;

    av_frame_unref(priv->avframe);

    MP_DBG(da, "Decoded %d -> %d samples\n", in_len, mpframe->samples);
    return 0;
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
    .decode_packet = decode_packet,
};
