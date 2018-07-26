/*
 * muxing using libavformat
 *
 * Copyright (C) 2010 Nicolas George <george@nsup.org>
 * Copyright (C) 2011-2012 Rudolf Polzer <divVerent@xonotic.org>
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

#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>

#include "config.h"
#include "encode_lavc.h"
#include "common/av_common.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/options.h"
#include "osdep/timer.h"
#include "video/out/vo.h"
#include "mpv_talloc.h"
#include "stream/stream.h"

struct encode_priv {
    struct mp_log *log;

    // --- All fields are protected by encode_lavc_context.lock

    bool failed;

    struct mp_tags *metadata;

    AVFormatContext *muxer;

    bool header_written; // muxer was initialized

    struct mux_stream **streams;
    int num_streams;

    // Statistics
    double t0;

    long long abytes;
    long long vbytes;

    unsigned int frames;
    double audioseconds;
};

struct mux_stream {
    int index;                      // index of this into p->streams[]
    char name[80];
    struct encode_lavc_context *ctx;
    enum AVMediaType codec_type;
    AVRational encoder_timebase;    // packet timestamps from encoder
    AVStream *st;
    void (*on_ready)(void *ctx);    // when finishing muxer init
    void *on_ready_ctx;
};

#define OPT_BASE_STRUCT struct encode_opts
const struct m_sub_options encode_config = {
    .opts = (const m_option_t[]) {
        OPT_STRING("o", file, M_OPT_FIXED | CONF_NOCFG | CONF_PRE_PARSE | M_OPT_FILE),
        OPT_STRING("of", format, M_OPT_FIXED),
        OPT_KEYVALUELIST("ofopts", fopts, M_OPT_FIXED | M_OPT_HAVE_HELP),
        OPT_STRING("ovc", vcodec, M_OPT_FIXED),
        OPT_KEYVALUELIST("ovcopts", vopts, M_OPT_FIXED | M_OPT_HAVE_HELP),
        OPT_STRING("oac", acodec, M_OPT_FIXED),
        OPT_KEYVALUELIST("oacopts", aopts, M_OPT_FIXED | M_OPT_HAVE_HELP),
        OPT_FLOATRANGE("ovoffset", voffset, M_OPT_FIXED, -1000000.0, 1000000.0,
                       .deprecation_message = "--audio-delay (once unbroken)"),
        OPT_FLOATRANGE("oaoffset", aoffset, M_OPT_FIXED, -1000000.0, 1000000.0,
                       .deprecation_message = "--audio-delay (once unbroken)"),
        OPT_FLAG("orawts", rawts, M_OPT_FIXED),
        OPT_FLAG("ovfirst", video_first, M_OPT_FIXED,
                 .deprecation_message = "no replacement"),
        OPT_FLAG("oafirst", audio_first, M_OPT_FIXED,
                 .deprecation_message = "no replacement"),
        OPT_FLAG("ocopy-metadata", copy_metadata, M_OPT_FIXED),
        OPT_KEYVALUELIST("oset-metadata", set_metadata, M_OPT_FIXED),
        OPT_STRINGLIST("oremove-metadata", remove_metadata, M_OPT_FIXED),

        OPT_REMOVED("ocopyts", "ocopyts is now the default"),
        OPT_REMOVED("oneverdrop", "no replacement"),
        OPT_REMOVED("oharddup", "use --vf-add=fps=VALUE"),
        OPT_REMOVED("ofps", "no replacement (use --vf-add=fps=VALUE for CFR)"),
        OPT_REMOVED("oautofps", "no replacement"),
        OPT_REMOVED("omaxfps", "no replacement"),
        {0}
    },
    .size = sizeof(struct encode_opts),
    .defaults = &(const struct encode_opts){
        .copy_metadata = 1,
    },
};

struct encode_lavc_context *encode_lavc_init(struct mpv_global *global)
{
    struct encode_lavc_context *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct encode_lavc_context){
        .global = global,
        .options = mp_get_config_group(ctx, global, &encode_config),
        .priv = talloc_zero(ctx, struct encode_priv),
        .log = mp_log_new(ctx, global->log, "encode"),
    };
    pthread_mutex_init(&ctx->lock, NULL);

    struct encode_priv *p = ctx->priv;
    p->log = ctx->log;

    const char *filename = ctx->options->file;

    // STUPID STUPID STUPID STUPID avio
    // does not support "-" as file name to mean stdin/stdout
    // ffmpeg.c works around this too, the same way
    if (!strcmp(filename, "-"))
        filename = "pipe:1";

    if (filename && (
            !strcmp(filename, "/dev/stdout") ||
            !strcmp(filename, "pipe:") ||
            !strcmp(filename, "pipe:1")))
        mp_msg_force_stderr(global, true);

    encode_lavc_discontinuity(ctx);

    p->muxer = avformat_alloc_context();
    MP_HANDLE_OOM(p->muxer);

    if (ctx->options->format && ctx->options->format[0]) {
        ctx->oformat = av_guess_format(ctx->options->format, filename, NULL);
    } else {
        ctx->oformat = av_guess_format(NULL, filename, NULL);
    }

    if (!ctx->oformat) {
        MP_FATAL(ctx, "format not found\n");
        goto fail;
    }

    p->muxer->oformat = ctx->oformat;

    p->muxer->url = av_strdup(filename);
    MP_HANDLE_OOM(p->muxer->url);

    return ctx;

fail:
    p->failed = true;
    encode_lavc_free(ctx);
    return NULL;
}

void encode_lavc_set_metadata(struct encode_lavc_context *ctx,
                              struct mp_tags *metadata)
{
    struct encode_priv *p = ctx->priv;

    pthread_mutex_lock(&ctx->lock);

    if (ctx->options->copy_metadata) {
        p->metadata = mp_tags_dup(ctx, metadata);
    } else {
        p->metadata = talloc_zero(ctx, struct mp_tags);
    }

    if (ctx->options->set_metadata) {
        char **kv = ctx->options->set_metadata;
        // Set all user-provided metadata tags
        for (int n = 0; kv[n * 2]; n++) {
            MP_VERBOSE(ctx, "setting metadata value '%s' for key '%s'\n",
                       kv[n*2 + 0], kv[n*2 +1]);
            mp_tags_set_str(p->metadata, kv[n*2 + 0], kv[n*2 +1]);
        }
    }

    if (ctx->options->remove_metadata) {
        char **k = ctx->options->remove_metadata;
        // Remove all user-provided metadata tags
        for (int n = 0; k[n]; n++) {
            MP_VERBOSE(ctx, "removing metadata key '%s'\n", k[n]);
            mp_tags_remove_str(p->metadata, k[n]);
        }
    }

    pthread_mutex_unlock(&ctx->lock);
}

bool encode_lavc_free(struct encode_lavc_context *ctx)
{
    bool res = true;
    if (!ctx)
        return res;

    struct encode_priv *p = ctx->priv;

    if (!p->failed && !p->header_written) {
        MP_FATAL(p, "no data written to target file\n");
        p->failed = true;
    }

    if (!p->failed && p->header_written) {
        if (av_write_trailer(p->muxer) < 0)
            MP_ERR(p, "error writing trailer\n");

        MP_INFO(p, "video: encoded %lld bytes\n", p->vbytes);
        MP_INFO(p, "audio: encoded %lld bytes\n", p->abytes);

        MP_INFO(p, "muxing overhead %lld bytes\n",
                (long long)(avio_size(p->muxer->pb) - p->vbytes - p->abytes));
    }

    if (avio_closep(&p->muxer->pb) < 0 && !p->failed) {
        MP_ERR(p, "Closing file failed\n");
        p->failed = true;
    }

    avformat_free_context(p->muxer);

    res = !p->failed;

    pthread_mutex_destroy(&ctx->lock);
    talloc_free(ctx);

    return res;
}

void encode_lavc_set_audio_pts(struct encode_lavc_context *ctx, double pts)
{
    if (ctx) {
        pthread_mutex_lock(&ctx->lock);
        ctx->last_audio_in_pts = pts;
        ctx->samples_since_last_pts = 0;
        pthread_mutex_unlock(&ctx->lock);
    }
}

// called locked
static void maybe_init_muxer(struct encode_lavc_context *ctx)
{
    struct encode_priv *p = ctx->priv;

    if (p->header_written || p->failed)
        return;

    // Check if all streams were initialized yet. We need data to know the
    // AVStream parameters, so we wait for data from _all_ streams before
    // starting.
    for (int n = 0; n < p->num_streams; n++) {
        if (!p->streams[n]->st)
            return;
    }

    if (!(p->muxer->oformat->flags & AVFMT_NOFILE)) {
        MP_INFO(p, "Opening output file: %s\n", p->muxer->url);

        if (avio_open(&p->muxer->pb, p->muxer->url, AVIO_FLAG_WRITE) < 0) {
            MP_FATAL(p, "could not open '%s'\n", p->muxer->url);
            goto failed;
        }
    }

    p->t0 = mp_time_sec();

    MP_INFO(p, "Opening muxer: %s [%s]\n",
            p->muxer->oformat->long_name, p->muxer->oformat->name);

    if (p->metadata) {
        for (int i = 0; i < p->metadata->num_keys; i++) {
            av_dict_set(&p->muxer->metadata,
                p->metadata->keys[i], p->metadata->values[i], 0);
        }
    }

    AVDictionary *opts = NULL;
    mp_set_avdict(&opts, ctx->options->fopts);

    if (avformat_write_header(p->muxer, &opts) < 0) {
        MP_FATAL(p, "Failed to initialize muxer.\n");
        p->failed = true;
    } else {
        mp_avdict_print_unset(p->log, MSGL_WARN, opts);
    }

    av_dict_free(&opts);

    if (p->failed)
        goto failed;

    p->header_written = true;

    for (int n = 0; n < p->num_streams; n++) {
        struct mux_stream *s = p->streams[n];

        if (s->on_ready)
            s->on_ready(s->on_ready_ctx);
    }

    return;

failed:
    p->failed = true;
}

// called locked
static struct mux_stream *find_mux_stream(struct encode_lavc_context *ctx,
                                          enum AVMediaType codec_type)
{
    struct encode_priv *p = ctx->priv;

    for (int n = 0; n < p->num_streams; n++) {
        struct mux_stream *s = p->streams[n];
        if (s->codec_type == codec_type)
            return s;
    }

    return NULL;
}

void encode_lavc_expect_stream(struct encode_lavc_context *ctx,
                               enum stream_type type)
{
    struct encode_priv *p = ctx->priv;

    pthread_mutex_lock(&ctx->lock);

    enum AVMediaType codec_type = mp_to_av_stream_type(type);

    // These calls are idempotent.
    if (find_mux_stream(ctx, codec_type))
        goto done;

    if (p->header_written) {
        MP_ERR(p, "Cannot add a stream during encoding.\n");
        p->failed = true;
        goto done;
    }

    struct mux_stream *dst = talloc_ptrtype(p, dst);
    *dst = (struct mux_stream){
        .index = p->num_streams,
        .ctx = ctx,
        .codec_type = mp_to_av_stream_type(type),
    };
    snprintf(dst->name, sizeof(dst->name), "%s", stream_type_name(type));
    MP_TARRAY_APPEND(p, p->streams, p->num_streams, dst);

done:
    pthread_mutex_unlock(&ctx->lock);
}

void encode_lavc_stream_eof(struct encode_lavc_context *ctx,
                            enum stream_type type)
{
    if (!ctx)
        return;

    struct encode_priv *p = ctx->priv;

    pthread_mutex_lock(&ctx->lock);

    enum AVMediaType codec_type = mp_to_av_stream_type(type);
    struct mux_stream *dst = find_mux_stream(ctx, codec_type);

    // If we've reached EOF, even though the stream was selected, and we didn't
    // ever initialize it, we have a problem. We could mux some sort of dummy
    // stream (and could fail if actual data arrives later), or we bail out
    // early.
    if (dst && !dst->st) {
        MP_ERR(p, "No data on stream %s.\n", dst->name);
        p->failed = true;
    }

    pthread_mutex_unlock(&ctx->lock);
}

// Signal that you are ready to encode (you provide the codec params etc. too).
// This returns a muxing handle which you can use to add encodec packets.
// Can be called only once per stream. info is copied by callee as needed.
static struct mux_stream *encode_lavc_add_stream(struct encode_lavc_context *ctx,
                                                 struct encoder_stream_info *info,
                                                 void (*on_ready)(void *ctx),
                                                 void *on_ready_ctx)
{
    struct encode_priv *p = ctx->priv;

    pthread_mutex_lock(&ctx->lock);

    struct mux_stream *dst = find_mux_stream(ctx, info->codecpar->codec_type);
    if (!dst) {
        MP_ERR(p, "Cannot add a stream at runtime.\n");
        p->failed = true;
        goto done;
    }
    if (dst->st) {
        // Possibly via --gapless-audio, or explicitly recreating AO/VO.
        MP_ERR(p, "Encoder was reinitialized; this is not allowed.\n");
        p->failed = true;
        dst = NULL;
        goto done;
    }

    dst->st = avformat_new_stream(p->muxer, NULL);
    MP_HANDLE_OOM(dst->st);

    dst->encoder_timebase = info->timebase;
    dst->st->time_base = info->timebase; // lavf will change this on muxer init
    if (avcodec_parameters_copy(dst->st->codecpar, info->codecpar) < 0)
        MP_HANDLE_OOM(0);

    dst->on_ready = on_ready;
    dst->on_ready_ctx = on_ready_ctx;

    maybe_init_muxer(ctx);

done:
    pthread_mutex_unlock(&ctx->lock);

    return dst;
}

// Write a packet. This will take over ownership of `pkt`
static void encode_lavc_add_packet(struct mux_stream *dst, AVPacket *pkt)
{
    struct encode_lavc_context *ctx = dst->ctx;
    struct encode_priv *p = ctx->priv;

    assert(dst->st);

    pthread_mutex_lock(&ctx->lock);

    if (p->failed)
        goto done;

    if (!p->header_written) {
        MP_ERR(p, "Encoder trying to write packet before muxer was initialized.\n");
        p->failed = true;
        goto done;
    }

    pkt->stream_index = dst->st->index;
    assert(dst->st == p->muxer->streams[pkt->stream_index]);

    av_packet_rescale_ts(pkt, dst->encoder_timebase, dst->st->time_base);

    switch (dst->st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        p->vbytes += pkt->size;
        p->frames += 1;
        break;
    case AVMEDIA_TYPE_AUDIO:
        p->abytes += pkt->size;
        p->audioseconds += pkt->duration
            * (double)dst->st->time_base.num
            / (double)dst->st->time_base.den;
        break;
    }

    if (av_interleaved_write_frame(p->muxer, pkt) < 0) {
        MP_ERR(p, "Writing packet failed.\n");
        p->failed = true;
        pkt = NULL;
        goto done;
    }

    pkt = NULL;

done:
    pthread_mutex_unlock(&ctx->lock);
    if (pkt)
        av_packet_unref(pkt);
}

void encode_lavc_discontinuity(struct encode_lavc_context *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock(&ctx->lock);

    ctx->audio_pts_offset = MP_NOPTS_VALUE;
    ctx->discontinuity_pts_offset = MP_NOPTS_VALUE;

    pthread_mutex_unlock(&ctx->lock);
}

static void encode_lavc_printoptions(struct mp_log *log, const void *obj,
                                     const char *indent, const char *subindent,
                                     const char *unit, int filter_and,
                                     int filter_eq)
{
    const AVOption *opt = NULL;
    char optbuf[32];
    while ((opt = av_opt_next(obj, opt))) {
        // if flags are 0, it simply hasn't been filled in yet and may be
        // potentially useful
        if (opt->flags)
            if ((opt->flags & filter_and) != filter_eq)
                continue;
        /* Don't print CONST's on level one.
         * Don't print anything but CONST's on level two.
         * Only print items from the requested unit.
         */
        if (!unit && opt->type == AV_OPT_TYPE_CONST) {
            continue;
        } else if (unit && opt->type != AV_OPT_TYPE_CONST) {
            continue;
        } else if (unit && opt->type == AV_OPT_TYPE_CONST
                 && strcmp(unit, opt->unit))
        {
            continue;
        } else if (unit && opt->type == AV_OPT_TYPE_CONST) {
            mp_info(log, "%s", subindent);
        } else {
            mp_info(log, "%s", indent);
        }

        switch (opt->type) {
        case AV_OPT_TYPE_FLAGS:
            snprintf(optbuf, sizeof(optbuf), "%s=<flags>", opt->name);
            break;
        case AV_OPT_TYPE_INT:
            snprintf(optbuf, sizeof(optbuf), "%s=<int>", opt->name);
            break;
        case AV_OPT_TYPE_INT64:
            snprintf(optbuf, sizeof(optbuf), "%s=<int64>", opt->name);
            break;
        case AV_OPT_TYPE_DOUBLE:
            snprintf(optbuf, sizeof(optbuf), "%s=<double>", opt->name);
            break;
        case AV_OPT_TYPE_FLOAT:
            snprintf(optbuf, sizeof(optbuf), "%s=<float>", opt->name);
            break;
        case AV_OPT_TYPE_STRING:
            snprintf(optbuf, sizeof(optbuf), "%s=<string>", opt->name);
            break;
        case AV_OPT_TYPE_RATIONAL:
            snprintf(optbuf, sizeof(optbuf), "%s=<rational>", opt->name);
            break;
        case AV_OPT_TYPE_BINARY:
            snprintf(optbuf, sizeof(optbuf), "%s=<binary>", opt->name);
            break;
        case AV_OPT_TYPE_CONST:
            snprintf(optbuf, sizeof(optbuf), "  [+-]%s", opt->name);
            break;
        default:
            snprintf(optbuf, sizeof(optbuf), "%s", opt->name);
            break;
        }
        optbuf[sizeof(optbuf) - 1] = 0;
        mp_info(log, "%-32s ", optbuf);
        if (opt->help)
            mp_info(log, " %s", opt->help);
        mp_info(log, "\n");
        if (opt->unit && opt->type != AV_OPT_TYPE_CONST)
            encode_lavc_printoptions(log, obj, indent, subindent, opt->unit,
                                     filter_and, filter_eq);
    }
}

