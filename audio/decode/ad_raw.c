/*
 * Copyright (C) 2018 Aman Gupta
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

#include <string.h>
#include <assert.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "audio/aframe.h"
#include "audio/format.h"
#include "common/av_common.h"
#include "common/codecs.h"
#include "common/msg.h"
#include "demux/packet.h"
#include "demux/stheader.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/filter_internal.h"
#include "options/options.h"

struct rawContext {
    struct mp_log   *log;
    enum AVCodecID   codec_id;
    struct mp_aframe_pool *pool;
    struct mp_aframe *fmt;
    int              sstride;

    struct mp_decoder public;
};

static void determine_codec_params(struct mp_filter *da, AVPacket *pkt,
                                   int *out_profile, int *out_rate,
                                   int64_t *out_bitrate)
{
    struct rawContext *raw_ctx = da->priv;
    int profile = FF_PROFILE_UNKNOWN;
    AVCodecContext *ctx = NULL;
    AVFrame *frame = NULL;

    AVCodecParserContext *parser = av_parser_init(raw_ctx->codec_id);
    if (parser) {
        // Don't make it wait for the next frame.
        parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

        ctx = avcodec_alloc_context3(NULL);
        if (!ctx) {
            av_parser_close(parser);
            goto done;
        }

        uint8_t *d = NULL;
        int s = 0;
        av_parser_parse2(parser, ctx, &d, &s, pkt->data, pkt->size, 0, 0, 0);
        *out_profile = profile = ctx->profile;
        *out_rate = ctx->sample_rate;
        *out_bitrate = ctx->bit_rate;

        avcodec_free_context(&ctx);
        av_parser_close(parser);
    }

    if (profile != FF_PROFILE_UNKNOWN || raw_ctx->codec_id != AV_CODEC_ID_DTS)
        return;

    AVCodec *codec = avcodec_find_decoder(raw_ctx->codec_id);
    if (!codec)
        goto done;

    frame = av_frame_alloc();
    if (!frame)
        goto done;

    ctx = avcodec_alloc_context3(codec);
    if (!ctx)
        goto done;

    if (avcodec_open2(ctx, codec, NULL) < 0)
        goto done;

    if (avcodec_send_packet(ctx, pkt) < 0)
        goto done;
    if (avcodec_receive_frame(ctx, frame) < 0)
        goto done;

    *out_profile = profile = ctx->profile;
    *out_rate = ctx->sample_rate;

done:
    av_frame_free(&frame);
    avcodec_free_context(&ctx);

    if (profile == FF_PROFILE_UNKNOWN)
        MP_WARN(da, "Failed to parse codec profile.\n");
}


static void process(struct mp_filter *da)
{
    struct rawContext *raw_ctx = da->priv;

    if (!mp_pin_can_transfer_data(da->ppins[1], da->ppins[0]))
        return;

    struct mp_frame inframe = mp_pin_out_read(da->ppins[0]);
    if (inframe.type == MP_FRAME_EOF) {
        mp_pin_in_write(da->ppins[1], inframe);
        return;
    } else if (inframe.type != MP_FRAME_PACKET) {
        if (inframe.type) {
            MP_ERR(da, "unknown frame type\n");
            mp_filter_internal_mark_failed(da);
        }
        return;
    }

    struct demux_packet *mpkt = inframe.data;
    struct mp_aframe *out = NULL;
    double pts = mpkt->pts;

    if (!raw_ctx->fmt) {
        AVPacket pkt;
        mp_set_av_packet(&pkt, mpkt, NULL);
        pkt.pts = pkt.dts = 0;
        int profile = FF_PROFILE_UNKNOWN;
        int c_rate = 0;
        int64_t bitrate = 0;
        determine_codec_params(da, &pkt, &profile, &c_rate, &bitrate);
        MP_VERBOSE(da, "In: profile=%d samplerate=%d bitrate=%"PRId64"\n", profile, c_rate, bitrate);

        raw_ctx->fmt = mp_aframe_create();
        talloc_steal(raw_ctx, raw_ctx->fmt);

        struct mp_chmap chmap;
        mp_chmap_from_channels(&chmap, 2);
        mp_aframe_set_chmap(raw_ctx->fmt, &chmap);
        mp_aframe_set_format(raw_ctx->fmt, AF_FORMAT_R_AC3);
        mp_aframe_set_rate(raw_ctx->fmt, c_rate > 0 ? c_rate : 48000);
        mp_aframe_set_bitrate(raw_ctx->fmt, bitrate);
        raw_ctx->sstride = mp_aframe_get_sstride(raw_ctx->fmt);
    }

    out = mp_aframe_new_ref(raw_ctx->fmt);
    int samples = mpkt->len / raw_ctx->sstride;
    if (mp_aframe_pool_allocate(raw_ctx->pool, out, samples) < 0) {
        TA_FREEP(&out);
        goto done;
    }

    uint8_t **data = mp_aframe_get_data_rw(out);
    if (!data) {
        TA_FREEP(&out);
        goto done;
    }

    memcpy(data[0], mpkt->buffer, mpkt->len);
    mp_aframe_set_pts(out, pts);

done:
    talloc_free(mpkt);
    if (out) {
        mp_pin_in_write(da->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, out));
    } else {
        mp_filter_internal_mark_failed(da);
    }
}

static const int codecs[] = {
    AV_CODEC_ID_AAC,
    AV_CODEC_ID_AC3,
    AV_CODEC_ID_DTS,
    AV_CODEC_ID_EAC3,
    AV_CODEC_ID_MP3,
    AV_CODEC_ID_TRUEHD,
    AV_CODEC_ID_NONE
};

static bool find_codec(const char *name)
{
    for (int n = 0; codecs[n] != AV_CODEC_ID_NONE; n++) {
        const char *format = mp_codec_from_av_codec_id(codecs[n]);
        if (format && name && strcmp(format, name) == 0)
            return true;
    }
    return false;
}

// codec is the libavcodec name of the source audio codec.
// pref is a ","-separated list of names, some of them which do not match with
// libavcodec names (like dts-hd).
struct mp_decoder_list *select_raw_codec(const char *codec, const char *pref)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);

    if (!find_codec(codec))
        return list;

    bool raw_allowed = false, dts_hd_allowed = false;
    bstr sel = bstr0(pref);
    while (sel.len) {
        bstr decoder;
        bstr_split_tok(sel, ",", &decoder, &sel);
        if (decoder.len) {
            if (bstr_equals0(decoder, codec))
                raw_allowed = true;
            if (bstr_equals0(decoder, "dts-hd") && strcmp(codec, "dts") == 0)
                raw_allowed = dts_hd_allowed = true;
        }
    }

    if (!raw_allowed)
        return list;

    const char *suffix_name = dts_hd_allowed ? "dts_hd" : codec;
    char name[80];
    snprintf(name, sizeof(name), "raw_%s", suffix_name);
    mp_add_decoder(list, codec, name,
                   "raw audio pass-through decoder");
    return list;
}

static const struct mp_filter_info ad_raw_filter = {
    .name = "ad_raw",
    .priv_size = sizeof(struct rawContext),
    .process = process,
};

static struct mp_decoder *create(struct mp_filter *parent,
                                 struct mp_codec_params *codec,
                                 const char *decoder)
{
    struct mp_filter *da = mp_filter_create(parent, &ad_raw_filter);
    if (!da)
        return NULL;

    mp_filter_add_pin(da, MP_PIN_IN, "in");
    mp_filter_add_pin(da, MP_PIN_OUT, "out");

    da->log = mp_log_new(da, parent->log, NULL);

    struct rawContext *raw_ctx = da->priv;
    raw_ctx->log = da->log;
    raw_ctx->pool = mp_aframe_pool_create(raw_ctx);
    raw_ctx->public.f = da;

    raw_ctx->codec_id = mp_codec_to_av_codec_id(codec->codec);
    if (raw_ctx->codec_id == AV_CODEC_ID_NONE) {
        talloc_free(da);
        return NULL;
    }

    return &raw_ctx->public;
}

const struct mp_decoder_fns ad_raw = {
    .create = create,
};
