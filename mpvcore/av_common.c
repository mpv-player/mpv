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

#include <assert.h>

#include <libavutil/common.h>
#include <libavcodec/avcodec.h>

#include "mpvcore/mp_common.h"
#include "demux/packet.h"
#include "av_common.h"
#include "codecs.h"


// Copy the codec-related fields from st into avctx. This does not set the
// codec itself, only codec related header data provided by libavformat.
// The goal is to initialize a new decoder with the header data provided by
// libavformat, and unlike avcodec_copy_context(), allow the user to create
// a clean AVCodecContext for a manually selected AVCodec.
// This is strictly for decoding only.
void mp_copy_lav_codec_headers(AVCodecContext *avctx, AVCodecContext *st)
{
    if (st->extradata_size) {
        av_free(avctx->extradata);
        avctx->extradata_size = 0;
        avctx->extradata =
            av_mallocz(st->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
        if (avctx->extradata) {
            avctx->extradata_size = st->extradata_size;
            memcpy(avctx->extradata, st->extradata, st->extradata_size);
        }
    }
    avctx->codec_tag                = st->codec_tag;
    avctx->stream_codec_tag         = st->stream_codec_tag;
    avctx->bit_rate                 = st->bit_rate;
    avctx->width                    = st->width;
    avctx->height                   = st->height;
    avctx->pix_fmt                  = st->pix_fmt;
    avctx->sample_aspect_ratio      = st->sample_aspect_ratio;
    avctx->chroma_sample_location   = st->chroma_sample_location;
    avctx->sample_rate              = st->sample_rate;
    avctx->channels                 = st->channels;
    avctx->block_align              = st->block_align;
    avctx->channel_layout           = st->channel_layout;
    avctx->audio_service_type       = st->audio_service_type;
    avctx->bits_per_coded_sample    = st->bits_per_coded_sample;
}

// We merely pass-through our PTS/DTS as an int64_t; libavcodec won't use it.
union pts { int64_t i; double d; };

// Set dst from mpkt. Note that dst is not refcountable.
// mpkt can be NULL to generate empty packets (used to flush delayed data).
// Sets pts/dts to reinterpret-casted mpv values - these are useful for
// reading back via mp_get_av_frame_okt_pdts(), but they will be non-sense for
// libavcodec itself. (And normally, libavcodec won't interpret them.)
// Does not set duration field.
void mp_set_av_packet(AVPacket *dst, struct demux_packet *mpkt)
{
    assert(sizeof(int64_t) >= sizeof(double));

    av_init_packet(dst);
    dst->data = mpkt ? mpkt->buffer : NULL;
    dst->size = mpkt ? mpkt->len : 0;
    /* Some codecs (ZeroCodec, some cases of PNG) may want keyframe info
     * from demuxer. */
    if (mpkt && mpkt->keyframe)
        dst->flags |= AV_PKT_FLAG_KEY;
    if (mpkt && mpkt->avpacket) {
        dst->side_data = mpkt->avpacket->side_data;
        dst->side_data_elems = mpkt->avpacket->side_data_elems;
    }
    dst->pts = (union pts){.d = mpkt ? mpkt->pts : MP_NOPTS_VALUE}.i;
    dst->dts = (union pts){.d = mpkt ? mpkt->dts : MP_NOPTS_VALUE}.i;
}

// Return the pts/dts from a frame returned by libavcodec. Note that this
// assumes libavcodec was fed a packet setup with mp_set_av_packet()! If not,
// the timestamps might contain garbage.
// Normally, this returns the pts. If the pts is unknown, return dts instead.
double mp_get_av_frame_pkt_pdts(AVFrame *frame)
{
    double pts = (union pts){.i = frame->pkt_pts}.d;
    double dts = (union pts){.i = frame->pkt_dts}.d;
    return pts == MP_NOPTS_VALUE ? dts : pts;
}

void mp_add_lavc_decoders(struct mp_decoder_list *list, enum AVMediaType type)
{
    AVCodec *cur = NULL;
    for (;;) {
        cur = av_codec_next(cur);
        if (!cur)
            break;
        if (av_codec_is_decoder(cur) && cur->type == type) {
            mp_add_decoder(list, "lavc", mp_codec_from_av_codec_id(cur->id),
                           cur->name, cur->long_name);
        }
    }
}

int mp_codec_to_av_codec_id(const char *codec)
{
    int id = AV_CODEC_ID_NONE;
    if (codec) {
        const AVCodecDescriptor *desc = avcodec_descriptor_get_by_name(codec);
        if (desc)
            id = desc->id;
        if (id == AV_CODEC_ID_NONE) {
            AVCodec *avcodec = avcodec_find_decoder_by_name(codec);
            if (avcodec)
                id = avcodec->id;
        }
    }
    return id;
}

const char *mp_codec_from_av_codec_id(int codec_id)
{
    const char *name = NULL;
    const AVCodecDescriptor *desc = avcodec_descriptor_get(codec_id);
    if (desc)
        name = desc->name;
    if (!name) {
        AVCodec *avcodec = avcodec_find_decoder(codec_id);
        if (avcodec)
            name = avcodec->name;
    }
    return name;
}
