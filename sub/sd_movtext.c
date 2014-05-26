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

#include <stdlib.h>
#include <assert.h>

#include <libavutil/intreadwrite.h>
#include <libavutil/common.h>

#include "sd.h"

static bool supports_format(const char *format)
{
    return format && strcmp(format, "mov_text") == 0;
}

static int init(struct sd *sd)
{
    sd->output_codec = "text";
    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    unsigned char *data = packet->buffer;
    int len = packet->len;
    if (len < 2)
        return;
    len = FFMIN(len - 2, AV_RB16(data));
    data += 2;
    if (len > 0)
        sd_conv_add_packet(sd, data, len, packet->pts, packet->duration);
}

const struct sd_functions sd_movtext = {
    .name = "movtext",
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_converted = sd_conv_def_get_converted,
    .reset = sd_conv_def_reset,
};
