/*
 * This file is part of MPlayer.
 *
 * Copyright (C) 2012 Naoya OYAMA
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
    int              iec61937_packet_size;
    int              out_buffer_len;
    int              out_buffer_size;
    uint8_t         *out_buffer;
    bool             need_close;
};

static int write_packet(void *p, uint8_t *buf, int buf_size)
{
    struct spdifContext *ctx = p;

    int buffer_left = ctx->out_buffer_size - ctx->out_buffer_len;
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
        spdif_ctx->iec61937_packet_size = 16384;
        sample_format                   = AF_FORMAT_IEC61937_LE;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_AC3:
        spdif_ctx->iec61937_packet_size = 6144;
        sample_format                   = AF_FORMAT_AC3_LE;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_DTS:
        if (da->opts->dtshd) {
            av_dict_set(&format_opts, "dtshd_rate", "768000", 0); // 4*192000
            spdif_ctx->iec61937_packet_size = 32768;
            sample_format                   = AF_FORMAT_IEC61937_LE;
            samplerate                      = 192000;
            num_channels                    = 2*4;
        } else {
            spdif_ctx->iec61937_packet_size = 32768;
            sample_format                   = AF_FORMAT_AC3_LE;
            samplerate                      = 48000;
            num_channels                    = 2;
        }
        break;
    case AV_CODEC_ID_EAC3:
        spdif_ctx->iec61937_packet_size = 24576;
        sample_format                   = AF_FORMAT_IEC61937_LE;
        samplerate                      = 192000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_MP3:
        spdif_ctx->iec61937_packet_size = 4608;
        sample_format                   = AF_FORMAT_MPEG2;
        samplerate                      = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_TRUEHD:
        spdif_ctx->iec61937_packet_size = 61440;
        sample_format                   = AF_FORMAT_IEC61937_LE;
        samplerate                      = 192000;
        num_channels                    = 8;
        break;
    default:
        abort();
    }
    mp_audio_set_num_channels(&da->decoded, num_channels);
    mp_audio_set_format(&da->decoded, sample_format);
    da->decoded.rate = samplerate;

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

static int decode_audio(struct dec_audio *da, struct mp_audio *buffer, int maxlen)
{
    struct spdifContext *spdif_ctx = da->priv;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;

    int sstride = 2 * da->decoded.channels.num;
    assert(sstride == buffer->sstride);

    if (maxlen * sstride < spdif_ctx->iec61937_packet_size)
        return 0;

    spdif_ctx->out_buffer_len  = 0;
    spdif_ctx->out_buffer_size = maxlen * sstride;
    spdif_ctx->out_buffer      = buffer->planes[0];

    struct demux_packet *mpkt = demux_read_packet(da->header);
    if (!mpkt)
        return AD_ERR;

    AVPacket pkt;
    mp_set_av_packet(&pkt, mpkt, NULL);
    pkt.pts = pkt.dts = 0;
    MP_VERBOSE(da, "spdif packet, size=%d\n", pkt.size);
    if (mpkt->pts != MP_NOPTS_VALUE) {
        da->pts        = mpkt->pts;
        da->pts_offset = 0;
    }
    int ret = av_write_frame(lavf_ctx, &pkt);
    avio_flush(lavf_ctx->pb);
    buffer->samples = spdif_ctx->out_buffer_len / sstride;
    da->pts_offset += buffer->samples;
    talloc_free(mpkt);
    if (ret < 0)
        return AD_ERR;

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
    .decode_audio = decode_audio,
};
