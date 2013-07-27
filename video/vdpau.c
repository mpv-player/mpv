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

#include "vdpau.h"

#include "video/img_format.h"

// Check whether vdpau initialization and preemption status is ok and we can
// proceed normally.
bool mp_vdpau_status_ok(struct mp_vdpau_ctx *ctx)
{
    return ctx->status_ok(ctx);
}

struct mp_image *mp_vdpau_get_video_surface(struct mp_vdpau_ctx *ctx, int fmt,
                                            VdpChromaType chroma, int w, int h)
{
    return ctx->get_video_surface(ctx, fmt, chroma, w, h);
}

bool mp_vdpau_get_format(int imgfmt, VdpChromaType *out_chroma_type,
                         VdpYCbCrFormat *out_pixel_format)
{
    VdpChromaType chroma = VDP_CHROMA_TYPE_420;
    VdpYCbCrFormat ycbcr = (VdpYCbCrFormat)-1;

    switch (imgfmt) {
    case IMGFMT_420P:
        ycbcr = VDP_YCBCR_FORMAT_YV12;
        break;
    case IMGFMT_NV12:
        ycbcr = VDP_YCBCR_FORMAT_NV12;
        break;
    case IMGFMT_YUYV:
        ycbcr = VDP_YCBCR_FORMAT_YUYV;
        chroma = VDP_CHROMA_TYPE_422;
        break;
    case IMGFMT_UYVY:
        ycbcr = VDP_YCBCR_FORMAT_UYVY;
        chroma = VDP_CHROMA_TYPE_422;
        break;
    case IMGFMT_VDPAU_MPEG1:
    case IMGFMT_VDPAU_MPEG2:
    case IMGFMT_VDPAU_H264:
    case IMGFMT_VDPAU_WMV3:
    case IMGFMT_VDPAU_VC1:
    case IMGFMT_VDPAU_MPEG4:
    case IMGFMT_VDPAU:
        break;
    default:
        return false;
    }

    if (out_chroma_type)
        *out_chroma_type = chroma;
    if (out_pixel_format)
        *out_pixel_format = ycbcr;
    return true;
}
