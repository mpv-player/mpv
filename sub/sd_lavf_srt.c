/*
 * This file is part of mpv.
 *
 * SRT timestamp parsing code lifted from FFmpeg srtdec.c (LGPL).
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

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "misc/bstr.h"
#include "sd.h"

/*
 * Background:
 *
 * Libav's .srt demuxer outputs packets that contain parts of the subtitle
 * event header. Also, the packet duration is not set (they don't parse it
 * on the demuxer side). As a result, the srt demuxer is useless.
 *
 * However, we can fix it by parsing the header, which spares us from writing
 * a full SRT demuxer.
 *
 * Newer versions of FFmpeg do not have this problem. To avoid compatibility
 * problems, they changed the codec name from "srt" to "subrip".
 *
 * Summary: this is a hack for broken SRT stuff in Libav.
 *
 */

static bool supports_format(const char *format)
{
    return format && strcmp(format, "srt") == 0;
}

static int init(struct sd *sd)
{
    sd->output_codec = "subrip";
    return 0;
}

static bool parse_pts(bstr header, double *duration)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%.*s", BSTR_P(header));
    int hh1, mm1, ss1, ms1;
    int hh2, mm2, ss2, ms2;
    if (sscanf(buf, "%d:%2d:%2d%*1[,.]%3d --> %d:%2d:%2d%*1[,.]%3d",
               &hh1, &mm1, &ss1, &ms1, &hh2, &mm2, &ss2, &ms2) >= 8)
    {
        int64_t start = (hh1*3600LL + mm1*60LL + ss1) * 1000LL + ms1;
        int64_t end   = (hh2*3600LL + mm2*60LL + ss2) * 1000LL + ms2;
        *duration = (end - start) / 1000.0;
        return true;
    }
    return false;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    bstr data = {packet->buffer, packet->len};
    // Remove the broken header. It's usually on the second or first line.
    bstr left = data;
    while (left.len) {
        bstr line = bstr_getline(left, &left);
        if (parse_pts(line, &packet->duration)) {
            data = left;
            break;
        }
    }
    sd_conv_add_packet(sd, data.start, data.len, packet->pts, packet->duration);
}

const struct sd_functions sd_lavf_srt = {
    .name = "lavf_srt",
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_converted = sd_conv_def_get_converted,
    .reset = sd_conv_def_reset,
};
