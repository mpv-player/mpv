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
#include "libavutil/avutil.h"
#include <libavutil/pixdesc.h>
#include "video/img_format.h"
#include "fmt-conversion.h"

static const struct {
    int fmt;
    enum PixelFormat pix_fmt;
} conversion_map[] = {
    {IMGFMT_ARGB, PIX_FMT_ARGB},
    {IMGFMT_BGRA, PIX_FMT_BGRA},
    {IMGFMT_BGR24, PIX_FMT_BGR24},
    {IMGFMT_BGR16_BE, PIX_FMT_RGB565BE},
    {IMGFMT_BGR16_LE, PIX_FMT_RGB565LE},
    {IMGFMT_BGR15_BE, PIX_FMT_RGB555BE},
    {IMGFMT_BGR15_LE, PIX_FMT_RGB555LE},
    {IMGFMT_BGR12_BE, PIX_FMT_RGB444BE},
    {IMGFMT_BGR12_LE, PIX_FMT_RGB444LE},
    {IMGFMT_BGR8,  PIX_FMT_RGB8},
    {IMGFMT_BGR4,  PIX_FMT_RGB4},
    {IMGFMT_MONO,  PIX_FMT_MONOBLACK},
    {IMGFMT_MONO_W,  PIX_FMT_MONOWHITE},
    {IMGFMT_RGB4_BYTE,  PIX_FMT_BGR4_BYTE},
    {IMGFMT_BGR4_BYTE,  PIX_FMT_RGB4_BYTE},
    {IMGFMT_RGB48_LE, PIX_FMT_RGB48LE},
    {IMGFMT_RGB48_BE, PIX_FMT_RGB48BE},
    {IMGFMT_ABGR, PIX_FMT_ABGR},
    {IMGFMT_RGBA, PIX_FMT_RGBA},
    {IMGFMT_RGB24, PIX_FMT_RGB24},
    {IMGFMT_RGB16_BE, PIX_FMT_BGR565BE},
    {IMGFMT_RGB16_LE, PIX_FMT_BGR565LE},
    {IMGFMT_RGB15_BE, PIX_FMT_BGR555BE},
    {IMGFMT_RGB15_LE, PIX_FMT_BGR555LE},
    {IMGFMT_RGB12_BE, PIX_FMT_BGR444BE},
    {IMGFMT_RGB12_LE, PIX_FMT_BGR444LE},
    {IMGFMT_RGB8,  PIX_FMT_BGR8},
    {IMGFMT_RGB4,  PIX_FMT_BGR4},
    {IMGFMT_PAL8,  PIX_FMT_PAL8},
    {IMGFMT_GBRP,  PIX_FMT_GBRP},
    {IMGFMT_GBRP9_BE,  PIX_FMT_GBRP9BE},
    {IMGFMT_GBRP9_LE,  PIX_FMT_GBRP9LE},
    {IMGFMT_GBRP10_BE, PIX_FMT_GBRP10BE},
    {IMGFMT_GBRP10_LE, PIX_FMT_GBRP10LE},
    {IMGFMT_GBRP16_BE, PIX_FMT_GBRP16BE},
    {IMGFMT_GBRP16_LE, PIX_FMT_GBRP16LE},
    {IMGFMT_YUYV,  PIX_FMT_YUYV422},
    {IMGFMT_UYVY,  PIX_FMT_UYVY422},
    {IMGFMT_NV12,  PIX_FMT_NV12},
    {IMGFMT_NV21,  PIX_FMT_NV21},
    {IMGFMT_Y8,    PIX_FMT_GRAY8},
    // Support really ancient ffmpeg versions (before e91946ed23dfbb)
    // Newer versions use PIX_FMT_GRAY8A
    {IMGFMT_YA8,   PIX_FMT_Y400A},
    {IMGFMT_Y16_LE, PIX_FMT_GRAY16LE},
    {IMGFMT_Y16_BE, PIX_FMT_GRAY16BE},
    {IMGFMT_410P,  PIX_FMT_YUV410P},
    {IMGFMT_420P,  PIX_FMT_YUV420P},
    {IMGFMT_411P,  PIX_FMT_YUV411P},
    {IMGFMT_422P,  PIX_FMT_YUV422P},
    {IMGFMT_444P,  PIX_FMT_YUV444P},
    {IMGFMT_440P,  PIX_FMT_YUV440P},

    {IMGFMT_420P16_LE,  PIX_FMT_YUV420P16LE},
    {IMGFMT_420P16_BE,  PIX_FMT_YUV420P16BE},
    {IMGFMT_420P9_LE,   PIX_FMT_YUV420P9LE},
    {IMGFMT_420P9_BE,   PIX_FMT_YUV420P9BE},
    {IMGFMT_420P10_LE,  PIX_FMT_YUV420P10LE},
    {IMGFMT_420P10_BE,  PIX_FMT_YUV420P10BE},
    {IMGFMT_422P10_LE,  PIX_FMT_YUV422P10LE},
    {IMGFMT_422P10_BE,  PIX_FMT_YUV422P10BE},
    {IMGFMT_444P9_BE ,  PIX_FMT_YUV444P9BE},
    {IMGFMT_444P9_LE ,  PIX_FMT_YUV444P9LE},
    {IMGFMT_444P10_BE,  PIX_FMT_YUV444P10BE},
    {IMGFMT_444P10_LE,  PIX_FMT_YUV444P10LE},
    {IMGFMT_422P16_LE,  PIX_FMT_YUV422P16LE},
    {IMGFMT_422P16_BE,  PIX_FMT_YUV422P16BE},
    {IMGFMT_422P9_LE,   PIX_FMT_YUV422P9LE},
    {IMGFMT_422P9_BE,   PIX_FMT_YUV422P9BE},
    {IMGFMT_444P16_LE,  PIX_FMT_YUV444P16LE},
    {IMGFMT_444P16_BE,  PIX_FMT_YUV444P16BE},

    // YUVJ are YUV formats that use the full Y range. Decoder color range
    // information is used instead. Deprecated in ffmpeg.
    {IMGFMT_420P,  PIX_FMT_YUVJ420P},
    {IMGFMT_422P,  PIX_FMT_YUVJ422P},
    {IMGFMT_444P,  PIX_FMT_YUVJ444P},
    {IMGFMT_440P,  PIX_FMT_YUVJ440P},

    {IMGFMT_420AP, PIX_FMT_YUVA420P},

#if LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 10, 0)
    {IMGFMT_422AP, PIX_FMT_YUVA422P},
    {IMGFMT_444AP, PIX_FMT_YUVA444P},

    {IMGFMT_420AP9_BE,  AV_PIX_FMT_YUVA420P9BE},
    {IMGFMT_420AP9_LE,  AV_PIX_FMT_YUVA420P9LE},
    {IMGFMT_420AP10_BE, AV_PIX_FMT_YUVA420P10BE},
    {IMGFMT_420AP10_LE, AV_PIX_FMT_YUVA420P10LE},
    {IMGFMT_420AP16_BE, AV_PIX_FMT_YUVA420P16BE},
    {IMGFMT_420AP16_LE, AV_PIX_FMT_YUVA420P16LE},

    {IMGFMT_422AP9_BE,  AV_PIX_FMT_YUVA422P9BE},
    {IMGFMT_422AP9_LE,  AV_PIX_FMT_YUVA422P9LE},
    {IMGFMT_422AP10_BE, AV_PIX_FMT_YUVA422P10BE},
    {IMGFMT_422AP10_LE, AV_PIX_FMT_YUVA422P10LE},
    {IMGFMT_422AP16_BE, AV_PIX_FMT_YUVA422P16BE},
    {IMGFMT_422AP16_LE, AV_PIX_FMT_YUVA422P16LE},

    {IMGFMT_444AP9_BE,  AV_PIX_FMT_YUVA444P9BE},
    {IMGFMT_444AP9_LE,  AV_PIX_FMT_YUVA444P9LE},
    {IMGFMT_444AP10_BE, AV_PIX_FMT_YUVA444P10BE},
    {IMGFMT_444AP10_LE, AV_PIX_FMT_YUVA444P10LE},
    {IMGFMT_444AP16_BE, AV_PIX_FMT_YUVA444P16BE},
    {IMGFMT_444AP16_LE, AV_PIX_FMT_YUVA444P16LE},
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 25, 0)
    // Libav added this already at 52.10.0
    {IMGFMT_XYZ12_LE,   AV_PIX_FMT_XYZ12LE},
    {IMGFMT_XYZ12_BE,   AV_PIX_FMT_XYZ12BE},
