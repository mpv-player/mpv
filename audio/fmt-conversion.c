/*
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

#include "core/mp_msg.h"
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>
#include "format.h"
#include "fmt-conversion.h"

static const struct {
    enum AVSampleFormat sample_fmt;
    int fmt;
} audio_conversion_map[] = {
    {AV_SAMPLE_FMT_U8,    AF_FORMAT_U8},
    {AV_SAMPLE_FMT_S16,   AF_FORMAT_S16_NE},
    {AV_SAMPLE_FMT_S32,   AF_FORMAT_S32_NE},
    {AV_SAMPLE_FMT_FLT,   AF_FORMAT_FLOAT_NE},

    {AV_SAMPLE_FMT_NONE,  0},
};

enum AVSampleFormat af_to_avformat(int fmt)
{
    int i;
    enum AVSampleFormat sample_fmt;
    for (i = 0; audio_conversion_map[i].fmt; i++)
        if (audio_conversion_map[i].fmt == fmt)
            break;
    sample_fmt = audio_conversion_map[i].sample_fmt;
    if (sample_fmt == AF_FORMAT_UNKNOWN)
        mp_msg(MSGT_GLOBAL, MSGL_V, "Unsupported sample format: %s\n",
                af_fmt2str_short(fmt));
    return sample_fmt;
}

int af_from_avformat(enum AVSampleFormat sample_fmt)
{
    int i;
    for (i = 0; audio_conversion_map[i].fmt; i++)
        if (audio_conversion_map[i].sample_fmt == sample_fmt)
            break;
    int fmt = audio_conversion_map[i].fmt;
    if (!fmt) {
        const char *fmtname = av_get_sample_fmt_name(sample_fmt);
        mp_msg(MSGT_GLOBAL, MSGL_ERR, "Unsupported AVSampleFormat %s (%d)\n",
               fmtname ? fmtname : "INVALID", sample_fmt);
    }
    return fmt;
}