bool encode_lavc_showhelp(struct mp_log *log, struct encode_opts *opts)
{
    bool help_output = false;
#define CHECKS(str) ((str) && \
                     strcmp((str), "help") == 0 ? (help_output |= 1) : 0)
#define CHECKV(strv) ((strv) && (strv)[0] && \
                      strcmp((strv)[0], "help") == 0 ? (help_output |= 1) : 0)
    if (CHECKS(opts->format)) {
        const AVOutputFormat *c = NULL;
        void *iter = NULL;
        mp_info(log, "Available output formats:\n");
        while ((c = av_muxer_iterate(&iter))) {
            mp_info(log, "  --of=%-13s %s\n", c->name,
                   c->long_name ? c->long_name : "");
        }
    }
    if (CHECKV(opts->fopts)) {
        AVFormatContext *c = avformat_alloc_context();
        const AVOutputFormat *format = NULL;
        mp_info(log, "Available output format ctx->options:\n");
        encode_lavc_printoptions(log, c, "  --ofopts=", "           ", NULL,
                                 AV_OPT_FLAG_ENCODING_PARAM,
                                 AV_OPT_FLAG_ENCODING_PARAM);
        av_free(c);
        void *iter = NULL;
        while ((format = av_muxer_iterate(&iter))) {
            if (format->priv_class) {
                mp_info(log, "Additionally, for --of=%s:\n",
                       format->name);
                encode_lavc_printoptions(log, &format->priv_class, "  --ofopts=",
                                         "           ", NULL,
                                         AV_OPT_FLAG_ENCODING_PARAM,
                                         AV_OPT_FLAG_ENCODING_PARAM);
            }
        }
    }
    if (CHECKV(opts->vopts)) {
        AVCodecContext *c = avcodec_alloc_context3(NULL);
        const AVCodec *codec = NULL;
        mp_info(log, "Available output video codec ctx->options:\n");
        encode_lavc_printoptions(log,
            c, "  --ovcopts=", "            ", NULL,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_VIDEO_PARAM,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_VIDEO_PARAM);
        av_free(c);
        void *iter = NULL;
        while ((codec = av_codec_iterate(&iter))) {
            if (!av_codec_is_encoder(codec))
                continue;
            if (codec->type != AVMEDIA_TYPE_VIDEO)
                continue;
            if (opts->vcodec && opts->vcodec[0] &&
                strcmp(opts->vcodec, codec->name) != 0)
                continue;
            if (codec->priv_class) {
                mp_info(log, "Additionally, for --ovc=%s:\n",
                       codec->name);
                encode_lavc_printoptions(log,
                    &codec->priv_class, "  --ovcopts=",
                    "            ", NULL,
                    AV_OPT_FLAG_ENCODING_PARAM |
                    AV_OPT_FLAG_VIDEO_PARAM,
                    AV_OPT_FLAG_ENCODING_PARAM |
                    AV_OPT_FLAG_VIDEO_PARAM);
            }
        }
    }
    if (CHECKV(opts->aopts)) {
        AVCodecContext *c = avcodec_alloc_context3(NULL);
        const AVCodec *codec = NULL;
        mp_info(log, "Available output audio codec ctx->options:\n");
        encode_lavc_printoptions(log,
            c, "  --oacopts=", "            ", NULL,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_AUDIO_PARAM,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_AUDIO_PARAM);
        av_free(c);
        void *iter = NULL;
        while ((codec = av_codec_iterate(&iter))) {
            if (!av_codec_is_encoder(codec))
                continue;
            if (codec->type != AVMEDIA_TYPE_AUDIO)
                continue;
            if (opts->acodec && opts->acodec[0] &&
                strcmp(opts->acodec, codec->name) != 0)
                continue;
            if (codec->priv_class) {
                mp_info(log, "Additionally, for --oac=%s:\n",
                       codec->name);
                encode_lavc_printoptions(log,
                    &codec->priv_class, "  --oacopts=",
                    "           ", NULL,
                    AV_OPT_FLAG_ENCODING_PARAM |
                    AV_OPT_FLAG_AUDIO_PARAM,
                    AV_OPT_FLAG_ENCODING_PARAM |
                    AV_OPT_FLAG_AUDIO_PARAM);
            }
        }
    }
    if (CHECKS(opts->vcodec)) {
        const AVCodec *c = NULL;
        void *iter = NULL;
        mp_info(log, "Available output video codecs:\n");
        while ((c = av_codec_iterate(&iter))) {
            if (!av_codec_is_encoder(c))
                continue;
            if (c->type != AVMEDIA_TYPE_VIDEO)
                continue;
            mp_info(log, "  --ovc=%-12s %s\n", c->name,
                   c->long_name ? c->long_name : "");
        }
    }
    if (CHECKS(opts->acodec)) {
        const AVCodec *c = NULL;
        void *iter = NULL;
        mp_info(log, "Available output audio codecs:\n");
        while ((c = av_codec_iterate(&iter))) {
            if (!av_codec_is_encoder(c))
                continue;
            if (c->type != AVMEDIA_TYPE_AUDIO)
                continue;
            mp_info(log, "  --oac=%-12s %s\n", c->name,
                   c->long_name ? c->long_name : "");
        }
    }
    return help_output;
}

