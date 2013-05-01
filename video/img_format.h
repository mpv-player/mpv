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

#ifndef MPLAYER_IMG_FORMAT_H
#define MPLAYER_IMG_FORMAT_H

#include <inttypes.h>
#include <sys/types.h>
#include "core/bstr.h"

#if BYTE_ORDER == BIG_ENDIAN
#define MP_SELECT_LE_BE(LE, BE) BE
#else
#define MP_SELECT_LE_BE(LE, BE) LE
#endif

#define MP_MAX_PLANES 4

// All pixels start in byte boundaries
#define MP_IMGFLAG_BYTE_ALIGNED 0x1
// set if (possibly) alpha is included (might be not definitive for packed RGB)
#define MP_IMGFLAG_ALPHA 0x80
// Uses one component per plane (set even if it's just one plane)
#define MP_IMGFLAG_PLANAR 0x100
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x200
// set if it's RGB colorspace
#define MP_IMGFLAG_RGB 0x400
// set if it's XYZ colorspace
#define MP_IMGFLAG_XYZ 0x800
// set if the format is in a standard YUV format:
// - planar and yuv colorspace
// - chroma shift 0-2
// - 1-4 planes (1: gray, 2: gray/alpha, 3: yuv, 4: yuv/alpha)
// - 8-16 bit per pixel/plane, all planes have same depth,
//   each plane has exactly one component
#define MP_IMGFLAG_YUV_P 0x1000
// set if in little endian, or endian independent
#define MP_IMGFLAG_LE 0x2000
// set if in big endian, or endian independent
#define MP_IMGFLAG_BE 0x4000
// set if in native (host) endian, or endian independent
#define MP_IMGFLAG_NE MP_SELECT_LE_BE(MP_IMGFLAG_LE, MP_IMGFLAG_BE)

#define MP_IMGFLAG_FMT_MASK 0x3FFF

struct mp_imgfmt_desc {
    int id;                 // IMGFMT_*
    int avformat;           // AV_PIX_FMT_* (or AV_PIX_FMT_NONE)
    const char *name;       // e.g. "420p16"
    int flags;              // MP_IMGFLAG_* bitfield
    int8_t num_planes;
    int8_t chroma_xs, chroma_ys; // chroma shift (i.e. log2 of chroma pixel size)
    int8_t align_x, align_y;     // pixel size to get byte alignment and to get
                                 // to a pixel pos where luma & chroma aligns
    int8_t bytes[MP_MAX_PLANES]; // bytes per pixel (MP_IMGFLAG_BYTE_ALIGNED)
    int8_t bpp[MP_MAX_PLANES];   // bits per pixel
    int8_t plane_bits;           // number of bits in use for plane 0
    // chroma shifts per plane (provided for convenience with planar formats)
    int8_t xs[MP_MAX_PLANES];
    int8_t ys[MP_MAX_PLANES];
};

struct mp_imgfmt_desc mp_imgfmt_get_desc(unsigned int out_fmt);

enum mp_imgfmt {
    IMGFMT_NONE = 0,

    // Offset to make confusing with ffmpeg formats harder
    IMGFMT_START = 1000,

    // Planar YUV formats

    IMGFMT_444P,                // 1x1
    IMGFMT_422P,                // 2x1
    IMGFMT_440P,                // 1x2
    IMGFMT_420P,                // 2x2
    IMGFMT_411P,                // 4x1
    IMGFMT_410P,                // 4x4

    // YUV formats with 2 bytes per plane-pixel. Formats with 9-15 bits pad the
    // most significant bits with 0 (use shifts to expand them to 16 bits).

    IMGFMT_444P16_LE,
    IMGFMT_444P16_BE,
    IMGFMT_444P14_LE,
    IMGFMT_444P14_BE,
    IMGFMT_444P12_LE,
    IMGFMT_444P12_BE,
    IMGFMT_444P10_LE,
    IMGFMT_444P10_BE,
    IMGFMT_444P9_LE,
    IMGFMT_444P9_BE,

    IMGFMT_422P16_LE,
    IMGFMT_422P16_BE,
    IMGFMT_422P14_LE,
    IMGFMT_422P14_BE,
    IMGFMT_422P12_LE,
    IMGFMT_422P12_BE,
    IMGFMT_422P10_LE,
    IMGFMT_422P10_BE,
    IMGFMT_422P9_LE,
    IMGFMT_422P9_BE,

