/*
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>

#include "talloc.h"

#include "config.h"
#include "common/av_common.h"
#include "common/codecs.h"
#include "common/msg.h"
#include "options/options.h"
#include "common/av_opts.h"

#include "ad.h"
#include "audio/fmt-conversion.h"

#include "compat/libav.h"

struct priv {
    AVCodecContext *avctx;
    AVFrame *avframe;
    struct mp_audio frame;
    bool force_channel_map;
    struct demux_packet *packet;
};

static void uninit(struct dec_audio *da);

#define OPT_BASE_STRUCT struct ad_lavc_params
struct ad_lavc_params {
    float ac3drc;
    int downmix;
    int threads;
    char *avopt;
};

const struct m_sub_options ad_lavc_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLOATRANGE("ac3drc", ac3drc, 0, 0, 2),
        OPT_FLAG("downmix", downmix, 0),
        OPT_INTRANGE("threads", threads, 0, 1, 16),
        OPT_STRING("o", avopt, 0),
        {0}
    },
    .size = sizeof(struct ad_lavc_params),
    .defaults = &(const struct ad_lavc_params){
        .ac3drc = 1.,
        .downmix = 1,
        .threads = 1,
    },
};

struct pcm_map
{
    int tag;
    const char *codecs[6]; // {any, 1byte, 2bytes, 3bytes, 4bytes, 8bytes}
};

// NOTE: these are needed to make rawaudio with demux_mkv work.
static const struct pcm_map tag_map[] = {
    // Microsoft PCM
    {0x0,           {NULL, "pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"}},
    {0x1,           {NULL, "pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"}},
    // MS PCM, Extended
    {0xfffe,        {NULL, "pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"}},
    // IEEE float
    {0x3,           {"pcm_f32le", [5] = "pcm_f64le"}},
    // 'raw '
    {0x20776172,    {"pcm_s16be", [1] = "pcm_u8"}},
    // 'twos', used by demux_mkv.c internally
    {MKTAG('t', 'w', 'o', 's'),
                    {NULL, "pcm_s8", "pcm_s16be", "pcm_s24be", "pcm_s32be"}},
    {-1},
};

// For demux_rawaudio.c; needed because ffmpeg doesn't have these sample
// formats natively.
static const struct pcm_map af_map[] = {
    {AF_FORMAT_U8,              {"pcm_u8"}},
    {AF_FORMAT_S8,              {"pcm_u8"}},
    {AF_FORMAT_U16_LE,          {"pcm_u16le"}},
    {AF_FORMAT_U16_BE,          {"pcm_u16be"}},
    {AF_FORMAT_S16_LE,          {"pcm_s16le"}},
    {AF_FORMAT_S16_BE,          {"pcm_s16be"}},
    {AF_FORMAT_U24_LE,          {"pcm_u24le"}},
    {AF_FORMAT_U24_BE,          {"pcm_u24be"}},
    {AF_FORMAT_S24_LE,          {"pcm_s24le"}},
    {AF_FORMAT_S24_BE,          {"pcm_s24be"}},
    {AF_FORMAT_U32_LE,          {"pcm_u32le"}},
    {AF_FORMAT_U32_BE,          {"pcm_u32be"}},
    {AF_FORMAT_S32_LE,          {"pcm_s32le"}},
    {AF_FORMAT_S32_BE,          {"pcm_s32be"}},
    {AF_FORMAT_FLOAT_LE,        {"pcm_f32le"}},
    {AF_FORMAT_FLOAT_BE,        {"pcm_f32be"}},
    {AF_FORMAT_DOUBLE_LE,       {"pcm_f64le"}},
    {AF_FORMAT_DOUBLE_BE,       {"pcm_f64be"}},
    {-1},
};

static const char *find_pcm_decoder(const struct pcm_map *map, int format,
                                    int bits_per_sample)
{
    int bytes = (bits_per_sample + 7) / 8;
    if (bytes == 8)
        bytes = 5; // 64 bit entry
    for (int n = 0; map[n].tag != -1; n++) {
        const struct pcm_map *entry = &map[n];
        if (entry->tag == format) {
            const char *dec = NULL;
            if (bytes >= 1 && bytes <= 5)
                dec = entry->codecs[bytes];
            if (!dec)
                dec = entry->codecs[0];
            if (dec)
                return dec;
        }
    }
    return NULL;
}

static void set_data_from_avframe(struct dec_audio *da)
{
    struct priv *priv = da->priv;
    AVCodecContext *lavc_context = priv->avctx;

    // Note: invalid parameters are rejected by dec_audio.c

    int fmt = lavc_context->sample_fmt;
    mp_audio_set_format(&da->decoded, af_from_avformat(fmt));
    if (!da->decoded.format)
        MP_FATAL(da, "unsupported lavc format %s", av_get_sample_fmt_name(fmt));

    da->decoded.rate = lavc_context->sample_rate;

    struct mp_chmap lavc_chmap;
    mp_chmap_from_lavc(&lavc_chmap, lavc_context->channel_layout);
    // No channel layout or layout disagrees with channel count
    if (lavc_chmap.num != lavc_context->channels)
        mp_chmap_from_channels(&lavc_chmap, lavc_context->channels);
    if (priv->force_channel_map) {
        struct sh_audio *sh_audio = da->header->audio;
        if (lavc_chmap.num == sh_audio->channels.num)
            lavc_chmap = sh_audio->channels;
    }
    mp_audio_set_channels(&da->decoded, &lavc_chmap);

    da->decoded.samples = priv->avframe->nb_samples;
    for (int n = 0; n < da->decoded.num_planes; n++)
        da->decoded.planes[n] = priv->avframe->data[n];
}

static void set_from_wf(AVCodecContext *avctx, MP_WAVEFORMATEX *wf)
{
    avctx->channels = wf->nChannels;
    avctx->sample_rate = wf->nSamplesPerSec;
    avctx->bit_rate = wf->nAvgBytesPerSec * 8;
    avctx->block_align = wf->nBlockAlign;
    avctx->bits_per_coded_sample = wf->wBitsPerSample;

    if (wf->cbSize > 0)
        mp_lavc_set_extradata(avctx, wf + 1, wf->cbSize);
}

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

    if (sh_audio->wf && strcmp(decoder, "pcm") == 0) {
        decoder = find_pcm_decoder(tag_map, sh->format,
                                   sh_audio->wf->wBitsPerSample);
    } else if (sh_audio->wf && strcmp(decoder, "mp-pcm") == 0) {
        decoder = find_pcm_decoder(af_map, sh->format, 0);
        ctx->force_channel_map = true;
    }

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

    if (opts->avopt) {
        if (parse_avopts(lavc_context, opts->avopt) < 0) {
            MP_ERR(da, "setting AVOptions '%s' failed.\n", opts->avopt);
            uninit(da);
            return 0;
        }
    }

    lavc_context->codec_tag = sh->format;
    lavc_context->sample_rate = sh_audio->samplerate;
    lavc_context->bit_rate = sh_audio->bitrate;
    lavc_context->channel_layout = mp_chmap_to_lavc(&sh_audio->channels);

    if (sh_audio->wf)
        set_from_wf(lavc_context, sh_audio->wf);

    // demux_mkv, demux_mpg
    if (sh_audio->codecdata_len && sh_audio->codecdata &&
            !lavc_context->extradata) {
        mp_lavc_set_extradata(lavc_context, sh_audio->codecdata,
                              sh_audio->codecdata_len);
    }

    if (sh->lav_headers)
        mp_copy_lav_codec_headers(lavc_context, sh->lav_headers);

    mp_set_avcodec_threads(lavc_context, opts->threads);

    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, NULL) < 0) {
        MP_ERR(da, "Could not open codec.\n");
        uninit(da);
        return 0;
    }

    if (lavc_context->bit_rate != 0)
        da->bitrate = lavc_context->bit_rate;
    else if (sh_audio->wf && sh_audio->wf->nAvgBytesPerSec)
        da->bitrate = sh_audio->wf->nAvgBytesPerSec * 8;

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
        mp_audio_set_null_data(&da->decoded);
        talloc_free(ctx->packet);
        ctx->packet = NULL;
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static int decode_packet(struct dec_audio *da)
{
    struct priv *priv = da->priv;
    AVCodecContext *avctx = priv->avctx;

    mp_audio_set_null_data(&da->decoded);

    struct demux_packet *mpkt = priv->packet;
    if (!mpkt)
        mpkt = demux_read_packet(da->header);

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
            return 0;
    }
    if (ret < 0) {
        MP_ERR(da, "Error decoding audio.\n");
        return AD_ERR;
    }
    if (!got_frame)
        return mpkt ? AD_OK : AD_EOF;

    set_data_from_avframe(da);

    double out_pts = mp_pts_from_av(priv->avframe->pkt_pts, NULL);
    if (out_pts != MP_NOPTS_VALUE) {
        da->pts = out_pts;
        da->pts_offset = 0;
    }

    MP_DBG(da, "Decoded %d -> %d samples\n", in_len, da->decoded.samples);
    return 0;
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_lavc_decoders(list, AVMEDIA_TYPE_AUDIO);
    mp_add_decoder(list, "lavc", "pcm", "pcm", "Raw PCM");
    mp_add_decoder(list, "lavc", "mp-pcm", "mp-pcm", "Raw PCM");
}

const struct ad_functions ad_lavc = {
    .name = "lavc",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode_packet = decode_packet,
};
