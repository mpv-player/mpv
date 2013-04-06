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

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/av_common.h"
#include "core/options.h"
#include "ad_internal.h"

LIBAD_EXTERN(spdif)

#define FILENAME_SPDIFENC "spdif"
#define OUTBUF_SIZE 65536
struct spdifContext {
    AVFormatContext *lavf_ctx;
    int              iec61937_packet_size;
    int              out_buffer_len;
    int              out_buffer_size;
    uint8_t         *out_buffer;
    uint8_t          pb_buffer[OUTBUF_SIZE];
};

static int read_packet(void *p, uint8_t *buf, int buf_size)
{
    // spdifenc does not use read callback.
    return 0;
}

static int write_packet(void *p, uint8_t *buf, int buf_size)
{
    int len;
    struct spdifContext *ctx = p;

    len = FFMIN(buf_size, ctx->out_buffer_size -ctx->out_buffer_len);
    memcpy(&ctx->out_buffer[ctx->out_buffer_len], buf, len);
    ctx->out_buffer_len += len;
    return len;
}

static int64_t seek(void *p, int64_t offset, int whence)
{
    // spdifenc does not use seek callback.
    return 0;
}

static int preinit(sh_audio_t *sh)
{
    sh->samplesize = 2;
    return 1;
}

static int codecs[] = {
    AV_CODEC_ID_AAC,
    AV_CODEC_ID_AC3,
    AV_CODEC_ID_DTS,
    AV_CODEC_ID_EAC3,
    AV_CODEC_ID_MP3,
    AV_CODEC_ID_TRUEHD,
    AV_CODEC_ID_NONE
};

static int init(sh_audio_t *sh, const char *decoder)
{
    int x, in_size, srate, bps, *dtshd_rate;
    unsigned char *start;
    double pts;
    AVFormatContext     *lavf_ctx  = NULL;
    AVStream            *stream    = NULL;
    const AVOption      *opt       = NULL;
    struct spdifContext *spdif_ctx = NULL;

    spdif_ctx = av_mallocz(sizeof(*spdif_ctx));
    if (!spdif_ctx)
        goto fail;
    spdif_ctx->lavf_ctx = avformat_alloc_context();
    if (!spdif_ctx->lavf_ctx)
        goto fail;

    sh->context = spdif_ctx;
    lavf_ctx    = spdif_ctx->lavf_ctx;

    lavf_ctx->oformat = av_guess_format(FILENAME_SPDIFENC, NULL, NULL);
    if (!lavf_ctx->oformat)
        goto fail;
    lavf_ctx->priv_data = av_mallocz(lavf_ctx->oformat->priv_data_size);
    if (!lavf_ctx->priv_data)
        goto fail;
    lavf_ctx->pb = avio_alloc_context(spdif_ctx->pb_buffer, OUTBUF_SIZE, 1, spdif_ctx,
                            read_packet, write_packet, seek);
    if (!lavf_ctx->pb)
        goto fail;
    stream = avformat_new_stream(lavf_ctx, 0);
    if (!stream)
        goto fail;
    lavf_ctx->duration   = AV_NOPTS_VALUE;
    lavf_ctx->start_time = AV_NOPTS_VALUE;
    lavf_ctx->streams[0]->codec->codec_id = mp_codec_to_av_codec_id(decoder);
    lavf_ctx->raw_packet_buffer_remaining_size = RAW_PACKET_BUFFER_SIZE;
    if (AVERROR_PATCHWELCOME == lavf_ctx->oformat->write_header(lavf_ctx)) {
        mp_msg(MSGT_DECAUDIO,MSGL_INFO,
               "This codec is not supported by spdifenc.\n");
        goto fail;
    }

    // get sample_rate & bitrate from parser
    x = ds_get_packet_pts(sh->ds, &start, &pts);
    in_size = x;
    if (x <= 0) {
        pts = MP_NOPTS_VALUE;
        x = 0;
    }
    ds_parse(sh->ds, &start, &x, pts, 0);
    srate = 48000;    //fake value
    bps   = 768000/8; //fake value
    if (x && sh->avctx) { // we have parser and large enough buffer
        if (sh->avctx->sample_rate < 44100) {
            mp_msg(MSGT_DECAUDIO,MSGL_INFO,
                   "This stream sample_rate[%d Hz] may be broken. "
                   "Force reset 48000Hz.\n",
                   sh->avctx->sample_rate);
            srate = 48000; //fake value
        } else
            srate = sh->avctx->sample_rate;
        bps = sh->avctx->bit_rate/8;
    }
    sh->ds->buffer_pos -= in_size;

    int num_channels = 0;
    switch (lavf_ctx->streams[0]->codec->codec_id) {
    case AV_CODEC_ID_AAC:
        spdif_ctx->iec61937_packet_size = 16384;
        sh->sample_format               = AF_FORMAT_IEC61937_LE;
        sh->samplerate                  = srate;
        num_channels                    = 2;
        sh->i_bps                       = bps;
        break;
    case AV_CODEC_ID_AC3:
        spdif_ctx->iec61937_packet_size = 6144;
        sh->sample_format               = AF_FORMAT_AC3_LE;
        sh->samplerate                  = srate;
        num_channels                    = 2;
        sh->i_bps                       = bps;
        break;
    case AV_CODEC_ID_DTS:
        if(sh->opts->dtshd) {
            opt = av_opt_find(&lavf_ctx->oformat->priv_class,
                              "dtshd_rate", NULL, 0, 0);
            if (!opt)
                goto fail;
            dtshd_rate                      = (int*)(((uint8_t*)lavf_ctx->priv_data) +
                                              opt->offset);
            *dtshd_rate                     = 192000*4;
            spdif_ctx->iec61937_packet_size = 32768;
            sh->sample_format               = AF_FORMAT_IEC61937_LE;
            sh->samplerate                  = 192000; // DTS core require 48000
            num_channels                    = 2*4;
            sh->i_bps                       = bps;
        } else {
            spdif_ctx->iec61937_packet_size = 32768;
            sh->sample_format               = AF_FORMAT_AC3_LE;
            sh->samplerate                  = srate;
            num_channels                    = 2;
            sh->i_bps                       = bps;
        }
        break;
    case AV_CODEC_ID_EAC3:
        spdif_ctx->iec61937_packet_size = 24576;
        sh->sample_format               = AF_FORMAT_IEC61937_LE;
        sh->samplerate                  = 192000;
        num_channels                    = 2;
        sh->i_bps                       = bps;
        break;
    case AV_CODEC_ID_MP3:
        spdif_ctx->iec61937_packet_size = 4608;
        sh->sample_format               = AF_FORMAT_MPEG2;
        sh->samplerate                  = srate;
        num_channels                    = 2;
        sh->i_bps                       = bps;
        break;
    case AV_CODEC_ID_TRUEHD:
        spdif_ctx->iec61937_packet_size = 61440;
        sh->sample_format               = AF_FORMAT_IEC61937_LE;
        sh->samplerate                  = 192000;
        num_channels                    = 8;
        sh->i_bps                       = bps;
        break;
    default:
        break;
    }
    if (num_channels)
        mp_chmap_from_channels(&sh->channels, num_channels);

    return 1;

fail:
    uninit(sh);
    return 0;
}

