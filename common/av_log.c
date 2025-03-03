/*
 * av_log to mp_msg converter
 * Copyright (C) 2006 Michael Niedermayer
 * Copyright (C) 2009 Uoti Urpala
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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "av_log.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "config.h"
#include "misc/bstr.h"
#include "osdep/threads.h"

#include <libavutil/avutil.h>
#include <libavutil/ffversion.h>
#include <libavutil/log.h>
#include <libavutil/version.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libswresample/swresample.h>
#include <libswresample/version.h>
#include <libswscale/swscale.h>
#include <libswscale/version.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/version.h>

#if HAVE_LIBAVDEVICE
#include <libavdevice/avdevice.h>
#endif

// Needed because the av_log callback does not provide a library-safe message
// callback.
static mp_static_mutex log_lock = MP_STATIC_MUTEX_INITIALIZER;
static struct mpv_global *log_mpv_instance;
static struct mp_log *log_root, *log_decaudio, *log_decvideo, *log_demuxer;
static bool log_print_prefix = true;
static bstr log_buffer;

static int av_log_level_to_mp_level(int av_level)
{
    if (av_level > AV_LOG_VERBOSE)
        return MSGL_TRACE;
    if (av_level > AV_LOG_INFO)
        return MSGL_DEBUG;
    if (av_level > AV_LOG_WARNING)
        return MSGL_V;
    if (av_level > AV_LOG_ERROR)
        return MSGL_WARN;
    if (av_level > AV_LOG_FATAL)
        return MSGL_ERR;
    return MSGL_FATAL;
}

static struct mp_log *get_av_log(void *ptr)
{
    if (!ptr)
        return log_root;

    AVClass *avc = *(AVClass **)ptr;
    if (!avc) {
        mp_warn(log_root,
               "av_log callback called with bad parameters (NULL AVClass).\n"
               "This is a bug in one of FFmpeg libraries used.\n");
        return log_root;
    }

    if (!strcmp(avc->class_name, "AVCodecContext")) {
        AVCodecContext *s = ptr;
        if (s->codec) {
            if (s->codec->type == AVMEDIA_TYPE_AUDIO) {
                if (av_codec_is_decoder(s->codec))
                    return log_decaudio;
            } else if (s->codec->type == AVMEDIA_TYPE_VIDEO) {
                if (av_codec_is_decoder(s->codec))
                    return log_decvideo;
            }
        }
    }

    if (!strcmp(avc->class_name, "AVFormatContext")) {
        AVFormatContext *s = ptr;
        if (s->iformat)
            return log_demuxer;
    }

    return log_root;
}

static const char *avclass_item_name(void *obj, const AVClass *avc)
{
    return (avc->item_name ? avc->item_name : av_default_item_name)(obj);
}

static void mp_msg_av_log_callback(void *ptr, int level, const char *fmt,
                                   va_list vl)
{
    AVClass *avc = ptr ? *(AVClass **)ptr : NULL;
    int mp_level = av_log_level_to_mp_level(level);

    // Note: mp_log is thread-safe, but destruction of the log instances is not.
    mp_mutex_lock(&log_lock);

    if (!log_mpv_instance) {
        mp_mutex_unlock(&log_lock);
        // Fallback to stderr
        vfprintf(stderr, fmt, vl);
        return;
    }

    struct mp_log *log = get_av_log(ptr);

    if (mp_msg_test(log, mp_level)) {
        log_buffer.len = 0;
        bstr_xappend_vasprintf(log_root, &log_buffer, fmt, vl);
        if (!log_buffer.len)
            goto done;
        const char *prefix = avc ? avclass_item_name(ptr, avc) : NULL;
        if (log_print_prefix && prefix) {
            mp_msg(log, mp_level, "%s: %.*s", prefix, BSTR_P(log_buffer));
        } else {
            mp_msg(log, mp_level, "%.*s", BSTR_P(log_buffer));
        }
        log_print_prefix = log_buffer.start[log_buffer.len - 1] == '\n';
    }

done:
    mp_mutex_unlock(&log_lock);
}

void init_libav(struct mpv_global *global)
{
    mp_mutex_lock(&log_lock);
    if (!log_mpv_instance) {
        log_mpv_instance = global;
        log_root = mp_log_new(NULL, global->log, "ffmpeg");
        log_decaudio = mp_log_new(log_root, log_root, "audio");
        log_decvideo = mp_log_new(log_root, log_root, "video");
        log_demuxer = mp_log_new(log_root, log_root, "demuxer");
        log_buffer = (bstr){0};
        av_log_set_callback(mp_msg_av_log_callback);
    }
    mp_mutex_unlock(&log_lock);

    avformat_network_init();

#if HAVE_LIBAVDEVICE
    avdevice_register_all();
#endif
}

void uninit_libav(struct mpv_global *global)
{
    mp_mutex_lock(&log_lock);
    if (log_mpv_instance == global) {
        av_log_set_callback(av_log_default_callback);
        log_mpv_instance = NULL;
        talloc_free(log_root);
    }
    mp_mutex_unlock(&log_lock);
}

#define V(x) AV_VERSION_MAJOR(x), \
             AV_VERSION_MINOR(x), \
             AV_VERSION_MICRO(x)

struct lib {
    const char *name;
    unsigned buildv;
    unsigned runv;
};

void check_library_versions(struct mp_log *log, int v)
{
    const struct lib libs[] = {
        {"libavcodec",    LIBAVCODEC_VERSION_INT,    avcodec_version()},
#if HAVE_LIBAVDEVICE
        {"libavdevice",   LIBAVDEVICE_VERSION_INT,   avdevice_version()},
#endif
        {"libavfilter",   LIBAVFILTER_VERSION_INT,   avfilter_version()},
        {"libavformat",   LIBAVFORMAT_VERSION_INT,   avformat_version()},
        {"libavutil",     LIBAVUTIL_VERSION_INT,     avutil_version()},
        {"libswresample", LIBSWRESAMPLE_VERSION_INT, swresample_version()},
        {"libswscale",    LIBSWSCALE_VERSION_INT,    swscale_version()},
    };

    const char *runtime_version = av_version_info();
    mp_msg(log, v, "FFmpeg version: %s", FFMPEG_VERSION);
    if (strcmp(runtime_version, FFMPEG_VERSION))
        mp_msg(log, v, " (runtime %s)", runtime_version);
    mp_msg(log, v, "\n");
    mp_msg(log, v, "FFmpeg library versions:\n");

    for (int n = 0; n < MP_ARRAY_SIZE(libs); n++) {
        const struct lib *l = &libs[n];
        mp_msg(log, v, "   %-15s %d.%d.%d", l->name, V(l->buildv));
        if (l->buildv != l->runv)
            mp_msg(log, v, " (runtime %d.%d.%d)", V(l->runv));
        mp_msg(log, v, "\n");
        if (l->buildv > l->runv ||
            AV_VERSION_MAJOR(l->buildv) != AV_VERSION_MAJOR(l->runv))
        {
            mp_fatal(log, "%s: build version %d.%d.%d incompatible with runtime version %d.%d.%d\n",
                     l->name, V(l->buildv), V(l->runv));
            exit(1);
        }
    }
}

#undef V
