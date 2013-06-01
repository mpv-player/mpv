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

#include <stdlib.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/common.h>

#include "talloc.h"
#include "core/mp_msg.h"
#include "core/av_common.h"
#include "sd.h"

struct sd_lavc_priv {
    AVCodecContext *avctx;
};

static bool supports_format(const char *format)
{
    enum AVCodecID cid = mp_codec_to_av_codec_id(format);
    const AVCodecDescriptor *desc = avcodec_descriptor_get(cid);
    // These are documented to support AVSubtitleRect->ass.
    return desc && (desc->props & AV_CODEC_PROP_TEXT_SUB);
}

static int init(struct sd *sd)
{
    struct sd_lavc_priv *priv = talloc_zero(NULL, struct sd_lavc_priv);
    AVCodecContext *avctx = NULL;
    AVCodec *codec = avcodec_find_decoder(mp_codec_to_av_codec_id(sd->codec));
    if (!codec)
        goto error;
    avctx = avcodec_alloc_context3(codec);
    if (!avctx)
        goto error;
    avctx->extradata_size = sd->extradata_len;
    avctx->extradata = sd->extradata;
    if (avcodec_open2(avctx, codec, NULL) < 0)
        goto error;
    // Documented as "set by libavcodec", but there is no other way
    avctx->time_base = (AVRational) {1, 1000};
    priv->avctx = avctx;
    sd->priv = priv;
    sd->output_codec = "ass";
    sd->output_extradata = avctx->subtitle_header;
    sd->output_extradata_len = avctx->subtitle_header_size;
    return 0;

 error:
    mp_msg(MSGT_SUBREADER, MSGL_ERR,
           "Could not open libavcodec subtitle converter\n");
    av_free(avctx);
    talloc_free(priv);
    return -1;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct sd_lavc_priv *priv = sd->priv;
    AVCodecContext *avctx = priv->avctx;
    double ts = av_q2d(av_inv_q(avctx->time_base));
    AVSubtitle sub = {0};
    AVPacket pkt;
    int ret, got_sub;

    av_init_packet(&pkt);
    pkt.data = packet->buffer;
    pkt.size = packet->len;
    pkt.pts = packet->pts == MP_NOPTS_VALUE ? AV_NOPTS_VALUE : packet->pts * ts;
    pkt.duration = packet->duration * ts;

    ret = avcodec_decode_subtitle2(avctx, &sub, &got_sub, &pkt);
    if (ret < 0) {
        mp_msg(MSGT_OSD, MSGL_ERR, "Error decoding subtitle\n");
    } else if (got_sub) {
        for (int i = 0; i < sub.num_rects; i++) {
            char *ass_line = sub.rects[i]->ass;
            if (!ass_line)
                break;
            // This might contain embedded timestamps, using the "old" ffmpeg
            // ASS packet format, in which case pts/duration might be ignored
            // at a later point.
            sd_conv_add_packet(sd, ass_line, strlen(ass_line),
                               packet->pts, packet->duration);
        }
    }

    av_free_packet(&pkt);
    avsubtitle_free(&sub);
}

static void reset(struct sd *sd)
{
    struct sd_lavc_priv *priv = sd->priv;

    avcodec_flush_buffers(priv->avctx);
    sd_conv_def_reset(sd);
}

static void uninit(struct sd *sd)
{
    struct sd_lavc_priv *priv = sd->priv;

    avcodec_close(priv->avctx);
    av_free(priv->avctx);
    talloc_free(priv);
}

const struct sd_functions sd_lavc_conv = {
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_converted = sd_conv_def_get_converted,
    .reset = reset,
    .uninit = uninit,
};
