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

#include <assert.h>
#include <math.h>
#include <limits.h>

#include <libavutil/common.h>
#include <libavutil/log.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/cpu.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "demux/packet.h"
#include "demux/stheader.h"
#include "misc/bstr.h"
#include "video/fmt-conversion.h"
#include "av_common.h"
#include "codecs.h"

int mp_lavc_set_extradata(AVCodecContext *avctx, void *ptr, int size)
{
    if (size) {
        av_free(avctx->extradata);
        avctx->extradata_size = 0;
        avctx->extradata = av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!avctx->extradata)
            return -1;
        avctx->extradata_size = size;
        memcpy(avctx->extradata, ptr, size);
    }
    return 0;
}

enum AVMediaType mp_to_av_stream_type(int type)
{
    switch (type) {
    case STREAM_VIDEO: return AVMEDIA_TYPE_VIDEO;
    case STREAM_AUDIO: return AVMEDIA_TYPE_AUDIO;
    case STREAM_SUB:   return AVMEDIA_TYPE_SUBTITLE;
    default:           return AVMEDIA_TYPE_UNKNOWN;
    }
}

AVCodecParameters *mp_codec_params_to_av(struct mp_codec_params *c)
{
    AVCodecParameters *avp = avcodec_parameters_alloc();
    if (!avp)
        return NULL;

    // If we have lavf demuxer params, they overwrite by definition any others.
    if (c->lav_codecpar) {
        if (avcodec_parameters_copy(avp, c->lav_codecpar) < 0)
            goto error;
        return avp;
    }

    avp->codec_type = mp_to_av_stream_type(c->type);
    avp->codec_id = mp_codec_to_av_codec_id(c->codec);
    avp->codec_tag = c->codec_tag;
    if (c->extradata_size) {
        uint8_t *extradata = c->extradata;
        int size = c->extradata_size;

        if (avp->codec_id == AV_CODEC_ID_FLAC) {
            // ffmpeg expects FLAC extradata to be just the STREAMINFO,
            // so grab only that (and assume it'll be the first block)
            if (size >= 8 && !memcmp(c->extradata, "fLaC", 4)) {
                extradata += 8;
                size = MPMIN(34, size - 8); // FLAC_STREAMINFO_SIZE
            }
        }

        avp->extradata = av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!avp->extradata)
            goto error;
        avp->extradata_size = size;
        memcpy(avp->extradata, extradata, size);
    }
    avp->bits_per_coded_sample = c->bits_per_coded_sample;

    // Video only
    avp->width = c->disp_w;
    avp->height = c->disp_h;

    // Audio only
    avp->sample_rate = c->samplerate;
    avp->bit_rate = c->bitrate;
    avp->block_align = c->block_align;
    avp->channels = c->channels.num;
    if (!mp_chmap_is_unknown(&c->channels))
        avp->channel_layout = mp_chmap_to_lavc(&c->channels);

    return avp;
error:
    avcodec_parameters_free(&avp);
    return NULL;
}

// Set avctx codec headers for decoding. Returns <0 on failure.
int mp_set_avctx_codec_headers(AVCodecContext *avctx, struct mp_codec_params *c)
{
    enum AVMediaType codec_type = avctx->codec_type;
    enum AVCodecID codec_id = avctx->codec_id;
    AVCodecParameters *avp = mp_codec_params_to_av(c);
    if (!avp)
        return -1;

    int r = avcodec_parameters_to_context(avctx, avp) < 0 ? -1 : 0;
    avcodec_parameters_free(&avp);

    if (avctx->codec_type != AVMEDIA_TYPE_UNKNOWN)
        avctx->codec_type = codec_type;
    if (avctx->codec_id != AV_CODEC_ID_NONE)
        avctx->codec_id = codec_id;
    return r;
}

// Pick a "good" timebase, which will be used to convert double timestamps
// back to fractions for passing them through libavcodec.
AVRational mp_get_codec_timebase(struct mp_codec_params *c)
{
    AVRational tb = {c->native_tb_num, c->native_tb_den};
    if (tb.num < 1 || tb.den < 1) {
        if (c->reliable_fps)
            tb = av_inv_q(av_d2q(c->fps, 1000000));
        if (tb.num < 1 || tb.den < 1)
            tb = AV_TIME_BASE_Q;
    }

    // If the timebase is too coarse, raise its precision, or small adjustments
    // to timestamps done between decoder and demuxer could be lost.
    if (av_q2d(tb) > 0.001) {
        AVRational r = av_div_q(tb, (AVRational){1, 1000});
        tb.den *= (r.num + r.den - 1) / r.den;
    }

    av_reduce(&tb.num, &tb.den, tb.num, tb.den, INT_MAX);

    if (tb.num < 1 || tb.den < 1)
        tb = AV_TIME_BASE_Q;

    return tb;
}