static int decode_audio(sh_audio_t *sh, unsigned char *buf,
                        int minlen, int maxlen)
{
    struct spdifContext *spdif_ctx = sh->context;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;
    AVPacket            pkt;
    double              pts;
    int                 ret, in_size, consumed, x;
    unsigned char       *start = NULL;

    consumed = spdif_ctx->out_buffer_len  = 0;
    spdif_ctx->out_buffer_size = maxlen;
    spdif_ctx->out_buffer      = buf;
    while (spdif_ctx->out_buffer_len + spdif_ctx->iec61937_packet_size < maxlen
           && spdif_ctx->out_buffer_len < minlen) {
        if (sh->ds->eof)
            break;
        x = ds_get_packet_pts(sh->ds, &start, &pts);
        if (x <= 0) {
            x = 0;
            ds_parse(sh->ds, &start, &x, MP_NOPTS_VALUE, 0);
            if (x == 0)
                continue; // END_NOT_FOUND
            in_size = x;
        } else {
            in_size = x;
            consumed = ds_parse(sh->ds, &start, &x, pts, 0);
            if (x == 0) {
                mp_msg(MSGT_DECAUDIO,MSGL_V,
                       "start[%p] in_size[%d] consumed[%d] x[%d].\n",
                       start, in_size, consumed, x);
                continue; // END_NOT_FOUND
            }
            sh->ds->buffer_pos -= in_size - consumed;
        }
        av_init_packet(&pkt);
        pkt.data = start;
        pkt.size = x;
        mp_msg(MSGT_DECAUDIO,MSGL_V,
               "start[%p] pkt.size[%d] in_size[%d] consumed[%d] x[%d].\n",
               start, pkt.size, in_size, consumed, x);
        if (pts != MP_NOPTS_VALUE) {
            sh->pts       = pts;
            sh->pts_bytes = 0;
        }
        ret = lavf_ctx->oformat->write_packet(lavf_ctx, &pkt);
        if (ret < 0)
            break;
    }
    sh->pts_bytes += spdif_ctx->out_buffer_len;
    return spdif_ctx->out_buffer_len;
}

static int control(sh_audio_t *sh, int cmd, void* arg, ...)
{
    unsigned char *start;
    double pts;

    switch (cmd) {
    case ADCTRL_RESYNC_STREAM:
    case ADCTRL_SKIP_FRAME:
        ds_get_packet_pts(sh->ds, &start, &pts);
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(sh_audio_t *sh)
{
    struct spdifContext *spdif_ctx = sh->context;
    AVFormatContext     *lavf_ctx  = spdif_ctx->lavf_ctx;

    if (lavf_ctx) {
        if (lavf_ctx->oformat)
            lavf_ctx->oformat->write_trailer(lavf_ctx);
        av_freep(&lavf_ctx->pb);
        if (lavf_ctx->streams) {
            av_freep(&lavf_ctx->streams[0]->codec);
            av_freep(&lavf_ctx->streams[0]->info);
            av_freep(&lavf_ctx->streams[0]);
        }
        av_freep(&lavf_ctx->streams);
        av_freep(&lavf_ctx->priv_data);
    }
    av_freep(&lavf_ctx);
    av_freep(&spdif_ctx);
}

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
