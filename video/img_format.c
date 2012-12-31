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

#include <assert.h>

#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>

#include "config.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"

#include <string.h>

struct mp_imgfmt_entry mp_imgfmt_list[] = {
    {"444p16le", IMGFMT_444P16_LE},
    {"444p16be", IMGFMT_444P16_BE},
    {"444p14le", IMGFMT_444P14_LE},
    {"444p14be", IMGFMT_444P14_BE},
    {"444p12le", IMGFMT_444P12_LE},
    {"444p12be", IMGFMT_444P12_BE},
    {"444p10le", IMGFMT_444P10_LE},
    {"444p10be", IMGFMT_444P10_BE},
    {"444p9le", IMGFMT_444P9_LE},
    {"444p9be", IMGFMT_444P9_BE},
    {"422p16le", IMGFMT_422P16_LE},
    {"422p16be", IMGFMT_422P16_BE},
    {"422p14le", IMGFMT_422P14_LE},
    {"422p14be", IMGFMT_422P14_BE},
    {"422p12le", IMGFMT_422P12_LE},
    {"422p12be", IMGFMT_422P12_BE},
    {"422p10le", IMGFMT_422P10_LE},
    {"422p10be", IMGFMT_422P10_BE},
    {"422p9le",  IMGFMT_422P9_LE},
    {"422p9be",  IMGFMT_422P9_BE},
    {"420p16le", IMGFMT_420P16_LE},
    {"420p16be", IMGFMT_420P16_BE},
    {"420p14le", IMGFMT_420P14_LE},
    {"420p14be", IMGFMT_420P14_BE},
    {"420p12le", IMGFMT_420P12_LE},
    {"420p12be", IMGFMT_420P12_BE},
    {"420p10le", IMGFMT_420P10_LE},
    {"420p10be", IMGFMT_420P10_BE},
    {"420p9le", IMGFMT_420P9_LE},
    {"420p9be", IMGFMT_420P9_BE},
    {"444p16", IMGFMT_444P16},
    {"444p14", IMGFMT_444P14},
    {"444p12", IMGFMT_444P12},
    {"444p10", IMGFMT_444P10},
    {"444p9", IMGFMT_444P9},
    {"422p16", IMGFMT_422P16},
    {"422p14", IMGFMT_422P14},
    {"422p12", IMGFMT_422P12},
    {"422p10", IMGFMT_422P10},
    {"420p14", IMGFMT_420P14},
    {"420p12", IMGFMT_420P12},
    {"420p10", IMGFMT_420P10},
    {"420p9", IMGFMT_420P9},
    {"420p16", IMGFMT_420P16},
    {"420a", IMGFMT_420A},
    {"444p", IMGFMT_444P},
    {"422p", IMGFMT_422P},
    {"411p", IMGFMT_411P},
    {"440p", IMGFMT_440P},
    {"yuy2", IMGFMT_YUY2},
    {"yvyu", IMGFMT_YVYU},
    {"uyvy", IMGFMT_UYVY},
    {"yvu9", IMGFMT_YVU9},
    {"if09", IMGFMT_IF09},
    {"yv12", IMGFMT_YV12},
    {"i420", IMGFMT_I420},
    {"iyuv", IMGFMT_IYUV},
    {"clpl", IMGFMT_CLPL},
    {"hm12", IMGFMT_HM12},
    {"y800", IMGFMT_Y800},
    {"y8", IMGFMT_Y8},
    {"y16ne", IMGFMT_Y16},
    {"y16le", IMGFMT_Y16LE},
    {"y16be", IMGFMT_Y16BE},
    {"nv12", IMGFMT_NV12},
    {"nv21", IMGFMT_NV21},
    {"bgr24", IMGFMT_BGR24},
    {"bgr32", IMGFMT_BGR32},
    {"bgr16", IMGFMT_BGR16},
    {"bgr15", IMGFMT_BGR15},
    {"bgr12", IMGFMT_BGR12},
    {"bgr8", IMGFMT_BGR8},
    {"bgr4", IMGFMT_BGR4},
    {"bg4b", IMGFMT_BG4B},
    {"bgr1", IMGFMT_BGR1},
    {"rgb48be", IMGFMT_RGB48BE},
    {"rgb48le", IMGFMT_RGB48LE},
    {"rgb48ne", IMGFMT_RGB48NE},
    {"rgb24", IMGFMT_RGB24},
    {"rgb32", IMGFMT_RGB32},
    {"rgb16", IMGFMT_RGB16},
    {"rgb15", IMGFMT_RGB15},
    {"rgb12", IMGFMT_RGB12},
    {"rgb8", IMGFMT_RGB8},
    {"rgb4", IMGFMT_RGB4},
    {"rg4b", IMGFMT_RG4B},
    {"rgb1", IMGFMT_RGB1},
    {"rgba", IMGFMT_RGBA},
    {"argb", IMGFMT_ARGB},
    {"bgra", IMGFMT_BGRA},
    {"abgr", IMGFMT_ABGR},
    {"bgr0", IMGFMT_BGR0},
    {"gbrp", IMGFMT_GBRP},
    {"mjpeg", IMGFMT_MJPEG},
    {"mjpg", IMGFMT_MJPEG},
    {"vdpau_h264", IMGFMT_VDPAU_H264},
    {"vdpau_mpeg1", IMGFMT_VDPAU_MPEG1},
    {"vdpau_mpeg2", IMGFMT_VDPAU_MPEG2},
    {"vdpau_mpeg4", IMGFMT_VDPAU_MPEG4},
    {"vdpau_wmv3", IMGFMT_VDPAU_WMV3},
    {"vdpau_vc1", IMGFMT_VDPAU_VC1},
    {0}
};

