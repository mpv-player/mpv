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

#include "lavc.h"
#include "common/common.h"

static const char *const codecs[][2] = {
    {"h264",        "h264_mediacodec"},
    {0}
};

static const char *map_codec(const char *c)
{
    for (int n = 0; codecs[n][0]; n++) {
        if (c && strcmp(codecs[n][0], c) == 0)
            return codecs[n][1];
    }
    return NULL;
}

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
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
    return map_codec(decoder) ? 0 : HWDEC_ERR_NO_CODEC;
}

static const char *get_codec(struct lavc_ctx *ctx, const char *codec)
{
    return map_codec(codec);
}

const struct vd_lavc_hwdec mp_vd_lavc_mediacodec = {
    .type = HWDEC_MEDIACODEC,
    .image_format = IMGFMT_NV12,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .get_codec = get_codec,
};