#endif

    // ffmpeg only
#if LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT > AV_VERSION_INT(52, 0, 0)
    {IMGFMT_420P12_LE,  PIX_FMT_YUV420P12LE},
    {IMGFMT_420P12_BE,  PIX_FMT_YUV420P12BE},
    {IMGFMT_420P14_LE,  PIX_FMT_YUV420P14LE},
    {IMGFMT_420P14_BE,  PIX_FMT_YUV420P14BE},
    {IMGFMT_422P12_LE,  PIX_FMT_YUV422P12LE},
    {IMGFMT_422P12_BE,  PIX_FMT_YUV422P12BE},
    {IMGFMT_422P14_LE,  PIX_FMT_YUV422P14LE},
    {IMGFMT_422P14_BE,  PIX_FMT_YUV422P14BE},
    {IMGFMT_444P12_BE,  PIX_FMT_YUV444P12BE},
    {IMGFMT_444P12_LE,  PIX_FMT_YUV444P12LE},
    {IMGFMT_444P14_BE,  PIX_FMT_YUV444P14BE},
    {IMGFMT_444P14_LE,  PIX_FMT_YUV444P14LE},

    {IMGFMT_GBRP12_BE, PIX_FMT_GBRP12BE},
    {IMGFMT_GBRP12_LE, PIX_FMT_GBRP12LE},
    {IMGFMT_GBRP14_BE, PIX_FMT_GBRP14BE},
    {IMGFMT_GBRP14_LE, PIX_FMT_GBRP14LE},

    {IMGFMT_BGR0,  PIX_FMT_BGR0},
    {IMGFMT_0RGB,  PIX_FMT_0RGB},
    {IMGFMT_RGB0,  PIX_FMT_RGB0},
    {IMGFMT_0BGR,  PIX_FMT_0BGR},
    {IMGFMT_BGR0,  PIX_FMT_BGR0},

    {IMGFMT_RGBA64_BE,  PIX_FMT_RGBA64BE},
    {IMGFMT_RGBA64_LE,  PIX_FMT_RGBA64LE},
    {IMGFMT_BGRA64_BE,  PIX_FMT_BGRA64BE},
    {IMGFMT_BGRA64_LE,  PIX_FMT_BGRA64LE},