const char *vo_format_name(int format)
{
    const char *name = mp_imgfmt_to_name(format);
    if (name)
        return name;
    static char unknown_format[20];
    snprintf(unknown_format, 20, "Unknown 0x%04x", format);
    return unknown_format;
}

int mp_get_chroma_shift(int format, int *x_shift, int *y_shift,
                        int *component_bits)
{
    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(format);
    if (fmt.id && (fmt.flags & MP_IMGFLAG_YUV_P)) {
        if (x_shift)
            *x_shift = fmt.xs[1];
        if (y_shift)
            *y_shift = fmt.ys[1];
        if (component_bits)
            *component_bits = fmt.plane_bits;
        return fmt.avg_bpp;
    }
    return 0;
}

unsigned int mp_imgfmt_from_name(bstr name, bool allow_hwaccel)
{
    if (bstr_startswith0(name, "0x")) {
        bstr rest;
        unsigned int fmt = bstrtoll(name, &rest, 16);
        if (rest.len == 0)
            return fmt;
    }
    for(struct mp_imgfmt_entry *p = mp_imgfmt_list; p->name; ++p) {
        if(!bstrcasecmp0(name, p->name)) {
            if (!allow_hwaccel && IMGFMT_IS_HWACCEL(p->fmt))
                return 0;
            return p->fmt;
        }
    }
    return 0;
}

const char *mp_imgfmt_to_name(unsigned int fmt)
{
    struct mp_imgfmt_entry *p = mp_imgfmt_list;
    for(; p->name; ++p) {
        if(p->fmt == fmt)
            return p->name;
    }
    return NULL;
}

static int comp_bit_order(const AVPixFmtDescriptor *pd, int bpp, int c)
{
    int el_size = (pd->flags & PIX_FMT_BITSTREAM) ? 1 : 8;
    // NOTE: offset_plus1 can be 0
    int offset = (((int)pd->comp[c].offset_plus1) - 1) * el_size;
    int read_depth = pd->comp[c].shift + pd->comp[c].depth_minus1 + 1;
    if (read_depth <= 8 && !(pd->flags & PIX_FMT_BITSTREAM))
        offset += 8 * !!(pd->flags & PIX_FMT_BE);
    offset += pd->comp[c].shift;
    // revert ffmpeg's bullshit hack that mixes byte and bit access
    if ((pd->flags & PIX_FMT_BE) && bpp <= 16 && read_depth <= 8)
        offset = (8 + offset) % 16;
    return offset;
}

static struct mp_imgfmt_desc get_avutil_fmt(enum PixelFormat fmt)
{
    const AVPixFmtDescriptor *pd = &av_pix_fmt_descriptors[fmt];
    int mpfmt = pixfmt2imgfmt(fmt);
    if (!pd || !mpfmt)
        return (struct mp_imgfmt_desc) {0};

    struct mp_imgfmt_desc desc = {
        .id = mpfmt,
        .avformat = fmt,
        .name = mp_imgfmt_to_name(desc.id),
        .chroma_xs = pd->log2_chroma_w,
        .chroma_ys = pd->log2_chroma_h,
    };

    int planedepth[4] = {0};
    int xs[4] = {0, pd->log2_chroma_w, pd->log2_chroma_w, 0};
    int ys[4] = {0, pd->log2_chroma_h, pd->log2_chroma_h, 0};
    int el_size = (pd->flags & PIX_FMT_BITSTREAM) ? 1 : 8;
    for (int c = 0; c < pd->nb_components; c++) {
        AVComponentDescriptor d = pd->comp[c];
        // multiple components per plane -> Y is definitive, ignore chroma
        if (!desc.bpp[d.plane])
            desc.bpp[d.plane] = (d.step_minus1 + 1) * el_size;
        planedepth[d.plane] += d.depth_minus1 + 1;
    }

    int avgbpp16 = 0;
    for (int p = 0; p < 4; p++)
        avgbpp16 += (16 * desc.bpp[p]) >> xs[p] >> ys[p];
    desc.avg_bpp = avgbpp16 / 16;
    //assert(desc.avg_bpp == av_get_padded_bits_per_pixel(pd));

    for (int p = 0; p < 4; p++) {
        if (desc.bpp[p])
            desc.num_planes++;
    }

