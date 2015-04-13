/*
 * muxing using libavformat
 * Copyright (C) 2010 Nicolas George <george@nsup.org>
 * Copyright (C) 2011-2012 Rudolf Polzer <divVerent@xonotic.org>
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

#include <libavutil/avutil.h>

#include "encode_lavc.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "options/m_option.h"
#include "options/options.h"
#include "osdep/timer.h"
#include "video/out/vo.h"
#include "talloc.h"
#include "stream/stream.h"

#define OPT_BASE_STRUCT struct encode_opts
const struct m_sub_options encode_config = {
    .opts = (const m_option_t[]) {
        OPT_STRING("o", file, CONF_GLOBAL | CONF_NOCFG | CONF_PRE_PARSE),
        OPT_STRING("of", format, CONF_GLOBAL),
        OPT_STRINGLIST("ofopts*", fopts, CONF_GLOBAL),
        OPT_FLOATRANGE("ofps", fps, CONF_GLOBAL, 0.0, 1000000.0),
        OPT_FLOATRANGE("omaxfps", maxfps, CONF_GLOBAL, 0.0, 1000000.0),
        OPT_STRING("ovc", vcodec, CONF_GLOBAL),
        OPT_STRINGLIST("ovcopts*", vopts, CONF_GLOBAL),
        OPT_STRING("oac", acodec, CONF_GLOBAL),
        OPT_STRINGLIST("oacopts*", aopts, CONF_GLOBAL),
        OPT_FLAG("oharddup", harddup, CONF_GLOBAL),
        OPT_FLOATRANGE("ovoffset", voffset, CONF_GLOBAL, -1000000.0, 1000000.0),
        OPT_FLOATRANGE("oaoffset", aoffset, CONF_GLOBAL, -1000000.0, 1000000.0),
        OPT_FLAG("ocopyts", copyts, CONF_GLOBAL),
        OPT_FLAG("orawts", rawts, CONF_GLOBAL),
        OPT_FLAG("oautofps", autofps, CONF_GLOBAL),
        OPT_FLAG("oneverdrop", neverdrop, CONF_GLOBAL),
        OPT_FLAG("ovfirst", video_first, CONF_GLOBAL),
        OPT_FLAG("oafirst", audio_first, CONF_GLOBAL),
        OPT_FLAG("ometadata", metadata, CONF_GLOBAL),
        {0}
    },
    .size = sizeof(struct encode_opts),
    .defaults = &(const struct encode_opts){
        .metadata = 1,
    },
};

static int set_to_avdictionary(struct encode_lavc_context *ctx,
                               AVDictionary **dictp,
                               const char *key,
                               const char *val)
{
    char keybuf[1024];
    char valuebuf[1024];

    if (key == NULL) {
        // we need to split at equals sign
        const char *equals = strchr(val, '=');
        if (!equals || equals - val >= sizeof(keybuf)) {
            MP_WARN(ctx, "option '%s' does not contain an equals sign\n",
                    val);
            return 0;
        }
        memcpy(keybuf, val, equals - val);
        keybuf[equals - val] = 0;
        key = keybuf;
        val = equals + 1;
    }

    // hack: support "qscale" key as virtual "global_quality" key that multiplies by QP2LAMBDA
    if (!strcmp(key, "qscale")) {
        key = "global_quality";
        snprintf(valuebuf, sizeof(valuebuf),
                 "%.1s(%s)*QP2LAMBDA",
                 (val[0] == '+' || val[0] == '-') ? val : "",
                 (val[0] == '+' || val[0] == '-') ? val + 1 : val);
        valuebuf[sizeof(valuebuf) - 1] = 0;
        val = valuebuf;
    }

    MP_VERBOSE(ctx, "setting value '%s' for key '%s'\n",
               val, key);

    if (av_dict_set(dictp, key, *val ? val : NULL,
                    (val[0] == '+' || val[0] == '-') ? AV_DICT_APPEND : 0) >= 0)
        return 1;

    return 0;
}

static bool value_has_flag(const char *value, const char *flag)
{
    bool state = true;
    bool ret = false;
    while (*value) {
        size_t l = strcspn(value, "+-");
        if (l == 0) {
            state = (*value == '+');
            ++value;
        } else {
            if (l == strlen(flag))
                if (!memcmp(value, flag, l))
                    ret = state;
            value += l;
        }
    }
    return ret;
}

#define CHECK_FAIL(ctx, val) \
    if (ctx && (ctx->failed || ctx->finished)) { \
        MP_ERR(ctx, \
               "Called a function on a %s encoding context. Bailing out.\n", \
               ctx->failed ? "failed" : "finished"); \
        return val; \
    }

#define CHECK_FAIL_UNLOCK(ctx, val) \
    if (ctx && (ctx->failed || ctx->finished)) { \
        MP_ERR(ctx, \
               "Called a function on a %s encoding context. Bailing out.\n", \
               ctx->failed ? "failed" : "finished"); \
        pthread_mutex_unlock(&ctx->lock); \
        return val; \
    }

int encode_lavc_available(struct encode_lavc_context *ctx)
{
    CHECK_FAIL(ctx, 0);
    return ctx && ctx->avc;
}

int encode_lavc_oformat_flags(struct encode_lavc_context *ctx)
{
    CHECK_FAIL(ctx, 0);
    return ctx->avc ? ctx->avc->oformat->flags : 0;
}

struct encode_lavc_context *encode_lavc_init(struct encode_opts *options,
                                             struct mpv_global *global)
{
    struct encode_lavc_context *ctx;
    const char *filename = options->file;

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

    ctx = talloc_zero(NULL, struct encode_lavc_context);
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->log = mp_log_new(ctx, global->log, "encode-lavc");
    ctx->global = global;
    encode_lavc_discontinuity(ctx);
    ctx->options = options;

    ctx->avc = avformat_alloc_context();

    if (ctx->options->format) {
        char *tok;
        const char *in = ctx->options->format;
        while (*in) {
            tok = av_get_token(&in, ",");
            ctx->avc->oformat = av_guess_format(tok, filename, NULL);
            av_free(tok);
            if (ctx->avc->oformat)
                break;
            if (*in)
                ++in;
        }
    } else
        ctx->avc->oformat = av_guess_format(NULL, filename, NULL);

    if (!ctx->avc->oformat) {
        encode_lavc_fail(ctx, "format not found\n");
        return NULL;
    }

    av_strlcpy(ctx->avc->filename, filename,
               sizeof(ctx->avc->filename));

    ctx->foptions = NULL;
    if (ctx->options->fopts) {
        char **p;
        for (p = ctx->options->fopts; *p; ++p) {
            if (!set_to_avdictionary(ctx, &ctx->foptions, NULL, *p))
                MP_WARN(ctx, "could not set option %s\n", *p);
        }
    }

    if (ctx->options->vcodec) {
        char *tok;
        const char *in = ctx->options->vcodec;
        while (*in) {
            tok = av_get_token(&in, ",");
            ctx->vc = avcodec_find_encoder_by_name(tok);
            av_free(tok);
            if (ctx->vc && ctx->vc->type != AVMEDIA_TYPE_VIDEO)
                ctx->vc = NULL;
            if (ctx->vc)
                break;
            if (*in)
                ++in;
        }
    } else
        ctx->vc = avcodec_find_encoder(av_guess_codec(ctx->avc->oformat, NULL,
                                                      ctx->avc->filename, NULL,
                                                      AVMEDIA_TYPE_VIDEO));

    if (ctx->options->acodec) {
        char *tok;
        const char *in = ctx->options->acodec;
        while (*in) {
            tok = av_get_token(&in, ",");
            ctx->ac = avcodec_find_encoder_by_name(tok);
            av_free(tok);
            if (ctx->ac && ctx->ac->type != AVMEDIA_TYPE_AUDIO)
                ctx->ac = NULL;
            if (ctx->ac)
                break;
            if (*in)
                ++in;
        }
    } else
        ctx->ac = avcodec_find_encoder(av_guess_codec(ctx->avc->oformat, NULL,
                                                      ctx->avc->filename, NULL,
                                                      AVMEDIA_TYPE_AUDIO));

    if (!ctx->vc && !ctx->ac) {
        encode_lavc_fail(
            ctx, "neither audio nor video codec was found\n");
        return NULL;
    }

    /* taken from ffmpeg unchanged
     * TODO turn this into an option if anyone needs this */

    ctx->avc->max_delay = 0.7 * AV_TIME_BASE;

    ctx->abytes = 0;
    ctx->vbytes = 0;
    ctx->frames = 0;

    if (options->video_first)
        ctx->video_first = true;
    if (options->audio_first)
        ctx->audio_first = true;

    return ctx;
}

