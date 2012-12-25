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
#include <string.h>

#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"

#define FMT(string, id)                                 \
    {string,            id},

#define FMT_ENDIAN(string, id)                          \
    {string,            id},                            \
    {string "le",       MP_CONCAT(id, _LE)},            \
    {string "be",       MP_CONCAT(id, _BE)},            \

struct mp_imgfmt_entry mp_imgfmt_list[] = {
    FMT("y8",                   IMGFMT_Y8)
    FMT_ENDIAN("y16",           IMGFMT_Y16)
    FMT("yuyv",                 IMGFMT_YUYV)
    FMT("uyvy",                 IMGFMT_UYVY)
    FMT("nv12",                 IMGFMT_NV12)
    FMT("nv21",                 IMGFMT_NV21)
    FMT("444p",                 IMGFMT_444P)
    FMT("422p",                 IMGFMT_422P)
    FMT("440p",                 IMGFMT_440P)
    FMT("420p",                 IMGFMT_420P)
    FMT("yv12",                 IMGFMT_420P) // old alias for UI
    FMT("411p",                 IMGFMT_411P)
    FMT("410p",                 IMGFMT_410P)
    FMT_ENDIAN("444p16",        IMGFMT_444P16)
    FMT_ENDIAN("444p14",        IMGFMT_444P14)
    FMT_ENDIAN("444p12",        IMGFMT_444P12)
    FMT_ENDIAN("444p10",        IMGFMT_444P10)
    FMT_ENDIAN("444p9",         IMGFMT_444P9)
    FMT_ENDIAN("422p16",        IMGFMT_422P16)
    FMT_ENDIAN("422p14",        IMGFMT_422P14)
    FMT_ENDIAN("422p12",        IMGFMT_422P12)
    FMT_ENDIAN("422p10",        IMGFMT_422P10)
    FMT_ENDIAN("422p9",         IMGFMT_422P9)
    FMT_ENDIAN("420p16",        IMGFMT_420P16)
    FMT_ENDIAN("420p14",        IMGFMT_420P14)
    FMT_ENDIAN("420p12",        IMGFMT_420P12)
    FMT_ENDIAN("420p10",        IMGFMT_420P10)
    FMT_ENDIAN("420p9",         IMGFMT_420P9)
    FMT("420ap",                IMGFMT_420AP)
    FMT("argb",                 IMGFMT_ARGB)
    FMT("bgra",                 IMGFMT_BGRA)
    FMT("bgr0",                 IMGFMT_BGR0)
    FMT("abgr",                 IMGFMT_ABGR)
    FMT("rgba",                 IMGFMT_RGBA)
    FMT("rgb32",                IMGFMT_RGB32)
    FMT("bgr32",                IMGFMT_BGR32)
    FMT("bgr24",                IMGFMT_BGR24)
    FMT("rgb24",                IMGFMT_RGB24)
    FMT_ENDIAN("rgb48",         IMGFMT_RGB48)
    FMT("rgb8",                 IMGFMT_RGB8)
    FMT("bgr8",                 IMGFMT_BGR8)
    FMT("rgb4_byte",            IMGFMT_RGB4_BYTE)
    FMT("bgr4_byte",            IMGFMT_BGR4_BYTE)
    FMT("rgb4",                 IMGFMT_RGB4)
    FMT("bgr4",                 IMGFMT_BGR4)
    FMT("mono",                 IMGFMT_MONO)
    FMT_ENDIAN("rgb12",         IMGFMT_RGB12)
    FMT_ENDIAN("rgb15",         IMGFMT_RGB15)
    FMT_ENDIAN("rgb16",         IMGFMT_RGB16)
    FMT_ENDIAN("bgr12",         IMGFMT_BGR12)
    FMT_ENDIAN("bgr15",         IMGFMT_BGR15)
    FMT_ENDIAN("bgr16",         IMGFMT_BGR16)
    FMT("pal8",                 IMGFMT_PAL8)
    FMT("gbrp",                 IMGFMT_GBRP)
    FMT("vdpau_mpeg1",          IMGFMT_VDPAU_MPEG1)
    FMT("vdpau_mpeg2",          IMGFMT_VDPAU_MPEG2)
    FMT("vdpau_h264",           IMGFMT_VDPAU_H264)
    FMT("vdpau_wmv3",           IMGFMT_VDPAU_WMV3)
    FMT("vdpau_vc1",            IMGFMT_VDPAU_VC1)
    FMT("vdpau_mpeg4",          IMGFMT_VDPAU_MPEG4)
    {0}
};

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

    // Packed RGB formats are the only formats that have less than 8 bits per
    // component, and still require endian dependent access.
    if (pd->comp[0].depth_minus1 + 1 <= 8 &&
        !(mpfmt >= IMGFMT_RGB12_LE || mpfmt <= IMGFMT_BGR16_BE))
    {
        desc.flags |= MP_IMGFLAG_LE | MP_IMGFLAG_BE;
    } else {
        desc.flags |= (pd->flags & PIX_FMT_BE) ? MP_IMGFLAG_BE : MP_IMGFLAG_LE;
    }

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
    case IMGFMT_UYVY:
        desc.flags |= MP_IMGFLAG_SWAPPED; // for vf_mpi_clear()
        /* fallthrough */
    case IMGFMT_YUYV:
        desc.chroma_ys = 1; // ???
        break;
    case IMGFMT_Y8:
    case IMGFMT_Y16_LE:
    case IMGFMT_Y16_BE:
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
        desc.flags ^= MP_IMGFLAG_SWAPPED; // ???
        break;
    case IMGFMT_BGR0:
        desc.flags &= ~MP_IMGFLAG_SWAPPED; // not covered by IS_RGB/IS_BGR
        break;
    }

    if (pd->flags & PIX_FMT_HWACCEL)
        desc.chroma_xs = desc.chroma_ys = 0;

    if (!(pd->flags & PIX_FMT_HWACCEL) && !(pd->flags & PIX_FMT_BITSTREAM)) {
        desc.flags |= MP_IMGFLAG_BYTE_ALIGNED;
        for (int p = 0; p < desc.num_planes; p++)
            desc.bytes[p] = desc.bpp[p] / 8;
    }

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

// Find a format that is MP_IMGFLAG_YUV_P with the following configuration.
int mp_imgfmt_find_yuv_planar(int xs, int ys, int planes, int component_bits)
{
    for (int n = IMGFMT_START + 1; n < IMGFMT_END; n++) {
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(n);
        if (desc.id && (desc.flags & MP_IMGFLAG_YUV_P)) {
            if (desc.num_planes == planes && desc.chroma_xs == xs &&
                desc.chroma_ys == ys && desc.plane_bits == component_bits &&
                (desc.flags & MP_IMGFLAG_NE))
                return desc.id;
        }
    }
    return 0;
}
