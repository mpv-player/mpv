/*
 * av_log to mp_msg converter
 * Copyright (C) 2006 Michael Niedermayer
 * Copyright (C) 2009 Uoti Urpala
 *
 * This file is part of MPlayer.
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

#include <stdlib.h>
#include <stdbool.h>

#include "av_log.h"
#include "config.h"
#include "core/mp_msg.h"
#include <libavutil/avutil.h>
#include <libavutil/log.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#ifdef CONFIG_LIBAVDEVICE
#include <libavdevice/avdevice.h>
#endif

#ifdef CONFIG_LIBAVFILTER
#include <libavfilter/avfilter.h>
#endif

static int av_log_level_to_mp_level(int av_level)
{
    if (av_level > AV_LOG_VERBOSE)
        return MSGL_DBG2;
    if (av_level > AV_LOG_INFO)
        return MSGL_V;
    if (av_level > AV_LOG_WARNING)
        return MSGL_INFO;
    if (av_level > AV_LOG_ERROR)
        return MSGL_WARN;
    if (av_level > AV_LOG_FATAL)
        return MSGL_ERR;
    return MSGL_FATAL;
}

static int extract_msg_type_from_ctx(void *ptr)
{
    if (!ptr)
        return MSGT_FIXME;

    AVClass *avc = *(AVClass **)ptr;
    if (!avc) {
        mp_msg(MSGT_FIXME, MSGL_WARN,
               "av_log callback called with bad parameters (NULL AVClass).\n"
               "This is a bug in one of Libav/FFmpeg libraries used.\n");
        return MSGT_FIXME;
    }

    if (!strcmp(avc->class_name, "AVCodecContext")) {
        AVCodecContext *s = ptr;
        if (s->codec) {
            if (s->codec->type == AVMEDIA_TYPE_AUDIO) {
                if (s->codec->decode)
                    return MSGT_DECAUDIO;
            } else if (s->codec->type == AVMEDIA_TYPE_VIDEO) {
                if (s->codec->decode)
                    return MSGT_DECVIDEO;
            }
            // FIXME subtitles, encoders
            // What msgt for them? There is nothing appropriate...
        }
        return MSGT_FIXME;
    }

    if (!strcmp(avc->class_name, "AVFormatContext")) {
        AVFormatContext *s = ptr;
        if (s->iformat)
            return MSGT_DEMUXER;
        else if (s->oformat)
            return MSGT_MUXER;
        return MSGT_FIXME;
    }

    return MSGT_FIXME;
}

static void mp_msg_av_log_callback(void *ptr, int level, const char *fmt,
                                   va_list vl)
{
    static bool print_prefix = 1;
    AVClass *avc = ptr ? *(AVClass **)ptr : NULL;
    int mp_level = av_log_level_to_mp_level(level);
    int type = extract_msg_type_from_ctx(ptr);

    if (!mp_msg_test(type, mp_level))
        return;

    if (print_prefix && avc)
        mp_msg(type, mp_level, "[%s @ %p]", avc->item_name(ptr), avc);
    print_prefix = fmt[strlen(fmt) - 1] == '\n';

    mp_msg_va(type, mp_level, fmt, vl);
}

void init_libav(void)
{
    av_log_set_callback(mp_msg_av_log_callback);
    avcodec_register_all();
    av_register_all();
    avformat_network_init();

#ifdef CONFIG_LIBAVFILTER
    avfilter_register_all();
#endif
#ifdef CONFIG_LIBAVDEVICE
    avdevice_register_all();
#endif
}

#define V(x) (x)>>16, (x)>>8 & 255, (x) & 255
static void print_version(char *name, unsigned buildv, unsigned runv)
{

    if (buildv == runv)
        mp_msg(MSGT_CPLAYER, MSGL_V, "Compiled against %s version %d.%d.%d\n",
               name, V(buildv));
    else
        mp_msg(MSGT_CPLAYER, MSGL_V, "Compiled against %s version %d.%d.%d "
               "(runtime %d.%d.%d)\n", name, V(buildv), V(runv));
}
#undef V

void print_libav_versions(void)
{
    print_version("libavutil", LIBAVUTIL_VERSION_INT, avutil_version());
    print_version("libavcodec", LIBAVCODEC_VERSION_INT, avcodec_version());
    print_version("libavformat", LIBAVFORMAT_VERSION_INT, avformat_version());
    print_version("libswscale", LIBSWSCALE_VERSION_INT, swscale_version());
}
