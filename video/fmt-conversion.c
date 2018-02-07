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

#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>

#include "video/img_format.h"
#include "fmt-conversion.h"
#include "config.h"

static const struct {
    int fmt;
    enum AVPixelFormat pix_fmt;
} conversion_map[] = {
    {IMGFMT_ARGB, AV_PIX_FMT_ARGB},
    {IMGFMT_BGRA, AV_PIX_FMT_BGRA},
    {IMGFMT_BGR24, AV_PIX_FMT_BGR24},
    {IMGFMT_RGB565, AV_PIX_FMT_RGB565},
    {IMGFMT_ABGR, AV_PIX_FMT_ABGR},
    {IMGFMT_RGBA, AV_PIX_FMT_RGBA},
    {IMGFMT_RGB24, AV_PIX_FMT_RGB24},
    {IMGFMT_UYVY,  AV_PIX_FMT_UYVY422},
    {IMGFMT_NV12,  AV_PIX_FMT_NV12},
    {IMGFMT_Y8,    AV_PIX_FMT_GRAY8},
    {IMGFMT_Y16, AV_PIX_FMT_GRAY16},
    {IMGFMT_420P,  AV_PIX_FMT_YUV420P},
    {IMGFMT_444P,  AV_PIX_FMT_YUV444P},

    // YUVJ are YUV formats that use the full Y range. Decoder color range
    // information is used instead. Deprecated in ffmpeg.
    {IMGFMT_420P,  AV_PIX_FMT_YUVJ420P},
    {IMGFMT_444P,  AV_PIX_FMT_YUVJ444P},

#if LIBAVUTIL_VERSION_MICRO >= 100
    {IMGFMT_BGR0,  AV_PIX_FMT_BGR0},
    {IMGFMT_0RGB,  AV_PIX_FMT_0RGB},
    {IMGFMT_RGB0,  AV_PIX_FMT_RGB0},
    {IMGFMT_0BGR,  AV_PIX_FMT_0BGR},
#else
    {IMGFMT_BGR0,  AV_PIX_FMT_BGRA},
    {IMGFMT_0RGB,  AV_PIX_FMT_ARGB},
    {IMGFMT_RGB0,  AV_PIX_FMT_RGBA},
    {IMGFMT_0BGR,  AV_PIX_FMT_ABGR},
#endif

    {IMGFMT_RGBA64, AV_PIX_FMT_RGBA64},

    {IMGFMT_VDPAU, AV_PIX_FMT_VDPAU},
#if HAVE_VIDEOTOOLBOX_HWACCEL
    {IMGFMT_VIDEOTOOLBOX,   AV_PIX_FMT_VIDEOTOOLBOX},
#endif
#if HAVE_ANDROID
    {IMGFMT_MEDIACODEC, AV_PIX_FMT_MEDIACODEC},
#endif
    {IMGFMT_VAAPI, AV_PIX_FMT_VAAPI},
    {IMGFMT_DXVA2, AV_PIX_FMT_DXVA2_VLD},
#if HAVE_D3D_HWACCEL
    {IMGFMT_D3D11, AV_PIX_FMT_D3D11},
#endif
    {IMGFMT_MMAL, AV_PIX_FMT_MMAL},
#if HAVE_CUDA_HWACCEL
    {IMGFMT_CUDA, AV_PIX_FMT_CUDA},
#endif
    {IMGFMT_P010, AV_PIX_FMT_P010},
#if HAVE_DRMPRIME
    {IMGFMT_DRMPRIME, AV_PIX_FMT_DRM_PRIME},
#endif

    {0, AV_PIX_FMT_NONE}
};

enum AVPixelFormat imgfmt2pixfmt(int fmt)
{
    if (fmt == IMGFMT_NONE)
        return AV_PIX_FMT_NONE;

    if (fmt >= IMGFMT_AVPIXFMT_START && fmt < IMGFMT_AVPIXFMT_END) {
        enum AVPixelFormat pixfmt = fmt - IMGFMT_AVPIXFMT_START;
        // Avoid duplicate format - each format must be unique.
        int mpfmt = pixfmt2imgfmt(pixfmt);
        if (mpfmt == fmt)
            return pixfmt;
        return AV_PIX_FMT_NONE;
    }

    for (int i = 0; conversion_map[i].fmt; i++) {
        if (conversion_map[i].fmt == fmt)
            return conversion_map[i].pix_fmt;
    }
    return AV_PIX_FMT_NONE;
}

int pixfmt2imgfmt(enum AVPixelFormat pix_fmt)
{
    if (pix_fmt == AV_PIX_FMT_NONE)
        return IMGFMT_NONE;

    for (int i = 0; conversion_map[i].pix_fmt != AV_PIX_FMT_NONE; i++) {
        if (conversion_map[i].pix_fmt == pix_fmt)
            return conversion_map[i].fmt;
    }

    int generic = IMGFMT_AVPIXFMT_START + pix_fmt;
    if (generic < IMGFMT_AVPIXFMT_END)
        return generic;

    return 0;
}