    if (desc.bpp[0] <= 8 || !(pd->flags & PIX_FMT_BE))
        desc.flags |= MP_IMGFLAG_NE;

    desc.plane_bits = planedepth[0];

    if (!(pd->flags & PIX_FMT_RGB) && !(pd->flags & PIX_FMT_HWACCEL) &&
        fmt != PIX_FMT_MONOWHITE && fmt != PIX_FMT_MONOBLACK &&
        fmt != PIX_FMT_PAL8)
    {
        desc.flags |= MP_IMGFLAG_YUV;
    }

#ifdef PIX_FMT_ALPHA
    if (pd->flags & PIX_FMT_ALPHA)
        desc.flags |= MP_IMGFLAG_ALPHA;
#else
    if (desc.num_planes > 3)
        desc.flags |= MP_IMGFLAG_ALPHA;
#endif

    if (desc.num_planes > 1)
        desc.flags |= MP_IMGFLAG_PLANAR;

    if (desc.flags & MP_IMGFLAG_YUV) {
        bool same_depth = true;
        for (int p = 0; p < desc.num_planes; p++) {
            same_depth &= planedepth[p] == planedepth[0] &&
                          desc.bpp[p] == desc.bpp[0];
        }
        if (same_depth && pd->nb_components == desc.num_planes)
            desc.flags |= MP_IMGFLAG_YUV_P;
    }

    if ((pd->flags & PIX_FMT_RGB) && desc.num_planes == 1
        && pd->nb_components >= 3)
    {
        // RGB vs. BGR component order, as distinguished by mplayer:
        // - for byte accessed formats (RGB24, RGB48), the order of bytes
        //   determines RGB/BGR (e.g. R is first byte -> RGB)
        // - for bit accessed formats (RGB32, RGB16, etc.), the order of bits
        //   determines BGR/RGB (e.g. R is LSB -> RGB)
        // - formats like IMGFMT_RGBA are aliases to allow byte access to bit-
        //   accessed formats (IMGFMT_RGBA is RGB32 on LE, BGR32|128 on BE)
        //   (ffmpeg does it the other way around, and defines bit-access
        //   aliases to byte-accessed formats)
        int b = desc.bpp[0];
        bool swap = comp_bit_order(pd, b, 0) > comp_bit_order(pd, b, 1);
        if ((desc.bpp[0] == 24 || desc.bpp[0] > 32) && BYTE_ORDER == BIG_ENDIAN)
            swap = !swap; // byte accessed
        if (swap)
            desc.flags |= MP_IMGFLAG_SWAPPED;
    }

    // compatibility with old mp_image_setfmt()

    switch (desc.id) {
    case IMGFMT_I420:
    case IMGFMT_IYUV:
        desc.flags |= MP_IMGFLAG_SWAPPED; // completely pointless
        break;
    case IMGFMT_UYVY:
        desc.flags |= MP_IMGFLAG_SWAPPED; // for vf_mpi_clear()
        /* fallthrough */
    case IMGFMT_YUY2:
        desc.chroma_ys = 1; // ???
        break;
    case IMGFMT_Y8:
    case IMGFMT_Y800:
    case IMGFMT_Y16LE:
    case IMGFMT_Y16BE:
        // probably for vo_opengl, and possibly more code using Y8
        desc.chroma_xs = desc.chroma_ys = 31;
        break;
    case IMGFMT_NV12:
        desc.flags |= MP_IMGFLAG_SWAPPED; // completely pointless
        /* fallthrough */
    case IMGFMT_NV21:
        // some hack to make cropping code etc. work? (doesn't work anyway)
        desc.chroma_xs = 0;
        desc.chroma_ys = 1;
        break;
    case IMGFMT_RGB4:
    case IMGFMT_BGR4:
    case IMGFMT_BGR1:
        desc.flags ^= MP_IMGFLAG_SWAPPED; // ???
        break;
    case IMGFMT_BGR0:
        desc.flags &= ~MP_IMGFLAG_SWAPPED; // not covered by IS_RGB/IS_BGR
        break;
    }

    if (pd->flags & PIX_FMT_HWACCEL)
        desc.chroma_xs = desc.chroma_ys = 0;

    for (int p = 0; p < desc.num_planes; p++) {
        desc.xs[p] = (p == 1 || p == 2) ? desc.chroma_xs : 0;
        desc.ys[p] = (p == 1 || p == 2) ? desc.chroma_ys : 0;
    }

    return desc;
}

struct mp_imgfmt_desc mp_imgfmt_get_desc(unsigned int out_fmt)
{
    struct mp_imgfmt_desc fmt = {0};
    enum PixelFormat avfmt = imgfmt2pixfmt(out_fmt);
    if (avfmt != PIX_FMT_NONE)
        fmt = get_avutil_fmt(avfmt);
    if (!fmt.id) {
        mp_msg(MSGT_DECVIDEO, MSGL_V, "mp_image: unknown out_fmt: 0x%X\n",
               out_fmt);
    }
    return fmt;
}