    IMGFMT_420P16_LE,
    IMGFMT_420P16_BE,
    IMGFMT_420P14_LE,
    IMGFMT_420P14_BE,
    IMGFMT_420P12_LE,
    IMGFMT_420P12_BE,
    IMGFMT_420P10_LE,
    IMGFMT_420P10_BE,
    IMGFMT_420P9_LE,
    IMGFMT_420P9_BE,

    // Planar YUV with alpha (4th plane)
    IMGFMT_444AP,
    IMGFMT_422AP,
    IMGFMT_420AP,

    IMGFMT_444AP16_LE,
    IMGFMT_444AP16_BE,
    IMGFMT_444AP10_LE,
    IMGFMT_444AP10_BE,
    IMGFMT_444AP9_LE,
    IMGFMT_444AP9_BE,

    IMGFMT_422AP16_LE,
    IMGFMT_422AP16_BE,
    IMGFMT_422AP10_LE,
    IMGFMT_422AP10_BE,
    IMGFMT_422AP9_LE,
    IMGFMT_422AP9_BE,

    IMGFMT_420AP16_LE,
    IMGFMT_420AP16_BE,
    IMGFMT_420AP10_LE,
    IMGFMT_420AP10_BE,
    IMGFMT_420AP9_LE,
    IMGFMT_420AP9_BE,

    // Gray
    IMGFMT_Y8,
    IMGFMT_Y16_LE,
    IMGFMT_Y16_BE,

    // Gray with alpha (packed)
    IMGFMT_YA8,

    // Packed YUV formats (components are byte-accessed)
    IMGFMT_YUYV,                // Y0 U  Y1 V
    IMGFMT_UYVY,                // U  Y0 V  Y1

    // Y plane + packed plane for chroma
    IMGFMT_NV12,
    IMGFMT_NV21,

    // RGB/BGR Formats

    // Byte accessed (low address to high address)
    IMGFMT_ARGB,
    IMGFMT_0RGB,
    IMGFMT_BGRA,
    IMGFMT_BGR0,
    IMGFMT_ABGR,
    IMGFMT_0BGR,
    IMGFMT_RGBA,
    IMGFMT_RGB0,
    IMGFMT_BGR24,
    IMGFMT_RGB24,
    IMGFMT_RGB48_LE,
    IMGFMT_RGB48_BE,
    IMGFMT_RGBA64_LE,
    IMGFMT_RGBA64_BE,
    IMGFMT_BGRA64_LE,
    IMGFMT_BGRA64_BE,

    // Accessed with bit-shifts (components ordered from LSB to MSB)
    IMGFMT_RGB8,                // r3 g3 b2
    IMGFMT_BGR8,
    IMGFMT_RGB4_BYTE,           // r1 g2 b1 with 1 pixel per byte
    IMGFMT_BGR4_BYTE,
    IMGFMT_RGB4,                // r1 g2 b1, bit-packed
    IMGFMT_BGR4,
    IMGFMT_MONO,                // 1 bit per pixel, bit-packed
    IMGFMT_MONO_W,              // like IMGFMT_MONO, but inverted (white pixels)

    // Accessed with bit-shifts after endian-swapping the uint16_t pixel
    IMGFMT_RGB12_LE,            // 4r 4g 4b 4a  (LSB to MSB)
    IMGFMT_RGB12_BE,
    IMGFMT_RGB15_LE,            // 5r 5g 5b 1a
    IMGFMT_RGB15_BE,
    IMGFMT_RGB16_LE,            // 5r 6g 5b
    IMGFMT_RGB16_BE,
    IMGFMT_BGR12_LE,            // 4b 4r 4g 4a
    IMGFMT_BGR12_BE,
    IMGFMT_BGR15_LE,            // 5b 5g 5r 1a
    IMGFMT_BGR15_BE,
    IMGFMT_BGR16_LE,            // 5b 6g 5r
    IMGFMT_BGR16_BE,

    IMGFMT_PAL8,                // Palette entries are IMGFMT_BGR32

    // Planar RGB (planes are shuffled: plane 0 is G, etc.)
    IMGFMT_GBRP,
    IMGFMT_GBRP9_LE,
    IMGFMT_GBRP9_BE,
    IMGFMT_GBRP10_LE,
    IMGFMT_GBRP10_BE,
    IMGFMT_GBRP12_LE,
    IMGFMT_GBRP12_BE,
    IMGFMT_GBRP14_LE,
    IMGFMT_GBRP14_BE,
    IMGFMT_GBRP16_LE,
    IMGFMT_GBRP16_BE,