void encode_lavc_set_metadata(struct encode_lavc_context *ctx,
                              struct mp_tags *metadata)
{
    if (ctx->options->metadata)
        ctx->metadata = metadata;
}

int encode_lavc_start(struct encode_lavc_context *ctx)
{
    AVDictionaryEntry *de;

    if (ctx->header_written < 0)
        return 0;
    if (ctx->header_written > 0)
        return 1;

    CHECK_FAIL(ctx, 0);

    if (ctx->expect_video) {
        unsigned i;
        for (i = 0; i < ctx->avc->nb_streams; ++i)
            if (ctx->avc->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
                break;
        if (i >= ctx->avc->nb_streams) {
            if (ctx->avc->oformat->video_codec != AV_CODEC_ID_NONE ||
                ctx->options->vcodec) {
                encode_lavc_fail(ctx,
                    "no video stream succeeded - invalid codec?\n");
                return 0;
            }
        }
    }
    if (ctx->expect_audio) {
        unsigned i;
        for (i = 0; i < ctx->avc->nb_streams; ++i)
            if (ctx->avc->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
                break;
        if (i >= ctx->avc->nb_streams) {
            if (ctx->avc->oformat->audio_codec != AV_CODEC_ID_NONE ||
                ctx->options->acodec) {
                encode_lavc_fail(ctx,
                    "no audio stream succeeded - invalid codec?\n");
                return 0;
            }
        }
    }

    ctx->header_written = -1;

    if (!(ctx->avc->oformat->flags & AVFMT_NOFILE)) {
        MP_INFO(ctx, "Opening output file: %s\n",
                ctx->avc->filename);

        if (avio_open(&ctx->avc->pb, ctx->avc->filename,
                      AVIO_FLAG_WRITE) < 0) {
            encode_lavc_fail(ctx, "could not open '%s'\n",
                             ctx->avc->filename);
            return 0;
        }
    }

    ctx->t0 = mp_time_sec();

    MP_INFO(ctx, "Opening muxer: %s [%s]\n",
            ctx->avc->oformat->long_name, ctx->avc->oformat->name);

    if (ctx->metadata) {
        for (int i = 0; i < ctx->metadata->num_keys; i++) {
            av_dict_set(&ctx->avc->metadata,
                ctx->metadata->keys[i], ctx->metadata->values[i], 0);
        }
    }

    if (avformat_write_header(ctx->avc, &ctx->foptions) < 0) {
        encode_lavc_fail(ctx, "could not write header\n");
        return 0;
    }

    for (de = NULL; (de = av_dict_get(ctx->foptions, "", de,
                                      AV_DICT_IGNORE_SUFFIX));)
        MP_WARN(ctx, "ofopts: key '%s' not found.\n", de->key);
    av_dict_free(&ctx->foptions);

    ctx->header_written = 1;
    return 1;
}

void encode_lavc_free(struct encode_lavc_context *ctx)
{
    if (!ctx)
        return;

    if (!ctx->finished)
        encode_lavc_fail(ctx,
                         "called encode_lavc_free without encode_lavc_finish\n");

    pthread_mutex_destroy(&ctx->lock);
    talloc_free(ctx);
}

void encode_lavc_finish(struct encode_lavc_context *ctx)
{
    unsigned i;

    if (!ctx)
        return;

    if (ctx->finished)
        return;

    if (ctx->avc) {
        if (ctx->header_written > 0)
            av_write_trailer(ctx->avc);  // this is allowed to fail

        for (i = 0; i < ctx->avc->nb_streams; i++) {
            switch (ctx->avc->streams[i]->codec->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                if (ctx->twopass_bytebuffer_v) {
                    char *stats = ctx->avc->streams[i]->codec->stats_out;
                    if (stats)
                        stream_write_buffer(ctx->twopass_bytebuffer_v,
                                            stats, strlen(stats));
                }
                break;
            case AVMEDIA_TYPE_AUDIO:
                if (ctx->twopass_bytebuffer_a) {
                    char *stats = ctx->avc->streams[i]->codec->stats_out;
                    if (stats)
                        stream_write_buffer(ctx->twopass_bytebuffer_a,
                                            stats, strlen(stats));
                }
                break;
            default:
                break;
            }
            avcodec_close(ctx->avc->streams[i]->codec);
            talloc_free(ctx->avc->streams[i]->codec->stats_in);
            av_free(ctx->avc->streams[i]->codec);
            av_free(ctx->avc->streams[i]->info);
            av_free(ctx->avc->streams[i]);
        }

        if (ctx->twopass_bytebuffer_v) {
            free_stream(ctx->twopass_bytebuffer_v);
            ctx->twopass_bytebuffer_v = NULL;
        }

        if (ctx->twopass_bytebuffer_a) {
            free_stream(ctx->twopass_bytebuffer_a);
            ctx->twopass_bytebuffer_a = NULL;
        }

        MP_INFO(ctx, "vo-lavc: encoded %lld bytes\n",
               ctx->vbytes);
        MP_INFO(ctx, "ao-lavc: encoded %lld bytes\n",
               ctx->abytes);
        if (ctx->avc->pb) {
            MP_INFO(ctx, "muxing overhead %lld bytes\n",
                   (long long) (avio_size(ctx->avc->pb) - ctx->vbytes
                                                        - ctx->abytes));
            avio_close(ctx->avc->pb);
        }

        av_free(ctx->avc);
    }

    ctx->finished = true;
}

void encode_lavc_set_video_fps(struct encode_lavc_context *ctx, float fps)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->vo_fps = fps;
    pthread_mutex_unlock(&ctx->lock);
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

static void encode_2pass_prepare(struct encode_lavc_context *ctx,
                                 AVDictionary **dictp,
                                 AVStream *stream, struct stream **bytebuf,
                                 const char *prefix)
{
    if (!*bytebuf) {
        char buf[sizeof(ctx->avc->filename) + 12];
        AVDictionaryEntry *de = av_dict_get(ctx->voptions, "flags", NULL, 0);

        snprintf(buf, sizeof(buf), "%s-%s-pass1.log", ctx->avc->filename,
                 prefix);
        buf[sizeof(buf) - 1] = 0;

        if (value_has_flag(de ? de->value : "", "pass2")) {
            if (!(*bytebuf = stream_open(buf, ctx->global))) {
                MP_WARN(ctx, "%s: could not open '%s', "
                       "disabling 2-pass encoding at pass 2\n", prefix, buf);
                stream->codec->flags &= ~CODEC_FLAG_PASS2;
                set_to_avdictionary(ctx, dictp, "flags", "-pass2");
            } else {
                struct bstr content = stream_read_complete(*bytebuf, NULL,
                                                           1000000000);
                if (content.start == NULL) {
                    MP_WARN(ctx, "%s: could not read '%s', "
                           "disabling 2-pass encoding at pass 1\n",
                           prefix, ctx->avc->filename);
                } else {
                    content.start[content.len] = 0;
                    stream->codec->stats_in = content.start;
                }
                free_stream(*bytebuf);
                *bytebuf = NULL;
            }
        }

        if (value_has_flag(de ? de->value : "", "pass1")) {
            if (!(*bytebuf = open_output_stream(buf, ctx->global))) {
                MP_WARN(ctx,
                    "%s: could not open '%s', disabling "
                    "2-pass encoding at pass 1\n",
                    prefix, ctx->avc->filename);
                set_to_avdictionary(ctx, dictp, "flags", "-pass1");
            }
        }
    }
}

AVStream *encode_lavc_alloc_stream(struct encode_lavc_context *ctx,
                                   enum AVMediaType mt)
{
    AVDictionaryEntry *de;
    AVStream *stream = NULL;
    char **p;
    int i;

    CHECK_FAIL(ctx, NULL);

    if (ctx->header_written)
        return NULL;

    for (i = 0; i < ctx->avc->nb_streams; ++i)
        if (ctx->avc->streams[i]->codec->codec_type == mt)
            // already have a stream of that type, this cannot really happen
            return NULL;

    if (ctx->avc->nb_streams == 0) {
        // if this stream isn't stream #0, allocate a dummy stream first for
        // the next loop to use
        if (mt == AVMEDIA_TYPE_VIDEO && ctx->audio_first) {
            MP_INFO(ctx, "vo-lavc: preallocated audio stream for later use\n");
            avformat_new_stream(ctx->avc, NULL); // this one is AVMEDIA_TYPE_UNKNOWN for now
        }
        if (mt == AVMEDIA_TYPE_AUDIO && ctx->video_first) {
            MP_INFO(ctx, "ao-lavc: preallocated video stream for later use\n");
            avformat_new_stream(ctx->avc, NULL); // this one is AVMEDIA_TYPE_UNKNOWN for now
        }
    } else {
        // find possibly preallocated stream
        for (i = 0; i < ctx->avc->nb_streams; ++i)
            if (ctx->avc->streams[i]->codec->codec_type == AVMEDIA_TYPE_UNKNOWN) // preallocated stream
                stream = ctx->avc->streams[i];
    }
    if (!stream)
        stream = avformat_new_stream(ctx->avc, NULL);

    if (ctx->timebase.den == 0) {
        AVRational r;

        if (ctx->options->fps > 0)
            r = av_d2q(ctx->options->fps, ctx->options->fps * 1001 + 2);
        else if (ctx->options->autofps && ctx->vo_fps > 0) {
            r = av_d2q(ctx->vo_fps, ctx->vo_fps * 1001 + 2);
            MP_INFO(ctx, "option --ofps not specified "
                "but --oautofps is active, using guess of %u/%u\n",
                (unsigned)r.num, (unsigned)r.den);
        } else {
            // we want to handle:
            //      1/25
            //   1001/24000
            //   1001/30000
            // for this we would need 120000fps...
            // however, mpeg-4 only allows 16bit values
            // so let's take 1001/30000 out
            r.num = 24000;
            r.den = 1;
            MP_INFO(ctx, "option --ofps not specified "
                "and fps could not be inferred, using guess of %u/%u\n",
                (unsigned)r.num, (unsigned)r.den);
        }

        if (ctx->vc && ctx->vc->supported_framerates)
            r = ctx->vc->supported_framerates[av_find_nearest_q_idx(r,
                    ctx->vc->supported_framerates)];

        ctx->timebase.num = r.den;
        ctx->timebase.den = r.num;
    }

    switch (mt) {
    case AVMEDIA_TYPE_VIDEO:
        if (!ctx->vc) {
            if (ctx->avc->oformat->video_codec != AV_CODEC_ID_NONE ||
                ctx->options->vcodec) {
                encode_lavc_fail(ctx, "vo-lavc: encoder not found\n");
            }
            return NULL;
        }
        avcodec_get_context_defaults3(stream->codec, ctx->vc);

        // Using codec->time_base is deprecated, but needed for older lavf.
        stream->time_base = ctx->timebase;
        stream->codec->time_base = ctx->timebase;

        ctx->voptions = NULL;

        if (ctx->options->vopts)
            for (p = ctx->options->vopts; *p; ++p)
                if (!set_to_avdictionary(ctx, &ctx->voptions, NULL, *p))
                    MP_WARN(ctx, "vo-lavc: could not set option %s\n", *p);

        de = av_dict_get(ctx->voptions, "global_quality", NULL, 0);
        if (de)
            set_to_avdictionary(ctx, &ctx->voptions, "flags", "+qscale");

        if (ctx->avc->oformat->flags & AVFMT_GLOBALHEADER)
            set_to_avdictionary(ctx, &ctx->voptions, "flags", "+global_header");

        encode_2pass_prepare(ctx, &ctx->voptions, stream,
                             &ctx->twopass_bytebuffer_v,
                             "vo-lavc");
        break;

    case AVMEDIA_TYPE_AUDIO:
        if (!ctx->ac) {
            if (ctx->avc->oformat->audio_codec != AV_CODEC_ID_NONE ||
                ctx->options->acodec) {
                encode_lavc_fail(ctx, "ao-lavc: encoder not found\n");
            }
            return NULL;
        }
        avcodec_get_context_defaults3(stream->codec, ctx->ac);

        // Using codec->time_base is deprecated, but needed for older lavf.
        stream->time_base = ctx->timebase;
        stream->codec->time_base = ctx->timebase;

        ctx->aoptions = NULL;

        if (ctx->options->aopts)
            for (p = ctx->options->aopts; *p; ++p)
                if (!set_to_avdictionary(ctx, &ctx->aoptions, NULL, *p))
                    MP_WARN(ctx, "ao-lavc: could not set option %s\n", *p);

        de = av_dict_get(ctx->aoptions, "global_quality", NULL, 0);
        if (de)
            set_to_avdictionary(ctx, &ctx->aoptions, "flags", "+qscale");

        if (ctx->avc->oformat->flags & AVFMT_GLOBALHEADER)
            set_to_avdictionary(ctx, &ctx->aoptions, "flags", "+global_header");

        encode_2pass_prepare(ctx, &ctx->aoptions, stream,
                             &ctx->twopass_bytebuffer_a,
                             "ao-lavc");
        break;

    default:
        encode_lavc_fail(ctx, "requested invalid stream type\n");
        return NULL;
    }

    return stream;
}

AVCodec *encode_lavc_get_codec(struct encode_lavc_context *ctx,
                               AVStream *stream)
{
    CHECK_FAIL(ctx, NULL);

    switch (stream->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        return ctx->vc;
    case AVMEDIA_TYPE_AUDIO:
        return ctx->ac;
    default:
        break;
    }
    return NULL;
}

int encode_lavc_open_codec(struct encode_lavc_context *ctx, AVStream *stream)
{
    AVDictionaryEntry *de;
    int ret;

    CHECK_FAIL(ctx, -1);

    switch (stream->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        MP_INFO(ctx, "Opening video encoder: %s [%s]\n",
                ctx->vc->long_name, ctx->vc->name);

        if (ctx->vc->capabilities & CODEC_CAP_EXPERIMENTAL) {
            stream->codec->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
            MP_WARN(ctx, "\n\n"
                       "           ********************************************\n"
                       "           **** Experimental VIDEO codec selected! ****\n"
                       "           ********************************************\n\n"
                       "This means the output file may be broken or bad.\n"
                       "Possible reasons, problems, workarounds:\n"
                       "- Codec implementation in ffmpeg/libav is not finished yet.\n"
                       "     Try updating ffmpeg or libav.\n"
                       "- Bad picture quality, blocks, blurriness.\n"
                       "     Experiment with codec settings (--ovcopts) to maybe still get the\n"
                       "     desired quality output at the expense of bitrate.\n"
                       "- Slow compression.\n"
                       "     Bear with it.\n"
                       "- Crashes.\n"
                       "     Happens. Try varying options to work around.\n"
                       "If none of this helps you, try another codec in place of %s.\n\n",
                   ctx->vc->name);
        }

        ret = avcodec_open2(stream->codec, ctx->vc, &ctx->voptions);

        // complain about all remaining options, then free the dict
        for (de = NULL; (de = av_dict_get(ctx->voptions, "", de,
                                          AV_DICT_IGNORE_SUFFIX));)
            MP_WARN(ctx, "ovcopts: key '%s' not found.\n",
                   de->key);
        av_dict_free(&ctx->voptions);

        break;
    case AVMEDIA_TYPE_AUDIO:
        MP_INFO(ctx, "Opening audio encoder: %s [%s]\n",
                ctx->ac->long_name, ctx->ac->name);

        if (ctx->ac->capabilities & CODEC_CAP_EXPERIMENTAL) {
            stream->codec->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
            MP_WARN(ctx, "\n\n"
                       "           ********************************************\n"
                       "           **** Experimental AUDIO codec selected! ****\n"
                       "           ********************************************\n\n"
                       "This means the output file may be broken or bad.\n"
                       "Possible reasons, problems, workarounds:\n"
                       "- Codec implementation in ffmpeg/libav is not finished yet.\n"
                       "     Try updating ffmpeg or libav.\n"
                       "- Bad sound quality, noise, clicking, whistles, choppiness.\n"
                       "     Experiment with codec settings (--oacopts) to maybe still get the\n"
                       "     desired quality output at the expense of bitrate.\n"
                       "- Slow compression.\n"
                       "     Bear with it.\n"
                       "- Crashes.\n"
                       "     Happens. Try varying options to work around.\n"
                       "If none of this helps you, try another codec in place of %s.\n\n",
                   ctx->ac->name);
        }
        ret = avcodec_open2(stream->codec, ctx->ac, &ctx->aoptions);

        // complain about all remaining options, then free the dict
        for (de = NULL; (de = av_dict_get(ctx->aoptions, "", de,
                                          AV_DICT_IGNORE_SUFFIX));)
            MP_WARN(ctx, "oacopts: key '%s' not found.\n",
                   de->key);
        av_dict_free(&ctx->aoptions);

        break;
    default:
        ret = -1;
        break;
    }

    if (ret < 0)
        encode_lavc_fail(ctx,
                         "unable to open encoder (see above for the cause)\n");

    return ret;
}

void encode_lavc_write_stats(struct encode_lavc_context *ctx, AVStream *stream)
{
    CHECK_FAIL(ctx, );

    switch (stream->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        if (ctx->twopass_bytebuffer_v)
            if (stream->codec->stats_out)
                stream_write_buffer(ctx->twopass_bytebuffer_v,
                                    stream->codec->stats_out,
                                    strlen(stream->codec->stats_out));
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (ctx->twopass_bytebuffer_a)
            if (stream->codec->stats_out)
                stream_write_buffer(ctx->twopass_bytebuffer_a,
                                    stream->codec->stats_out,
                                    strlen(stream->codec->stats_out));
        break;
    default:
        break;
    }
}

int encode_lavc_write_frame(struct encode_lavc_context *ctx, AVPacket *packet)
{
    int r;

    CHECK_FAIL(ctx, -1);

    if (ctx->header_written <= 0)
        return -1;

    MP_DBG(ctx,
        "write frame: stream %d ptsi %d (%f) dtsi %d (%f) size %d\n",
        (int)packet->stream_index,
        (int)packet->pts,
        packet->pts
        * (double)ctx->avc->streams[packet->stream_index]->time_base.num
        / (double)ctx->avc->streams[packet->stream_index]->time_base.den,
        (int)packet->dts,
        packet->dts
        * (double)ctx->avc->streams[packet->stream_index]->time_base.num
        / (double)ctx->avc->streams[packet->stream_index]->time_base.den,
        (int)packet->size);

    switch (ctx->avc->streams[packet->stream_index]->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        ctx->vbytes += packet->size;
        ++ctx->frames;
        break;
    case AVMEDIA_TYPE_AUDIO:
        ctx->abytes += packet->size;
        ctx->audioseconds += packet->duration
            * (double)ctx->avc->streams[packet->stream_index]->time_base.num
            / (double)ctx->avc->streams[packet->stream_index]->time_base.den;
        break;
    default:
        break;
    }

    r = av_interleaved_write_frame(ctx->avc, packet);

    return r;
}

int encode_lavc_supports_pixfmt(struct encode_lavc_context *ctx,
                                enum AVPixelFormat pix_fmt)
{
    CHECK_FAIL(ctx, 0);

    if (!ctx->vc)
        return 0;
    if (pix_fmt == AV_PIX_FMT_NONE)
        return 0;

    if (!ctx->vc->pix_fmts)
        return 1;
    else {
        const enum AVPixelFormat *p;
        for (p = ctx->vc->pix_fmts; *p >= 0; ++p) {
            if (pix_fmt == *p)
                return 1;
        }
    }
    return 0;
}

void encode_lavc_discontinuity(struct encode_lavc_context *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock(&ctx->lock);

    CHECK_FAIL_UNLOCK(ctx, );

    ctx->audio_pts_offset = MP_NOPTS_VALUE;
    ctx->last_video_in_pts = MP_NOPTS_VALUE;
    ctx->discontinuity_pts_offset = MP_NOPTS_VALUE;

    pthread_mutex_unlock(&ctx->lock);
}

static void encode_lavc_printoptions(struct mp_log *log, void *obj,
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
        if (!unit && opt->type == AV_OPT_TYPE_CONST)
            continue;
        else if (unit && opt->type != AV_OPT_TYPE_CONST)
            continue;
        else if (unit && opt->type == AV_OPT_TYPE_CONST
                 && strcmp(unit, opt->unit))
            continue;
        else if (unit && opt->type == AV_OPT_TYPE_CONST)
            mp_info(log, "%s", subindent);
        else
            mp_info(log, "%s", indent);

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
    if (av_codec_next(NULL) == NULL)
        mp_err(log, "NO CODECS\n");
#define CHECKS(str) ((str) && \
                     strcmp((str), "help") == 0 ? (help_output |= 1) : 0)
#define CHECKV(strv) ((strv) && (strv)[0] && \
                      strcmp((strv)[0], "help") == 0 ? (help_output |= 1) : 0)
    if (CHECKS(opts->format)) {
        AVOutputFormat *c = NULL;
        mp_info(log, "Available output formats:\n");
        while ((c = av_oformat_next(c)))
            mp_info(log, "  --of=%-13s %s\n", c->name,
                   c->long_name ? c->long_name : "");
        av_free(c);
    }
    if (CHECKV(opts->fopts)) {
        AVFormatContext *c = avformat_alloc_context();
        AVOutputFormat *format = NULL;
        mp_info(log, "Available output format ctx->options:\n");
        encode_lavc_printoptions(log, c, "  --ofopts=", "           ", NULL,
                                 AV_OPT_FLAG_ENCODING_PARAM,
                                 AV_OPT_FLAG_ENCODING_PARAM);
        av_free(c);
        while ((format = av_oformat_next(format))) {
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
        AVCodec *codec = NULL;
        mp_info(log, "Available output video codec ctx->options:\n");
        encode_lavc_printoptions(log,
            c, "  --ovcopts=", "            ", NULL,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_VIDEO_PARAM,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_VIDEO_PARAM);
        av_free(c);
        while ((codec = av_codec_next(codec))) {
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
        AVCodec *codec = NULL;
        mp_info(log, "Available output audio codec ctx->options:\n");
        encode_lavc_printoptions(log,
            c, "  --oacopts=", "            ", NULL,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_AUDIO_PARAM,
            AV_OPT_FLAG_ENCODING_PARAM |
            AV_OPT_FLAG_AUDIO_PARAM);
        av_free(c);
        while ((codec = av_codec_next(codec))) {
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
        AVCodec *c = NULL;
        mp_info(log, "Available output video codecs:\n");
        while ((c = av_codec_next(c))) {
            if (!av_codec_is_encoder(c))
                continue;
            if (c->type != AVMEDIA_TYPE_VIDEO)
                continue;
            mp_info(log, "  --ovc=%-12s %s\n", c->name,
                   c->long_name ? c->long_name : "");
        }
        av_free(c);
    }
    if (CHECKS(opts->acodec)) {
        AVCodec *c = NULL;
        mp_info(log, "Available output audio codecs:\n");
        while ((c = av_codec_next(c))) {
            if (!av_codec_is_encoder(c))
                continue;
            if (c->type != AVMEDIA_TYPE_AUDIO)
                continue;
            mp_info(log, "  --oac=%-12s %s\n", c->name,
                   c->long_name ? c->long_name : "");
        }
        av_free(c);
    }
    return help_output;
}

double encode_lavc_getoffset(struct encode_lavc_context *ctx, AVStream *stream)
{
    CHECK_FAIL(ctx, 0);

    switch (stream->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        return ctx->options->voffset;
    case AVMEDIA_TYPE_AUDIO:
        return ctx->options->aoffset;
    default:
        break;
    }
    return 0;
}

int encode_lavc_getstatus(struct encode_lavc_context *ctx,
                          char *buf, int bufsize,
                          float relative_position)
{
    double now = mp_time_sec();
    float minutes, megabytes, fps, x;
    float f = FFMAX(0.0001, relative_position);
    if (!ctx)
        return -1;

    pthread_mutex_lock(&ctx->lock);

    CHECK_FAIL_UNLOCK(ctx, -1);

    minutes = (now - ctx->t0) / 60.0 * (1 - f) / f;
    megabytes = ctx->avc->pb ? (avio_size(ctx->avc->pb) / 1048576.0 / f) : 0;
    fps = ctx->frames / (now - ctx->t0);
    x = ctx->audioseconds / (now - ctx->t0);
    if (ctx->frames)
        snprintf(buf, bufsize, "{%.1fmin %.1ffps %.1fMB}",
                 minutes, fps, megabytes);
    else if (ctx->audioseconds)
        snprintf(buf, bufsize, "{%.1fmin %.2fx %.1fMB}",
                 minutes, x, megabytes);
    else
        snprintf(buf, bufsize, "{%.1fmin %.1fMB}",
                 minutes, megabytes);
    buf[bufsize - 1] = 0;

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

void encode_lavc_expect_stream(struct encode_lavc_context *ctx, int mt)
{
    pthread_mutex_lock(&ctx->lock);

    CHECK_FAIL_UNLOCK(ctx, );

    switch (mt) {
    case AVMEDIA_TYPE_VIDEO:
        ctx->expect_video = true;
        break;
    case AVMEDIA_TYPE_AUDIO:
        ctx->expect_audio = true;
        break;
    }

    pthread_mutex_unlock(&ctx->lock);
}

bool encode_lavc_didfail(struct encode_lavc_context *ctx)
{
    if (!ctx)
        return false;
    pthread_mutex_lock(&ctx->lock);
    bool fail = ctx && ctx->failed;
    pthread_mutex_unlock(&ctx->lock);
    return fail;
}

void encode_lavc_fail(struct encode_lavc_context *ctx, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_va(ctx->log, MSGL_ERR, format, va);
    va_end(va);
    if (ctx->failed)
        return;
    ctx->failed = true;
    encode_lavc_finish(ctx);
}

bool encode_lavc_set_csp(struct encode_lavc_context *ctx,
                         AVStream *stream, enum mp_csp csp)
{
    CHECK_FAIL(ctx, NULL);

    if (ctx->header_written) {
        if (stream->codec->colorspace != mp_csp_to_avcol_spc(csp))
            MP_WARN(ctx, "can not change color space during encoding\n");
        return false;
    }

    stream->codec->colorspace = mp_csp_to_avcol_spc(csp);
    return true;
}

bool encode_lavc_set_csp_levels(struct encode_lavc_context *ctx,
                                AVStream *stream, enum mp_csp_levels lev)
{
    CHECK_FAIL(ctx, NULL);

    if (ctx->header_written) {
        if (stream->codec->color_range != mp_csp_levels_to_avcol_range(lev))
            MP_WARN(ctx, "can not change color space during encoding\n");
        return false;
    }

    stream->codec->color_range = mp_csp_levels_to_avcol_range(lev);
    return true;
}

enum mp_csp encode_lavc_get_csp(struct encode_lavc_context *ctx,
                                AVStream *stream)
{
    CHECK_FAIL(ctx, 0);

    return avcol_spc_to_mp_csp(stream->codec->colorspace);
}

enum mp_csp_levels encode_lavc_get_csp_levels(struct encode_lavc_context *ctx,
                                              AVStream *stream)
{
    CHECK_FAIL(ctx, 0);

    return avcol_range_to_mp_csp_levels(stream->codec->color_range);
}

// vim: ts=4 sw=4 et
