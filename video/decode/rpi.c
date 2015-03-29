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

#include "lavc.h"
#include "common/common.h"

static int init_decoder(struct lavc_ctx *ctx, int fmt, int w, int h)
{
    return 0;
}

static void uninit(struct lavc_ctx *ctx)
{
}

static int init(struct lavc_ctx *ctx)
{
    return 0;
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    if (strcmp(decoder, "h264") != 0)
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

static const char *get_codec(struct lavc_ctx *ctx)
{
    return "h264_mmal";
}

const struct vd_lavc_hwdec mp_vd_lavc_rpi = {
    .type = HWDEC_RPI,
    .image_format = IMGFMT_MMAL,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .get_codec = get_codec,
};
