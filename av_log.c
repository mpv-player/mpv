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

#include "config.h"
#include "mp_msg.h"
#include <libavutil/log.h>

#ifdef CONFIG_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif

static int av_log_level_to_mp_level(int av_level)
{
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

#ifdef CONFIG_FFMPEG
    if (!strcmp(avc->class_name, "AVCodecContext")) {
        AVCodecContext *s = ptr;
        if (s->codec) {
            if (s->codec->type == CODEC_TYPE_AUDIO) {
                if (s->codec->decode)
                    return MSGT_DECAUDIO;
            } else if (s->codec->type == CODEC_TYPE_VIDEO) {
                if (s->codec->decode)
                    return MSGT_DECVIDEO;
            }
            // FIXME subtitles, encoders
            // What msgt for them? There is nothing appropriate...
        }
        return MSGT_FIXME;
    }
#endif

#ifdef CONFIG_FFMPEG
    if (!strcmp(avc->class_name, "AVFormatContext")) {
        AVFormatContext *s = ptr;
        if (s->iformat)
            return MSGT_DEMUXER;
        else if (s->oformat)
            return MSGT_MUXER;
        return MSGT_FIXME;
    }
#endif

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

void set_av_log_callback(void)
{
  av_log_set_callback(mp_msg_av_log_callback);
}