#endif

    {IMGFMT_VDPAU_MPEG1,     PIX_FMT_VDPAU_MPEG1},
    {IMGFMT_VDPAU_MPEG2,     PIX_FMT_VDPAU_MPEG2},
    {IMGFMT_VDPAU_H264,      PIX_FMT_VDPAU_H264},
    {IMGFMT_VDPAU_WMV3,      PIX_FMT_VDPAU_WMV3},
    {IMGFMT_VDPAU_VC1,       PIX_FMT_VDPAU_VC1},
    {IMGFMT_VDPAU_MPEG4,     PIX_FMT_VDPAU_MPEG4},
    {0, PIX_FMT_NONE}
};

enum PixelFormat imgfmt2pixfmt(int fmt)
{
    if (fmt == IMGFMT_NONE)
        return PIX_FMT_NONE;

    int i;
    enum PixelFormat pix_fmt;
    for (i = 0; conversion_map[i].fmt; i++)
        if (conversion_map[i].fmt == fmt)
            break;
    pix_fmt = conversion_map[i].pix_fmt;
    if (pix_fmt == PIX_FMT_NONE)
        mp_msg(MSGT_GLOBAL, MSGL_V, "Unsupported format %s\n", vo_format_name(fmt));
    return pix_fmt;
}

int pixfmt2imgfmt(enum PixelFormat pix_fmt)
{
    if (pix_fmt == PIX_FMT_NONE)
        return IMGFMT_NONE;

    int i;
    for (i = 0; conversion_map[i].pix_fmt != PIX_FMT_NONE; i++)
        if (conversion_map[i].pix_fmt == pix_fmt)
            break;
    int fmt = conversion_map[i].fmt;
    if (!fmt) {
        const char *fmtname = av_get_pix_fmt_name(pix_fmt);
        mp_msg(MSGT_GLOBAL, MSGL_ERR, "Unsupported PixelFormat %s (%d)\n",
               fmtname ? fmtname : "INVALID", pix_fmt);
    }
    return fmt;
}
