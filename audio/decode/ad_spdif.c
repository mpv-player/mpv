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
#include "mpvcore/mp_msg.h"
#include "mpvcore/av_common.h"
#include "mpvcore/options.h"
#include "ad.h"

#define OUTBUF_SIZE 65536

struct spdifContext {
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
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "spdif packet too large.\n");
        buf_size = buffer_left;
    }

    memcpy(&ctx->out_buffer[ctx->out_buffer_len], buf, buf_size);
    ctx->out_buffer_len += buf_size;
    return buf_size;
}

static void uninit(sh_audio_t *sh)
{
    struct spdifContext *spdif_ctx = sh->context;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;

    if (lavf_ctx) {
        if (spdif_ctx->need_close)
            av_write_trailer(lavf_ctx);
        if (lavf_ctx->pb)
            av_freep(&lavf_ctx->pb->buffer);
        av_freep(&lavf_ctx->pb);
        avformat_free_context(lavf_ctx);
    }
    talloc_free(spdif_ctx);
}

static int preinit(sh_audio_t *sh)
{
    return 1;
}

static int init(sh_audio_t *sh, const char *decoder)
{
    struct spdifContext *spdif_ctx = talloc_zero(NULL, struct spdifContext);
    sh->context = spdif_ctx;

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
    switch (stream->codec->codec_id) {
    case AV_CODEC_ID_AAC:
        spdif_ctx->iec61937_packet_size = 16384;
        sh->sample_format               = AF_FORMAT_IEC61937_LE;
        sh->samplerate                  = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_AC3:
        spdif_ctx->iec61937_packet_size = 6144;
        sh->sample_format               = AF_FORMAT_AC3_LE;
        sh->samplerate                  = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_DTS:
        if (sh->opts->dtshd) {
            av_dict_set(&format_opts, "dtshd_rate", "768000", 0); // 4*192000
            spdif_ctx->iec61937_packet_size = 32768;
            sh->sample_format               = AF_FORMAT_IEC61937_LE;
            sh->samplerate                  = 192000; // DTS core require 48000
            num_channels                    = 2*4;
        } else {
            spdif_ctx->iec61937_packet_size = 32768;
            sh->sample_format               = AF_FORMAT_AC3_LE;
            sh->samplerate                  = 48000;
            num_channels                    = 2;
        }
        break;
    case AV_CODEC_ID_EAC3:
        spdif_ctx->iec61937_packet_size = 24576;
        sh->sample_format               = AF_FORMAT_IEC61937_LE;
        sh->samplerate                  = 192000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_MP3:
        spdif_ctx->iec61937_packet_size = 4608;
        sh->sample_format               = AF_FORMAT_MPEG2;
        sh->samplerate                  = 48000;
        num_channels                    = 2;
        break;
    case AV_CODEC_ID_TRUEHD:
        spdif_ctx->iec61937_packet_size = 61440;
        sh->sample_format               = AF_FORMAT_IEC61937_LE;
        sh->samplerate                  = 192000;
        num_channels                    = 8;
        break;
    default:
        abort();
    }
    if (num_channels)
        mp_chmap_from_channels(&sh->channels, num_channels);

    if (avformat_write_header(lavf_ctx, &format_opts) < 0) {
        mp_msg(MSGT_DECAUDIO, MSGL_FATAL,
               "libavformat spdif initialization failed.\n");
        av_dict_free(&format_opts);
        goto fail;
    }
    av_dict_free(&format_opts);

    spdif_ctx->need_close = true;

    return 1;

fail:
    uninit(sh);
    return 0;
}

static int decode_audio(sh_audio_t *sh, struct mp_audio *buffer, int maxlen)
{
    struct spdifContext *spdif_ctx = sh->context;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;

    int sstride = 2 * sh->channels.num;
    assert(sstride == buffer->sstride);

    if (maxlen < spdif_ctx->iec61937_packet_size)
        return 0;

    spdif_ctx->out_buffer_len  = 0;
    spdif_ctx->out_buffer_size = maxlen;
    spdif_ctx->out_buffer      = buffer->planes[0];

    struct demux_packet *mpkt = demux_read_packet(sh->gsh);
    if (!mpkt)
        return 0;

    AVPacket pkt;
    mp_set_av_packet(&pkt, mpkt);
    pkt.pts = pkt.dts = 0;
    mp_msg(MSGT_DECAUDIO, MSGL_V, "spdif packet, size=%d\n", pkt.size);
    if (mpkt->pts != MP_NOPTS_VALUE) {
        sh->pts        = mpkt->pts;
        sh->pts_offset = 0;
    }
    int out_len = spdif_ctx->out_buffer_len;
    int ret = av_write_frame(lavf_ctx, &pkt);
    avio_flush(lavf_ctx->pb);
    sh->pts_offset += (spdif_ctx->out_buffer_len - out_len) / sstride;
    talloc_free(mpkt);
    if (ret < 0)
        return -1;

    buffer->samples = spdif_ctx->out_buffer_len / sstride;
    return 0;
}

static int control(sh_audio_t *sh, int cmd, void *arg)
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
    .preinit = preinit,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode_audio = decode_audio,
};