    // XYZ colorspace, similar organization to RGB48. Even though it says "12",
    // the components are stored as 16 bit, with lower 4 bits set to 0.
    IMGFMT_XYZ12_LE,
    IMGFMT_XYZ12_BE,

    // Hardware acclerated formats. Plane data points to special data
    // structures, instead of pixel data.

    IMGFMT_VDPAU_MPEG1,
    IMGFMT_VDPAU_MPEG2,
    IMGFMT_VDPAU_H264,
    IMGFMT_VDPAU_WMV3,
    IMGFMT_VDPAU_VC1,
    IMGFMT_VDPAU_MPEG4,

    IMGFMT_VDPAU_FIRST = IMGFMT_VDPAU_MPEG1,
    IMGFMT_VDPAU_LAST  = IMGFMT_VDPAU_MPEG4,

    IMGFMT_END,

    // Redundant format aliases for native endian access
    // For all formats that have _LE/_BE, define a native-endian entry without
    // the suffix.

    // The IMGFMT_RGB32 and IMGFMT_BGR32 formats provide bit-shift access to
    // normally byte-accessed formats:
    // IMGFMT_RGB32 = r | (g << 8) | (b << 16) | (a << 24)
    // IMGFMT_BGR32 = b | (g << 8) | (r << 16) | (a << 24)
    IMGFMT_RGB32   = MP_SELECT_LE_BE(IMGFMT_RGBA, IMGFMT_ABGR),
    IMGFMT_BGR32   = MP_SELECT_LE_BE(IMGFMT_BGRA, IMGFMT_ARGB),

    IMGFMT_RGB12   = MP_SELECT_LE_BE(IMGFMT_RGB12_LE, IMGFMT_RGB12_BE),
    IMGFMT_RGB15   = MP_SELECT_LE_BE(IMGFMT_RGB15_LE, IMGFMT_RGB15_BE),
    IMGFMT_RGB16   = MP_SELECT_LE_BE(IMGFMT_RGB16_LE, IMGFMT_RGB16_BE),
    IMGFMT_BGR12   = MP_SELECT_LE_BE(IMGFMT_BGR12_LE, IMGFMT_BGR12_BE),
    IMGFMT_BGR15   = MP_SELECT_LE_BE(IMGFMT_BGR15_LE, IMGFMT_BGR15_BE),
    IMGFMT_BGR16   = MP_SELECT_LE_BE(IMGFMT_BGR16_LE, IMGFMT_BGR16_BE),
    IMGFMT_RGB48   = MP_SELECT_LE_BE(IMGFMT_RGB48_LE, IMGFMT_RGB48_BE),
    IMGFMT_RGBA64  = MP_SELECT_LE_BE(IMGFMT_RGBA64_LE, IMGFMT_RGBA64_BE),
    IMGFMT_BGRA64  = MP_SELECT_LE_BE(IMGFMT_BGRA64_LE, IMGFMT_BGRA64_BE),

    IMGFMT_444P16  = MP_SELECT_LE_BE(IMGFMT_444P16_LE, IMGFMT_444P16_BE),
    IMGFMT_444P14  = MP_SELECT_LE_BE(IMGFMT_444P14_LE, IMGFMT_444P14_BE),
    IMGFMT_444P12  = MP_SELECT_LE_BE(IMGFMT_444P12_LE, IMGFMT_444P12_BE),
    IMGFMT_444P10  = MP_SELECT_LE_BE(IMGFMT_444P10_LE, IMGFMT_444P10_BE),
    IMGFMT_444P9   = MP_SELECT_LE_BE(IMGFMT_444P9_LE, IMGFMT_444P9_BE),

    IMGFMT_422P16  = MP_SELECT_LE_BE(IMGFMT_422P16_LE, IMGFMT_422P16_BE),
    IMGFMT_422P14  = MP_SELECT_LE_BE(IMGFMT_422P14_LE, IMGFMT_422P14_BE),
    IMGFMT_422P12  = MP_SELECT_LE_BE(IMGFMT_422P12_LE, IMGFMT_422P12_BE),
    IMGFMT_422P10  = MP_SELECT_LE_BE(IMGFMT_422P10_LE, IMGFMT_422P10_BE),
    IMGFMT_422P9   = MP_SELECT_LE_BE(IMGFMT_422P9_LE, IMGFMT_422P9_BE),