static AVRational get_def_tb(AVRational *tb)
{
    return tb && tb->num > 0 && tb->den > 0 ? *tb : AV_TIME_BASE_Q;
}

// Convert the mpv style timestamp (seconds as double) to a libavcodec style
// timestamp (integer units in a given timebase).
int64_t mp_pts_to_av(double mp_pts, AVRational *tb)
{
    AVRational b = get_def_tb(tb);
    return mp_pts == MP_NOPTS_VALUE ? AV_NOPTS_VALUE : llrint(mp_pts / av_q2d(b));
}

// Inverse of mp_pts_to_av(). (The timebases must be exactly the same.)
double mp_pts_from_av(int64_t av_pts, AVRational *tb)
{
    AVRational b = get_def_tb(tb);
    return av_pts == AV_NOPTS_VALUE ? MP_NOPTS_VALUE : av_pts * av_q2d(b);
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
        dst->flags |= mpkt->avpacket->flags;
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

static void add_codecs(struct mp_decoder_list *list, enum AVMediaType type,
                       bool decoders)
{
    void *iter = NULL;
    for (;;) {
        const AVCodec *cur = av_codec_iterate(&iter);
        if (!cur)
            break;
        if (av_codec_is_decoder(cur) == decoders &&
            (type == AVMEDIA_TYPE_UNKNOWN || cur->type == type))
        {
            mp_add_decoder(list, mp_codec_from_av_codec_id(cur->id),
                           cur->name, cur->long_name);
        }
    }
}

void mp_add_lavc_decoders(struct mp_decoder_list *list, enum AVMediaType type)
{
    add_codecs(list, type, true);
}

// (Abuses the decoder list data structures.)
void mp_add_lavc_encoders(struct mp_decoder_list *list)
{
    add_codecs(list, AVMEDIA_TYPE_UNKNOWN, false);
}

char **mp_get_lavf_demuxers(void)
{
    char **list = NULL;
    void *iter = NULL;
    int num = 0;
    for (;;) {
        const AVInputFormat *cur = av_demuxer_iterate(&iter);
        if (!cur)
            break;
        MP_TARRAY_APPEND(NULL, list, num, talloc_strdup(NULL, cur->name));
    }
    MP_TARRAY_APPEND(NULL, list, num, NULL);
    return list;
}

int mp_codec_to_av_codec_id(const char *codec)
{
    int id = AV_CODEC_ID_NONE;
    if (codec) {
        const AVCodecDescriptor *desc = avcodec_descriptor_get_by_name(codec);
        if (desc)
            id = desc->id;
        if (id == AV_CODEC_ID_NONE) {
            const AVCodec *avcodec = avcodec_find_decoder_by_name(codec);
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
        const AVCodec *avcodec = avcodec_find_decoder(codec_id);
        if (avcodec)
            name = avcodec->name;
    }
    return name;
}

bool mp_codec_is_lossless(const char *codec)
{
    const AVCodecDescriptor *desc =
        avcodec_descriptor_get(mp_codec_to_av_codec_id(codec));
    return desc && (desc->props & AV_CODEC_PROP_LOSSLESS);
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

// If the name starts with "@", try to interpret it as a number, and set *name
// to the name of the n-th parameter.
static void resolve_positional_arg(void *avobj, char **name)
{
    if (!*name || (*name)[0] != '@' || !avobj)
        return;

    char *end = NULL;
    int pos = strtol(*name + 1, &end, 10);
    if (!end || *end)
        return;

    const AVOption *opt = NULL;
    int offset = -1;
    while (1) {
        opt = av_opt_next(avobj, opt);
        if (!opt)
            return;
        // This is what libavfilter's parser does to skip aliases.
        if (opt->offset != offset && opt->type != AV_OPT_TYPE_CONST)
            pos--;
        if (pos < 0) {
            *name = (char *)opt->name;
            return;
        }
        offset = opt->offset;
    }
}

// kv is in the format as by OPT_KEYVALUELIST(): kv[0]=key0, kv[1]=val0, ...
// Set these options on given avobj (using av_opt_set..., meaning avobj must
// point to a struct that has AVClass as first member).
// Options which fail to set (error or not found) are printed to log.
// Returns: >=0 success, <0 failed to set an option
int mp_set_avopts(struct mp_log *log, void *avobj, char **kv)
{
    return mp_set_avopts_pos(log, avobj, avobj, kv);
}

// Like mp_set_avopts(), but the posargs argument is used to resolve positional
// arguments. If posargs==NULL, positional args are disabled.
int mp_set_avopts_pos(struct mp_log *log, void *avobj, void *posargs, char **kv)
{
    int success = 0;
    for (int n = 0; kv && kv[n * 2]; n++) {
        char *k = kv[n * 2 + 0];
        char *v = kv[n * 2 + 1];
        resolve_positional_arg(posargs, &k);
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
