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
#include <libavutil/log.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/cpu.h>
#include <libavcodec/avcodec.h>

#include "common/common.h"
#include "common/msg.h"
#include "demux/packet.h"
#include "av_common.h"
#include "codecs.h"

int mp_lavc_set_extradata(AVCodecContext *avctx, void *ptr, int size)
{
    if (size) {
        av_free(avctx->extradata);
        avctx->extradata_size = 0;
        avctx->extradata = av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
        if (!avctx->extradata)
            return -1;
        avctx->extradata_size = size;
        memcpy(avctx->extradata, ptr, size);
    }
    return 0;
}

// Copy the codec-related fields from st into avctx. This does not set the
// codec itself, only codec related header data provided by libavformat.
// The goal is to initialize a new decoder with the header data provided by
// libavformat, and unlike avcodec_copy_context(), allow the user to create
// a clean AVCodecContext for a manually selected AVCodec.
// This is strictly for decoding only.
void mp_copy_lav_codec_headers(AVCodecContext *avctx, AVCodecContext *st)
{
    mp_lavc_set_extradata(avctx, st->extradata, st->extradata_size);
    avctx->codec_tag                = st->codec_tag;
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
    avctx->bits_per_coded_sample    = st->bits_per_coded_sample;
    // Required in FFmpeg 2.5.x / Libav 11, deprecated afterwards.
    avctx->stream_codec_tag         = st->stream_codec_tag;
}

// We merely pass-through our PTS/DTS as an int64_t; libavcodec won't use it.
union pts { int64_t i; double d; };

// Convert the mpv style timestamp (seconds as double) to a libavcodec style
// timestamp (integer units in a given timebase).
//
// If the given timebase is NULL or invalid, pass through the mpv timestamp by
// reinterpret casting them to int64_t. In this case, the timestamps will be
// non-sense for libavcodec, but we expect that it doesn't interpret them,
// and treats them as opaque.
int64_t mp_pts_to_av(double mp_pts, AVRational *tb)
{
    assert(sizeof(int64_t) >= sizeof(double));
    if (tb && tb->num > 0 && tb->den > 0)
        return mp_pts == MP_NOPTS_VALUE ? AV_NOPTS_VALUE : mp_pts / av_q2d(*tb);
    // The + 0.0 is to squash possible negative zero mp_pts, which would
    // happen to end up as AV_NOPTS_VALUE.
    return (union pts){.d = mp_pts + 0.0}.i;
}

// Inverse of mp_pts_to_av(). (The timebases must be exactly the same.)
double mp_pts_from_av(int64_t av_pts, AVRational *tb)
{
    assert(sizeof(int64_t) >= sizeof(double));
    if (tb && tb->num > 0 && tb->den > 0)
        return av_pts == AV_NOPTS_VALUE ? MP_NOPTS_VALUE : av_pts * av_q2d(*tb);
    // Should libavcodec set the PTS to AV_NOPTS_VALUE, it would end up as
    // non-sense (usually negative zero) when unwrapped to double.
    return av_pts == AV_NOPTS_VALUE ? MP_NOPTS_VALUE : (union pts){.i = av_pts}.d;
}

// Set dst from mpkt. Note that dst is not refcountable.
// mpkt can be NULL to generate empty packets (used to flush delayed data).
// Sets pts/dts using mp_pts_to_av(ts, tb). (Be aware of the implications.)
// Set duration field only if tb is set.
void mp_set_av_packet(AVPacket *dst, struct demux_packet *mpkt, AVRational *tb)
{
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
        if (dst->data == mpkt->avpacket->data)
            dst->buf = mpkt->avpacket->buf;
    }
    if (mpkt && tb && tb->num > 0 && tb->den > 0)
        dst->duration = mpkt->duration / av_q2d(*tb);
    dst->pts = mp_pts_to_av(mpkt ? mpkt->pts : MP_NOPTS_VALUE, tb);
    dst->dts = mp_pts_to_av(mpkt ? mpkt->dts : MP_NOPTS_VALUE, tb);
}

void mp_set_avcodec_threads(struct mp_log *l, AVCodecContext *avctx, int threads)
{
    if (threads == 0) {
        threads = av_cpu_count();
        if (threads < 1) {
            mp_warn(l, "Could not determine thread count to use, defaulting to 1.\n");
            threads = 1;
        } else {
            mp_verbose(l, "Detected %d logical cores.\n", threads);
            if (threads > 1)
                threads += 1; // extra thread for better load balancing
        }
        // Apparently some libavcodec versions have or had trouble with more
        // than 16 threads, and/or print a warning when using > 16.
        threads = MPMIN(threads, 16);
    }
    mp_verbose(l, "Requesting %d threads for decoding.\n", threads);
    avctx->thread_count = threads;
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

// kv is in the format as by OPT_KEYVALUELIST(): kv[0]=key0, kv[1]=val0, ...
// Copy them to the dict.
void mp_set_avdict(AVDictionary **dict, char **kv)
{
    for (int n = 0; kv && kv[n * 2]; n++)
        av_dict_set(dict, kv[n * 2 + 0], kv[n * 2 + 1], 0);
}

// For use with libav* APIs that take AVDictionaries of options.
// Print options remaining in the dict as unset.
void mp_avdict_print_unset(struct mp_log *log, int msgl, AVDictionary *dict)
{
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(dict, "", t, AV_DICT_IGNORE_SUFFIX)))
        mp_msg(log, msgl, "Could not set AVOption %s='%s'\n", t->key, t->value);
}

// kv is in the format as by OPT_KEYVALUELIST(): kv[0]=key0, kv[1]=val0, ...
// Set these options on given avobj (using av_opt_set..., meaning avobj must
// point to a struct that has AVClass as first member).
// Options which fail to set (error or not found) are printed to log.
// Returns: >=0 success, <0 failed to set an option
int mp_set_avopts(struct mp_log *log, void *avobj, char **kv)
{
    int success = 0;
    for (int n = 0; kv && kv[n * 2]; n++) {
        char *k = kv[n * 2 + 0];
        char *v = kv[n * 2 + 1];
        int r = av_opt_set(avobj, k, v, AV_OPT_SEARCH_CHILDREN);
        if (r == AVERROR_OPTION_NOT_FOUND) {
            mp_err(log, "AVOption '%s' not found.\n", k);
            success = -1;
        } else if (r < 0) {
            char errstr[80];
            av_strerror(r, errstr, sizeof(errstr));
            mp_err(log, "Could not set AVOption %s='%s' (%s)\n", k, v, errstr);
            success = -1;
        }
    }
    return success;
}