int encode_lavc_getstatus(struct encode_lavc_context *ctx,
                          char *buf, int bufsize,
                          float relative_position)
{
    if (!ctx)
        return -1;

    struct encode_priv *p = ctx->priv;

    double now = mp_time_sec();
    float minutes, megabytes, fps, x;
    float f = FFMAX(0.0001, relative_position);

    pthread_mutex_lock(&ctx->lock);

    if (p->failed) {
        snprintf(buf, bufsize, "(failed)\n");
        goto done;
    }

    minutes = (now - p->t0) / 60.0 * (1 - f) / f;
    megabytes = p->muxer->pb ? (avio_size(p->muxer->pb) / 1048576.0 / f) : 0;
    fps = p->frames / (now - p->t0);
    x = p->audioseconds / (now - p->t0);
    if (p->frames) {
        snprintf(buf, bufsize, "{%.1fmin %.1ffps %.1fMB}",
                 minutes, fps, megabytes);
    } else if (p->audioseconds) {
        snprintf(buf, bufsize, "{%.1fmin %.2fx %.1fMB}",
                 minutes, x, megabytes);
    } else {
        snprintf(buf, bufsize, "{%.1fmin %.1fMB}",
                 minutes, megabytes);
    }
    buf[bufsize - 1] = 0;

done:
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

bool encode_lavc_didfail(struct encode_lavc_context *ctx)
{
    if (!ctx)
        return false;
    pthread_mutex_lock(&ctx->lock);
    bool fail = ctx->priv->failed;
    pthread_mutex_unlock(&ctx->lock);
    return fail;
}

static void encoder_destroy(void *ptr)
{
    struct encoder_context *p = ptr;

    avcodec_free_context(&p->encoder);
    free_stream(p->twopass_bytebuffer);
}

struct encoder_context *encoder_context_alloc(struct encode_lavc_context *ctx,
                                              enum stream_type type,
                                              struct mp_log *log)
{
    if (!ctx) {
        mp_err(log, "the option --o (output file) must be specified\n");
        return NULL;
    }

    struct encoder_context *p = talloc_ptrtype(NULL, p);
    talloc_set_destructor(p, encoder_destroy);
    *p = (struct encoder_context){
        .global = ctx->global,
        .options = ctx->options,
        .oformat = ctx->oformat,
        .type = type,
        .log = log,
        .encode_lavc_ctx = ctx,
    };

    char *codec_name = type == STREAM_VIDEO
        ? p->options->vcodec
        : p->options->acodec;
    enum AVMediaType codec_type = mp_to_av_stream_type(type);
    const char *tname = stream_type_name(type);

    AVCodec *codec;
    if (codec_name&& codec_name[0]) {
        codec = avcodec_find_encoder_by_name(codec_name);
    } else {
        codec = avcodec_find_encoder(av_guess_codec(p->oformat, NULL,
                                                    p->options->file, NULL,
                                                    codec_type));
    }

    if (!codec) {
        MP_FATAL(p, "codec for %s not found\n", tname);
        goto fail;
    }
    if (codec->type != codec_type) {
        MP_FATAL(p, "codec for %s has wrong media type\n", tname);
        goto fail;
    }

    p->encoder = avcodec_alloc_context3(codec);
    MP_HANDLE_OOM(p->encoder);

    return p;

fail:
    talloc_free(p);
    return NULL;
}

static void encoder_2pass_prepare(struct encoder_context *p)
{
    char *filename = talloc_asprintf(NULL, "%s-%s-pass1.log",
                                     p->options->file,
                                     stream_type_name(p->type));

    if (p->encoder->flags & AV_CODEC_FLAG_PASS2) {
        MP_INFO(p, "Reading 2-pass log: %s\n", filename);
        struct stream *s = stream_open(filename, p->global);
        if (s) {
            struct bstr content = stream_read_complete(s, p, 1000000000);
            if (content.start) {
                p->encoder->stats_in = content.start;
            } else {
                MP_WARN(p, "could not read '%s', "
                        "disabling 2-pass encoding at pass 1\n", filename);
            }
            free_stream(s);
        } else {
            MP_WARN(p, "could not open '%s', "
                    "disabling 2-pass encoding at pass 2\n", filename);
            p->encoder->flags &= ~(unsigned)AV_CODEC_FLAG_PASS2;
        }
    }

    if (p->encoder->flags & AV_CODEC_FLAG_PASS1) {
        MP_INFO(p, "Writing to 2-pass log: %s\n", filename);
        p->twopass_bytebuffer = open_output_stream(filename, p->global);
        if (!p->twopass_bytebuffer) {
            MP_WARN(p, "could not open '%s', "
                    "disabling 2-pass encoding at pass 1\n", filename);
            p->encoder->flags &= ~(unsigned)AV_CODEC_FLAG_PASS1;
        }
    }

    talloc_free(filename);
}

bool encoder_init_codec_and_muxer(struct encoder_context *p,
                                  void (*on_ready)(void *ctx), void *ctx)
{
    assert(!avcodec_is_open(p->encoder));

    char **copts = p->type == STREAM_VIDEO
        ? p->options->vopts
        : p->options->aopts;

    // Set these now, so the code below can read back parsed settings from it.
    mp_set_avopts(p->log, p->encoder, copts);

    encoder_2pass_prepare(p);

    if (p->oformat->flags & AVFMT_GLOBALHEADER)
        p->encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    MP_INFO(p, "Opening encoder: %s [%s]\n",
            p->encoder->codec->long_name, p->encoder->codec->name);

    if (p->encoder->codec->capabilities & AV_CODEC_CAP_EXPERIMENTAL) {
        p->encoder->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        MP_WARN(p, "\n\n"
                   "           ********************************************\n"
                   "           ****    Experimental codec selected!     ****\n"
                   "           ********************************************\n\n"
                   "This means the output file may be broken or bad.\n"
                   "Possible reasons, problems, workarounds:\n"
                   "- Codec implementation in ffmpeg/libav is not finished yet.\n"
                   "     Try updating ffmpeg or libav.\n"
                   "- Bad picture quality, blocks, blurriness.\n"
                   "     Experiment with codec settings to maybe still get the\n"
                   "     desired quality output at the expense of bitrate.\n"
                   "- Broken files.\n"
                   "     May not work at all, or break with other software.\n"
                   "- Slow compression.\n"
                   "     Bear with it.\n"
                   "- Crashes.\n"
                   "     Happens. Try varying options to work around.\n"
                   "If none of this helps you, try another codec in place of %s.\n\n",
                p->encoder->codec->name);
    }

    if (avcodec_open2(p->encoder, p->encoder->codec, NULL) < 0) {
        MP_FATAL(p, "Could not initialize encoder.\n");
        goto fail;
    }

    p->info.timebase = p->encoder->time_base; // (_not_ changed by enc. init)
    p->info.codecpar = avcodec_parameters_alloc();
    MP_HANDLE_OOM(p->info.codecpar);
    if (avcodec_parameters_from_context(p->info.codecpar, p->encoder) < 0)
        goto fail;

    p->mux_stream = encode_lavc_add_stream(p->encode_lavc_ctx, &p->info,
                                           on_ready, ctx);
    if (!p->mux_stream)
        goto fail;

    return true;

fail:
    avcodec_close(p->encoder);
    return false;
}

bool encoder_encode(struct encoder_context *p, AVFrame *frame)
{
    int status = avcodec_send_frame(p->encoder, frame);
    if (status < 0) {
        if (frame && status == AVERROR_EOF)
            MP_ERR(p, "new data after sending EOF to encoder\n");
        goto fail;
    }

    for (;;) {
        AVPacket packet = {0};
        av_init_packet(&packet);

        status = avcodec_receive_packet(p->encoder, &packet);
        if (status == AVERROR(EAGAIN))
            break;
        if (status < 0 && status != AVERROR_EOF)
            goto fail;

        if (p->twopass_bytebuffer && p->encoder->stats_out) {
            stream_write_buffer(p->twopass_bytebuffer, p->encoder->stats_out,
                                strlen(p->encoder->stats_out));
        }

        if (status == AVERROR_EOF)
            break;

        encode_lavc_add_packet(p->mux_stream, &packet);
    }

    return true;

fail:
    MP_ERR(p, "error encoding at %s\n",
           frame ? av_ts2timestr(frame->pts, &p->encoder->time_base) : "EOF");
    return false;
}

double encoder_get_offset(struct encoder_context *p)
{
    switch (p->encoder->codec_type) {
    case AVMEDIA_TYPE_VIDEO: return p->options->voffset;
    case AVMEDIA_TYPE_AUDIO: return p->options->aoffset;
    default:                 return 0;
    }
}

// vim: ts=4 sw=4 et
