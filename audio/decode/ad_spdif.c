/*
 * Copyright (C) 2012 Naoya OYAMA
 *
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

#include <string.h>
#include <assert.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "config.h"
#include "common/msg.h"
#include "common/av_common.h"
#include "options/options.h"
#include "ad.h"

#define OUTBUF_SIZE 65536

struct spdifContext {
    struct mp_log   *log;
    AVFormatContext *lavf_ctx;
    int              out_buffer_len;
    uint8_t          out_buffer[OUTBUF_SIZE];
    bool             need_close;
    struct mp_audio  fmt;
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

static void uninit(struct dec_audio *da)
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
    }
}

static int init(struct dec_audio *da, const char *decoder)
{
    struct spdifContext *spdif_ctx = talloc_zero(NULL, struct spdifContext);
    da->priv = spdif_ctx;
    spdif_ctx->log = da->log;

    AVFormatContext *lavf_ctx  = avformat_alloc_context();
    if (!lavf_ctx)
        goto fail;

    lavf_ctx->oformat = av_guess_format("spdif", NULL, NULL);
    if (!lavf_ctx->oformat)
        goto fail;

    spdif_ctx->lavf_ctx = lavf_ctx;

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

    stream->codec->codec_id = mp_codec_to_av_codec_id(decoder);

    AVDictionary *format_opts = NULL;

    int num_channels = 0;
    int sample_format = 0;
    int samplerate = 0;
    switch (stream->codec->codec_id) {
    case AV_CODEC_ID_AAC:
        sample_format                   = AF_FORMAT_S_AAC;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_AC3:
        sample_format                   = AF_FORMAT_S_AC3;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_DTS:
        if (da->opts->dtshd) {
            av_dict_set(&format_opts, "dtshd_rate", "768000", 0); // 4*192000
            sample_format                   = AF_FORMAT_S_DTSHD;
            samplerate                      = 192000;
            num_channels                    = 2*4;
        } else {
            sample_format                   = AF_FORMAT_S_DTS;
            samplerate                      = 48000;
            num_channels                    = 2;
        }
        break;
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
    mp_audio_set_num_channels(&spdif_ctx->fmt, num_channels);
    mp_audio_set_format(&spdif_ctx->fmt, sample_format);
    spdif_ctx->fmt.rate = samplerate;

    if (avformat_write_header(lavf_ctx, &format_opts) < 0) {
        MP_FATAL(da, "libavformat spdif initialization failed.\n");
        av_dict_free(&format_opts);
        goto fail;
    }
    av_dict_free(&format_opts);

    spdif_ctx->need_close = true;

    return 1;

fail:
    uninit(da);
    return 0;
}

static int decode_packet(struct dec_audio *da, struct mp_audio **out)
{
    struct spdifContext *spdif_ctx = da->priv;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;

    spdif_ctx->out_buffer_len  = 0;

    struct demux_packet *mpkt;
    if (demux_read_packet_async(da->header, &mpkt) == 0)
        return AD_WAIT;

    if (!mpkt)
        return AD_EOF;

    AVPacket pkt;
    mp_set_av_packet(&pkt, mpkt, NULL);
    pkt.pts = pkt.dts = 0;
    if (mpkt->pts != MP_NOPTS_VALUE) {
        da->pts        = mpkt->pts;
        da->pts_offset = 0;
    }
    int ret = av_write_frame(lavf_ctx, &pkt);
    talloc_free(mpkt);
    avio_flush(lavf_ctx->pb);
    if (ret < 0)
        return AD_ERR;

    int samples = spdif_ctx->out_buffer_len / spdif_ctx->fmt.sstride;
    *out = mp_audio_pool_get(da->pool, &spdif_ctx->fmt, samples);
    if (!*out)
        return AD_ERR;

    memcpy((*out)->planes[0], spdif_ctx->out_buffer, spdif_ctx->out_buffer_len);

    return 0;
}

static int control(struct dec_audio *da, int cmd, void *arg)
{
    return CONTROL_UNKNOWN;
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

static void add_decoders(struct mp_decoder_list *list)
{
    for (int n = 0; codecs[n] != AV_CODEC_ID_NONE; n++) {
        const char *format = mp_codec_from_av_codec_id(codecs[n]);
        if (format) {
            mp_add_decoder(list, "spdif", format, format,
                           "libavformat/spdifenc audio pass-through decoder");
        }
    }
}

const struct ad_functions ad_spdif = {
    .name = "spdif",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode_packet = decode_packet,
};
