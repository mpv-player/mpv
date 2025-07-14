/*
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

#include <stdlib.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/common.h>
#include <libavutil/opt.h>

#include "mpv_talloc.h"
#include "common/msg.h"
#include "common/av_common.h"
#include "demux/stheader.h"
#include "misc/bstr.h"
#include "sd.h"

struct lavc_conv {
    struct mp_log *log;
    struct mp_subtitle_opts *opts;
    bool styled;
    AVCodecContext *avctx;
    AVPacket *avpkt;
    AVPacket *avpkt_vtt;
    const char *codec;
    char *extradata;
    AVSubtitle cur;
    char **cur_list;
};

static const char *get_lavc_format(const char *format)
{
    // For the hack involving parse_webvtt().
    if (format && strcmp(format, "webvtt-webm") == 0)
        format = "webvtt";
    // Most text subtitles are srt/html style anyway.
    if (format && strcmp(format, "text") == 0)
        format = "subrip";
    return format;
}

struct lavc_conv *lavc_conv_create(struct sd *sd)
{
    struct lavc_conv *priv = talloc_zero(NULL, struct lavc_conv);
    priv->log = sd->log;
    priv->opts = sd->opts;
    priv->cur_list = talloc_array(priv, char*, 0);
    priv->codec = sd->codec->codec;
    AVCodecContext *avctx = NULL;
    AVDictionary *opts = NULL;
    const char *fmt = get_lavc_format(priv->codec);
    const AVCodec *codec = avcodec_find_decoder(mp_codec_to_av_codec_id(fmt));
    if (!codec)
        goto error;
    avctx = avcodec_alloc_context3(codec);
    if (!avctx)
        goto error;
    if (mp_set_avctx_codec_headers(avctx, sd->codec) < 0)
        goto error;

    MP_VERBOSE(sd, "Using subtitle decoder %s\n", codec->name);

    priv->avpkt = av_packet_alloc();
    priv->avpkt_vtt = av_packet_alloc();
    if (!priv->avpkt || !priv->avpkt_vtt)
        goto error;

    switch (codec->id) {
    case AV_CODEC_ID_DVB_TELETEXT:
        av_dict_set_int(&opts, "txt_format", 2, 0);
        break;
    case AV_CODEC_ID_ARIB_CAPTION:
        av_dict_set_int(&opts, "sub_type", SUBTITLE_ASS, 0);
        break;
    }

    av_dict_set(&opts, "sub_text_format", "ass", 0);
    av_dict_set(&opts, "flags2", "+ass_ro_flush_noop", 0);
    if (strcmp(priv->codec, "eia_608") == 0)
        av_dict_set(&opts, "real_time", "1", 0);
    if (avcodec_open2(avctx, codec, &opts) < 0)
        goto error;
    av_dict_free(&opts);
    // Documented as "set by libavcodec", but there is no other way
    avctx->time_base = (AVRational) {1, 1000};
    avctx->pkt_timebase = avctx->time_base;
    avctx->sub_charenc_mode = FF_SUB_CHARENC_MODE_IGNORE;
    priv->avctx = avctx;
    priv->extradata = talloc_strndup(priv, avctx->subtitle_header,
                                     avctx->subtitle_header_size);
    mp_codec_info_from_av(avctx, sd->codec);
    // Keep original codec name, which get_lavc_format() may have transformed
    sd->codec->codec = priv->codec;
    return priv;

 error:
    MP_FATAL(priv, "Could not open libavcodec subtitle converter\n");
    av_dict_free(&opts);
    avcodec_free_context(&avctx);
    mp_free_av_packet(&priv->avpkt);
    mp_free_av_packet(&priv->avpkt_vtt);
    talloc_free(priv);
    return NULL;
}

char *lavc_conv_get_extradata(struct lavc_conv *priv)
{
    return priv->extradata;
}

// FFmpeg WebVTT packets are pre-parsed in some way. The FFmpeg Matroska
// demuxer does this on its own. In order to free our demuxer_mkv.c from
// codec-specific crud, we do this here.
// Copied from libavformat/matroskadec.c (FFmpeg 818ebe9 / 2013-08-19)
// License: LGPL v2.1 or later
// Author header: The FFmpeg Project
// Modified in some ways.
static int parse_webvtt(AVPacket *in, AVPacket *pkt)
{
    uint8_t *id, *settings, *text, *buf;
    int id_len, settings_len, text_len;
    uint8_t *p, *q;
    int err;

    uint8_t *data = in->data;
    int data_len = in->size;

    if (data_len <= 0)
        return AVERROR_INVALIDDATA;

    p = data;
    q = data + data_len;

    id = p;
    id_len = -1;
    while (p < q) {
        if (*p == '\r' || *p == '\n') {
            id_len = p - id;
            if (*p == '\r')
                p++;
            break;
        }
        p++;
    }

    if (p >= q || *p != '\n')
        return AVERROR_INVALIDDATA;
    p++;

    settings = p;
    settings_len = -1;
    while (p < q) {
        if (*p == '\r' || *p == '\n') {
            settings_len = p - settings;
            if (*p == '\r')
                p++;
            break;
        }
        p++;
    }

    if (p >= q || *p != '\n')
        return AVERROR_INVALIDDATA;
    p++;

    text = p;
    text_len = q - p;
    while (text_len > 0) {
        const int len = text_len - 1;
        const uint8_t c = p[len];
        if (c != '\r' && c != '\n')
            break;
        text_len = len;
    }

    if (text_len <= 0)
        return AVERROR_INVALIDDATA;

    err = av_new_packet(pkt, text_len);
    if (err < 0)
        return AVERROR(err);

    memcpy(pkt->data, text, text_len);

    if (id_len > 0) {
        buf = av_packet_new_side_data(pkt,
                                      AV_PKT_DATA_WEBVTT_IDENTIFIER,
                                      id_len);
        if (buf == NULL) {
            av_packet_unref(pkt);
            return AVERROR(ENOMEM);
        }
        memcpy(buf, id, id_len);
    }

    if (settings_len > 0) {
        buf = av_packet_new_side_data(pkt,
                                      AV_PKT_DATA_WEBVTT_SETTINGS,
                                      settings_len);
        if (buf == NULL) {
            av_packet_unref(pkt);
            return AVERROR(ENOMEM);
        }
        memcpy(buf, settings, settings_len);
    }

    pkt->pts = in->pts;
    pkt->duration = in->duration;
    return 0;
}

// Return a NULL-terminated list of ASS event lines and have
// the AVSubtitle display PTS and duration set to input
// double variables.
char **lavc_conv_decode(struct lavc_conv *priv, struct demux_packet *packet,
                        double *sub_pts, double *sub_duration)
{
    AVCodecContext *avctx = priv->avctx;
    AVPacket *curr_pkt = priv->avpkt;
    int ret, got_sub;
    int num_cur = 0;

    avsubtitle_free(&priv->cur);

    mp_set_av_packet(priv->avpkt, packet, &avctx->time_base);
    if (priv->avpkt->pts < 0)
        priv->avpkt->pts = 0;

    if (strcmp(priv->codec, "webvtt-webm") == 0) {
        if (parse_webvtt(priv->avpkt, priv->avpkt_vtt) < 0) {
            MP_ERR(priv, "Error parsing subtitle\n");
            goto done;
        }
        curr_pkt = priv->avpkt_vtt;
    }

    priv->styled = avctx->codec_id == AV_CODEC_ID_DVB_TELETEXT;

    if (avctx->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
        if (!priv->opts->teletext_page) {
            av_opt_set(avctx, "txt_page", "subtitle", AV_OPT_SEARCH_CHILDREN);
            priv->styled = false;
        } else if (priv->opts->teletext_page == -1) {
            av_opt_set(avctx, "txt_page", "*", AV_OPT_SEARCH_CHILDREN);
        } else {
            char page[4];
            snprintf(page, sizeof(page), "%d", priv->opts->teletext_page);
            av_opt_set(avctx, "txt_page", page, AV_OPT_SEARCH_CHILDREN);
        }
    }

    ret = avcodec_decode_subtitle2(avctx, &priv->cur, &got_sub, curr_pkt);
    if (ret < 0) {
        MP_ERR(priv, "Error decoding subtitle\n");
    } else if (got_sub) {
        *sub_pts = packet->pts + mp_pts_from_av(priv->cur.start_display_time,
                                               &avctx->time_base);
        *sub_duration = priv->cur.end_display_time == UINT32_MAX ?
                        UINT32_MAX :
                        mp_pts_from_av(priv->cur.end_display_time -
                                       priv->cur.start_display_time,
                                       &avctx->time_base);

        for (int i = 0; i < priv->cur.num_rects; i++) {
            if (priv->cur.rects[i]->w > 0 && priv->cur.rects[i]->h > 0)
                MP_WARN(priv, "Ignoring bitmap subtitle.\n");
            char *ass_line = priv->cur.rects[i]->ass;
            if (!ass_line)
                continue;
            MP_TARRAY_APPEND(priv, priv->cur_list, num_cur, ass_line);
        }
    }

done:
    av_packet_unref(priv->avpkt_vtt);
    MP_TARRAY_APPEND(priv, priv->cur_list, num_cur, NULL);
    return priv->cur_list;
}

bool lavc_conv_is_styled(struct lavc_conv *priv)
{
    return priv->styled;
}

void lavc_conv_reset(struct lavc_conv *priv)
{
    avcodec_flush_buffers(priv->avctx);
}

void lavc_conv_uninit(struct lavc_conv *priv)
{
    avsubtitle_free(&priv->cur);
    avcodec_free_context(&priv->avctx);
    mp_free_av_packet(&priv->avpkt);
    mp_free_av_packet(&priv->avpkt_vtt);
    talloc_free(priv);
}
