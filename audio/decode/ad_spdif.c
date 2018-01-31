/*
 * Copyright (C) 2012 Naoya OYAMA
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

#define OUTBUF_SIZE 65536

struct spdifContext {
    struct mp_log   *log;
    enum AVCodecID   codec_id;
    AVFormatContext *lavf_ctx;
    int              out_buffer_len;
    uint8_t          out_buffer[OUTBUF_SIZE];
    bool             need_close;
    bool             use_dts_hd;
    struct mp_aframe *fmt;
    int              sstride;
    struct mp_aframe_pool *pool;

    struct mp_decoder public;
};

static int write_packet(void *p, uint8_t *buf, int buf_size)
{
    struct spdifContext *ctx = p;

    int buffer_left = OUTBUF_SIZE - ctx->out_buffer_len;
    if (buf_size > buffer_left) {
        MP_ERR(ctx, "spdif packet too large.\n");
        buf_size = buffer_left;
    }

    memcpy(&ctx->out_buffer[ctx->out_buffer_len], buf, buf_size);
    ctx->out_buffer_len += buf_size;
    return buf_size;
}

// (called on both filter destruction _and_ if lavf fails to init)
static void destroy(struct mp_filter *da)
{
    struct spdifContext *spdif_ctx = da->priv;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;

    if (lavf_ctx) {
        if (spdif_ctx->need_close)
            av_write_trailer(lavf_ctx);
        if (lavf_ctx->pb)
            av_freep(&lavf_ctx->pb->buffer);
        av_freep(&lavf_ctx->pb);
        avformat_free_context(lavf_ctx);
        spdif_ctx->lavf_ctx = NULL;
    }
}

static void determine_codec_params(struct mp_filter *da, AVPacket *pkt,
                                   int *out_profile, int *out_rate)
{
    struct spdifContext *spdif_ctx = da->priv;
    int profile = FF_PROFILE_UNKNOWN;
    AVCodecContext *ctx = NULL;
    AVFrame *frame = NULL;

    AVCodecParserContext *parser = av_parser_init(spdif_ctx->codec_id);
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

        avcodec_free_context(&ctx);
        av_parser_close(parser);
    }

    if (profile != FF_PROFILE_UNKNOWN || spdif_ctx->codec_id != AV_CODEC_ID_DTS)
        return;

    AVCodec *codec = avcodec_find_decoder(spdif_ctx->codec_id);
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

static int init_filter(struct mp_filter *da, AVPacket *pkt)
{
    struct spdifContext *spdif_ctx = da->priv;

    int profile = FF_PROFILE_UNKNOWN;
    int c_rate = 0;
    determine_codec_params(da, pkt, &profile, &c_rate);
    MP_VERBOSE(da, "In: profile=%d samplerate=%d\n", profile, c_rate);

    AVFormatContext *lavf_ctx  = avformat_alloc_context();
    if (!lavf_ctx)
        goto fail;

    spdif_ctx->lavf_ctx = lavf_ctx;

    lavf_ctx->oformat = av_guess_format("spdif", NULL, NULL);
    if (!lavf_ctx->oformat)
        goto fail;

    void *buffer = av_mallocz(OUTBUF_SIZE);
    if (!buffer)
        abort();
    lavf_ctx->pb = avio_alloc_context(buffer, OUTBUF_SIZE, 1, spdif_ctx, NULL,
                                      write_packet, NULL);
    if (!lavf_ctx->pb) {
        av_free(buffer);
        goto fail;
    }

    // Request minimal buffering (not available on Libav)
#if LIBAVFORMAT_VERSION_MICRO >= 100
    lavf_ctx->pb->direct = 1;
#endif

    AVStream *stream = avformat_new_stream(lavf_ctx, 0);
    if (!stream)
        goto fail;

    stream->codecpar->codec_id = spdif_ctx->codec_id;

    AVDictionary *format_opts = NULL;

    spdif_ctx->fmt = mp_aframe_create();
    talloc_steal(spdif_ctx, spdif_ctx->fmt);

    int num_channels = 0;
    int sample_format = 0;
    int samplerate = 0;
    switch (spdif_ctx->codec_id) {
    case AV_CODEC_ID_AAC:
        sample_format                   = AF_FORMAT_S_AAC;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_AC3:
        sample_format                   = AF_FORMAT_S_AC3;
        samplerate                      = c_rate > 0 ? c_rate : 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_DTS: {
        bool is_hd = profile == FF_PROFILE_DTS_HD_HRA ||
                     profile == FF_PROFILE_DTS_HD_MA ||
                     profile == FF_PROFILE_UNKNOWN;
        if (spdif_ctx->use_dts_hd && is_hd) {
            av_dict_set(&format_opts, "dtshd_rate", "768000", 0); // 4*192000
            sample_format               = AF_FORMAT_S_DTSHD;
            samplerate                  = 192000;
            num_channels                = 2*4;
        } else {
            sample_format               = AF_FORMAT_S_DTS;
            samplerate                  = 48000;
            num_channels                = 2;
        }
        break;
    }
    case AV_CODEC_ID_EAC3:
        sample_format                   = AF_FORMAT_S_EAC3;
        samplerate                      = 192000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_MP3:
        sample_format                   = AF_FORMAT_S_MP3;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_TRUEHD:
        sample_format                   = AF_FORMAT_S_TRUEHD;
        samplerate                      = 192000;
        num_channels                    = 8;
        break;
    default:
        abort();
    }

    struct mp_chmap chmap;
    mp_chmap_from_channels(&chmap, num_channels);
    mp_aframe_set_chmap(spdif_ctx->fmt, &chmap);
    mp_aframe_set_format(spdif_ctx->fmt, sample_format);
    mp_aframe_set_rate(spdif_ctx->fmt, samplerate);

    spdif_ctx->sstride = mp_aframe_get_sstride(spdif_ctx->fmt);

    if (avformat_write_header(lavf_ctx, &format_opts) < 0) {
        MP_FATAL(da, "libavformat spdif initialization failed.\n");
        av_dict_free(&format_opts);
        goto fail;
    }
    av_dict_free(&format_opts);

    spdif_ctx->need_close = true;

    return 0;

fail:
    destroy(da);
    mp_filter_internal_mark_failed(da);
    return -1;
}

static void process(struct mp_filter *da)
{
    struct spdifContext *spdif_ctx = da->priv;

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

    AVPacket pkt;
    mp_set_av_packet(&pkt, mpkt, NULL);
    pkt.pts = pkt.dts = 0;
    if (!spdif_ctx->lavf_ctx) {
        if (init_filter(da, &pkt) < 0)
            goto done;
    }
    spdif_ctx->out_buffer_len  = 0;
    int ret = av_write_frame(spdif_ctx->lavf_ctx, &pkt);
    avio_flush(spdif_ctx->lavf_ctx->pb);
    if (ret < 0) {
        MP_ERR(da, "spdif mux error: '%s'\n", mp_strerror(AVUNERROR(ret)));
        goto done;
    }

    out = mp_aframe_new_ref(spdif_ctx->fmt);
    int samples = spdif_ctx->out_buffer_len / spdif_ctx->sstride;
    if (mp_aframe_pool_allocate(spdif_ctx->pool, out, samples) < 0) {
        TA_FREEP(&out);
        goto done;
    }

    uint8_t **data = mp_aframe_get_data_rw(out);
    if (!data) {
        TA_FREEP(&out);
        goto done;
    }

    memcpy(data[0], spdif_ctx->out_buffer, spdif_ctx->out_buffer_len);
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
struct mp_decoder_list *select_spdif_codec(const char *codec, const char *pref)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);

    if (!find_codec(codec))
        return list;

    bool spdif_allowed = false, dts_hd_allowed = false;
    bstr sel = bstr0(pref);
    while (sel.len) {
        bstr decoder;
        bstr_split_tok(sel, ",", &decoder, &sel);
        if (decoder.len) {
            if (bstr_equals0(decoder, codec))
                spdif_allowed = true;
            if (bstr_equals0(decoder, "dts-hd") && strcmp(codec, "dts") == 0)
                spdif_allowed = dts_hd_allowed = true;
        }
    }

    if (!spdif_allowed)
        return list;

    const char *suffix_name = dts_hd_allowed ? "dts_hd" : codec;
    char name[80];
    snprintf(name, sizeof(name), "spdif_%s", suffix_name);
    mp_add_decoder(list, codec, name,
                   "libavformat/spdifenc audio pass-through decoder");
    return list;
}

static const struct mp_filter_info ad_spdif_filter = {
    .name = "ad_spdif",
    .priv_size = sizeof(struct spdifContext),
    .process = process,
    .destroy = destroy,
};

static struct mp_decoder *create(struct mp_filter *parent,
                                 struct mp_codec_params *codec,
                                 const char *decoder)
{
    struct mp_filter *da = mp_filter_create(parent, &ad_spdif_filter);
    if (!da)
        return NULL;

    mp_filter_add_pin(da, MP_PIN_IN, "in");
    mp_filter_add_pin(da, MP_PIN_OUT, "out");

    da->log = mp_log_new(da, parent->log, NULL);

    struct spdifContext *spdif_ctx = da->priv;
    spdif_ctx->log = da->log;
    spdif_ctx->pool = mp_aframe_pool_create(spdif_ctx);
    spdif_ctx->public.f = da;

    if (strcmp(decoder, "spdif_dts_hd") == 0)
        spdif_ctx->use_dts_hd = true;

    spdif_ctx->codec_id = mp_codec_to_av_codec_id(codec->codec);


    if (spdif_ctx->codec_id == AV_CODEC_ID_NONE) {
        talloc_free(da);
        return NULL;
    }
    return &spdif_ctx->public;
}

const struct mp_decoder_fns ad_spdif = {
    .create = create,
};