    IMGFMT_420P16  = MP_SELECT_LE_BE(IMGFMT_420P16_LE, IMGFMT_420P16_BE),
    IMGFMT_420P14  = MP_SELECT_LE_BE(IMGFMT_420P14_LE, IMGFMT_420P14_BE),
    IMGFMT_420P12  = MP_SELECT_LE_BE(IMGFMT_420P12_LE, IMGFMT_420P12_BE),
    IMGFMT_420P10  = MP_SELECT_LE_BE(IMGFMT_420P10_LE, IMGFMT_420P10_BE),
    IMGFMT_420P9   = MP_SELECT_LE_BE(IMGFMT_420P9_LE, IMGFMT_420P9_BE),

    IMGFMT_444AP16 = MP_SELECT_LE_BE(IMGFMT_444AP16_LE, IMGFMT_444AP16_BE),
    IMGFMT_444AP10 = MP_SELECT_LE_BE(IMGFMT_444AP10_LE, IMGFMT_444AP10_BE),
    IMGFMT_444AP9  = MP_SELECT_LE_BE(IMGFMT_444AP9_LE, IMGFMT_444AP9_BE),

    IMGFMT_422AP16 = MP_SELECT_LE_BE(IMGFMT_422AP16_LE, IMGFMT_422AP16_BE),
    IMGFMT_422AP10 = MP_SELECT_LE_BE(IMGFMT_422AP10_LE, IMGFMT_422AP10_BE),
    IMGFMT_422AP9  = MP_SELECT_LE_BE(IMGFMT_422AP9_LE, IMGFMT_422AP9_BE),

    IMGFMT_420AP16 = MP_SELECT_LE_BE(IMGFMT_420AP16_LE, IMGFMT_420AP16_BE),
    IMGFMT_420AP10 = MP_SELECT_LE_BE(IMGFMT_420AP10_LE, IMGFMT_420AP10_BE),
    IMGFMT_420AP9  = MP_SELECT_LE_BE(IMGFMT_420AP9_LE, IMGFMT_420AP9_BE),

    IMGFMT_Y16     = MP_SELECT_LE_BE(IMGFMT_Y16_LE, IMGFMT_Y16_BE),

    IMGFMT_GBRP9   = MP_SELECT_LE_BE(IMGFMT_GBRP9_LE, IMGFMT_GBRP9_BE),
    IMGFMT_GBRP10  = MP_SELECT_LE_BE(IMGFMT_GBRP10_LE, IMGFMT_GBRP10_BE),
    IMGFMT_GBRP12  = MP_SELECT_LE_BE(IMGFMT_GBRP12_LE, IMGFMT_GBRP12_BE),
    IMGFMT_GBRP14  = MP_SELECT_LE_BE(IMGFMT_GBRP14_LE, IMGFMT_GBRP14_BE),
    IMGFMT_GBRP16  = MP_SELECT_LE_BE(IMGFMT_GBRP16_LE, IMGFMT_GBRP16_BE),

    IMGFMT_XYZ12   = MP_SELECT_LE_BE(IMGFMT_XYZ12_LE, IMGFMT_XYZ12_BE),
};

static inline bool IMGFMT_IS_RGB(unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    return (desc.flags & MP_IMGFLAG_RGB) && desc.num_planes == 1;
}

#define IMGFMT_RGB_DEPTH(fmt) (mp_imgfmt_get_desc(fmt).plane_bits)

#define IMGFMT_IS_VDPAU(fmt) \
    (((fmt) >= IMGFMT_VDPAU_FIRST) && ((fmt) <= IMGFMT_VDPAU_LAST))

#define IMGFMT_IS_HWACCEL(fmt) IMGFMT_IS_VDPAU(fmt)


struct mp_imgfmt_entry {
    const char *name;
    unsigned int fmt;
};

extern struct mp_imgfmt_entry mp_imgfmt_list[];

unsigned int mp_imgfmt_from_name(bstr name, bool allow_hwaccel);
const char *mp_imgfmt_to_name(unsigned int fmt);

#define vo_format_name mp_imgfmt_to_name

int mp_imgfmt_find_yuv_planar(int xs, int ys, int planes, int component_bits);

#endif /* MPLAYER_IMG_FORMAT_H */
