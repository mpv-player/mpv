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

#include <sys/types.h>
#include "config.h"
#include "core/bstr.h"

#define MP_MAX_PLANES 4

// set if (possibly) alpha is included (might be not definitive for packed RGB)
#define MP_IMGFLAG_ALPHA 0x80
// set if number of planes > 1
#define MP_IMGFLAG_PLANAR 0x100
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x200
// set if it's swapped (BGR or YVU) plane/byteorder
#define MP_IMGFLAG_SWAPPED 0x400
// set if the format is standard YUV format:
// - planar and yuv colorspace
// - chroma shift 0-2
// - 1-4 planes (1: gray, 2: gray/alpha, 3: yuv, 4: yuv/alpha)
// - 8-16 bit per pixel/plane, all planes have same depth
#define MP_IMGFLAG_YUV_P 0x1000
// set if format is in native endian, or <= 8 bit per pixel/plane
#define MP_IMGFLAG_NE 0x2000

#define MP_IMGFLAG_FMT_MASK 0x3FFF

struct mp_imgfmt_desc {
    int id;                 // IMGFMT_*
    int avformat;           // AV_PIX_FMT_* (or AV_PIX_FMT_NONE)
    const char *name;       // e.g. "420p16"
    int flags;              // MP_IMGFLAG_* bitfield
    int num_planes;
    int chroma_xs, chroma_ys; // chroma shift (i.e. log2 of chroma pixel size)
    int avg_bpp;
    int bpp[MP_MAX_PLANES];
    int plane_bits;         // number of bits in use for plane 0
    // chroma shifts per plane (provided for convenience with planar formats)
    int xs[MP_MAX_PLANES];
    int ys[MP_MAX_PLANES];
};

struct mp_imgfmt_desc mp_imgfmt_get_desc(unsigned int out_fmt);

/* RGB/BGR Formats */

#define IMGFMT_RGB_MASK 0xFFFFFF00
#define IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define IMGFMT_RGB1  (IMGFMT_RGB|1)
#define IMGFMT_RGB4  (IMGFMT_RGB|4)
#define IMGFMT_RGB4_CHAR  (IMGFMT_RGB|4|128) // RGB4 with 1 pixel per byte
#define IMGFMT_RGB8  (IMGFMT_RGB|8)
#define IMGFMT_RGB12 (IMGFMT_RGB|12)
#define IMGFMT_RGB15 (IMGFMT_RGB|15)
#define IMGFMT_RGB16 (IMGFMT_RGB|16)
#define IMGFMT_RGB24 (IMGFMT_RGB|24)
#define IMGFMT_RGB32 (IMGFMT_RGB|32)
#define IMGFMT_RGB48LE (IMGFMT_RGB|48)
#define IMGFMT_RGB48BE (IMGFMT_RGB|48|128)

#define IMGFMT_BGR_MASK 0xFFFFFF00
#define IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#define IMGFMT_BGR1 (IMGFMT_BGR|1)
#define IMGFMT_BGR4 (IMGFMT_BGR|4)
#define IMGFMT_BGR4_CHAR (IMGFMT_BGR|4|128) // BGR4 with 1 pixel per byte
#define IMGFMT_BGR8 (IMGFMT_BGR|8)
#define IMGFMT_BGR12 (IMGFMT_BGR|12)
#define IMGFMT_BGR15 (IMGFMT_BGR|15)
#define IMGFMT_BGR16 (IMGFMT_BGR|16)
#define IMGFMT_BGR24 (IMGFMT_BGR|24)
#define IMGFMT_BGR32 (IMGFMT_BGR|32)

#define IMGFMT_GBRP (('G'<<24)|('B'<<16)|('R'<<8)|24)

#if BYTE_ORDER == BIG_ENDIAN
#define IMGFMT_ABGR IMGFMT_RGB32
#define IMGFMT_BGRA (IMGFMT_RGB32|128)
#define IMGFMT_ARGB IMGFMT_BGR32
#define IMGFMT_RGBA (IMGFMT_BGR32|128)
#define IMGFMT_RGB48NE IMGFMT_RGB48BE
#define IMGFMT_RGB12BE IMGFMT_RGB12
#define IMGFMT_RGB12LE (IMGFMT_RGB12|128)
#define IMGFMT_RGB15BE IMGFMT_RGB15
#define IMGFMT_RGB15LE (IMGFMT_RGB15|128)
#define IMGFMT_RGB16BE IMGFMT_RGB16
#define IMGFMT_RGB16LE (IMGFMT_RGB16|128)
#define IMGFMT_BGR12BE IMGFMT_BGR12
#define IMGFMT_BGR12LE (IMGFMT_BGR12|128)
#define IMGFMT_BGR15BE IMGFMT_BGR15
#define IMGFMT_BGR15LE (IMGFMT_BGR15|128)
#define IMGFMT_BGR16BE IMGFMT_BGR16
#define IMGFMT_BGR16LE (IMGFMT_BGR16|128)
#else
#define IMGFMT_ABGR (IMGFMT_BGR32|128)
#define IMGFMT_BGRA IMGFMT_BGR32
#define IMGFMT_ARGB (IMGFMT_RGB32|128)
#define IMGFMT_RGBA IMGFMT_RGB32
#define IMGFMT_RGB48NE IMGFMT_RGB48LE
#define IMGFMT_RGB12BE (IMGFMT_RGB12|128)
#define IMGFMT_RGB12LE IMGFMT_RGB12
#define IMGFMT_RGB15BE (IMGFMT_RGB15|128)
#define IMGFMT_RGB15LE IMGFMT_RGB15
#define IMGFMT_RGB16BE (IMGFMT_RGB16|128)
#define IMGFMT_RGB16LE IMGFMT_RGB16
#define IMGFMT_BGR12BE (IMGFMT_BGR12|128)
#define IMGFMT_BGR12LE IMGFMT_BGR12
#define IMGFMT_BGR15BE (IMGFMT_BGR15|128)
#define IMGFMT_BGR15LE IMGFMT_BGR15
#define IMGFMT_BGR16BE (IMGFMT_BGR16|128)
#define IMGFMT_BGR16LE IMGFMT_BGR16
#endif

/* old names for compatibility */
#define IMGFMT_RG4B  IMGFMT_RGB4_CHAR
#define IMGFMT_BG4B  IMGFMT_BGR4_CHAR

// AV_PIX_FMT_BGR0
#define IMGFMT_BGR0  0x1DC70000

static inline bool IMGFMT_IS_RGB(unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    return !(desc.flags & MP_IMGFLAG_YUV) && !(desc.flags & MP_IMGFLAG_SWAPPED)
           && desc.num_planes == 1 && desc.id != IMGFMT_BGR0;
}
static inline bool IMGFMT_IS_BGR(unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    return !(desc.flags & MP_IMGFLAG_YUV) && (desc.flags & MP_IMGFLAG_SWAPPED)
           && desc.num_planes == 1 && desc.id != IMGFMT_BGR0;
}

#define IMGFMT_RGB_DEPTH(fmt) (mp_imgfmt_get_desc(fmt).plane_bits)
#define IMGFMT_BGR_DEPTH(fmt) (mp_imgfmt_get_desc(fmt).plane_bits)

// AV_PIX_FMT_GRAY16LE
#define IMGFMT_Y16LE 0x1DC70001
// AV_PIX_FMT_GRAY16BE
#define IMGFMT_Y16BE 0x1DC70002
// AV_PIX_FMT_PAL8
#define IMGFMT_PAL8  0x1DC70003

#if BYTE_ORDER == BIG_ENDIAN
#define IMGFMT_Y16 IMGFMT_Y16BE
#else
#define IMGFMT_Y16 IMGFMT_Y16LE
#endif

/* Planar YUV Formats */

#define IMGFMT_YVU9 0x39555659
#define IMGFMT_IF09 0x39304649
#define IMGFMT_YV12 0x32315659
#define IMGFMT_I420 0x30323449
#define IMGFMT_IYUV 0x56555949
#define IMGFMT_CLPL 0x4C504C43
#define IMGFMT_Y800 0x30303859
#define IMGFMT_Y8   0x20203859
#define IMGFMT_NV12 0x3231564E
#define IMGFMT_NV21 0x3132564E

/* unofficial Planar Formats, FIXME if official 4CC exists */
#define IMGFMT_444P 0x50343434
#define IMGFMT_422P 0x50323234
#define IMGFMT_411P 0x50313134
#define IMGFMT_440P 0x50303434
#define IMGFMT_HM12 0x32314D48

// 4:2:0 planar with alpha
#define IMGFMT_420A 0x41303234

#define IMGFMT_444P16_LE 0x51343434
#define IMGFMT_444P16_BE 0x34343451
#define IMGFMT_444P10_LE 0x52343434
#define IMGFMT_444P10_BE 0x34343452
#define IMGFMT_444P9_LE 0x53343434
#define IMGFMT_444P9_BE 0x34343453
#define IMGFMT_444P12_LE 0x54343434
#define IMGFMT_444P12_BE 0x34343454
#define IMGFMT_444P14_LE 0x55343434
#define IMGFMT_444P14_BE 0x34343455
#define IMGFMT_422P16_LE 0x51323234
#define IMGFMT_422P16_BE 0x34323251
#define IMGFMT_422P10_LE 0x52323234
#define IMGFMT_422P10_BE 0x34323252
#define IMGFMT_422P9_LE  0x53323234
#define IMGFMT_422P9_BE  0x34323253
#define IMGFMT_422P12_LE  0x54323234
#define IMGFMT_422P12_BE  0x34323254
#define IMGFMT_422P14_LE  0x55323234
#define IMGFMT_422P14_BE  0x34323255
#define IMGFMT_420P16_LE 0x51303234
#define IMGFMT_420P16_BE 0x34323051
#define IMGFMT_420P10_LE 0x52303234
#define IMGFMT_420P10_BE 0x34323052
#define IMGFMT_420P9_LE 0x53303234
#define IMGFMT_420P9_BE 0x34323053
#define IMGFMT_420P12_LE 0x54303234
#define IMGFMT_420P12_BE 0x34323054
#define IMGFMT_420P14_LE 0x55303234
#define IMGFMT_420P14_BE 0x34323055
#if BYTE_ORDER == BIG_ENDIAN
#define IMGFMT_444P16 IMGFMT_444P16_BE
#define IMGFMT_444P14 IMGFMT_444P14_BE
#define IMGFMT_444P12 IMGFMT_444P12_BE
#define IMGFMT_444P10 IMGFMT_444P10_BE
#define IMGFMT_444P9 IMGFMT_444P9_BE
#define IMGFMT_422P16 IMGFMT_422P16_BE
#define IMGFMT_422P14 IMGFMT_422P14_BE
#define IMGFMT_422P12 IMGFMT_422P12_BE
#define IMGFMT_422P10 IMGFMT_422P10_BE
#define IMGFMT_422P9  IMGFMT_422P9_BE
#define IMGFMT_420P16 IMGFMT_420P16_BE
#define IMGFMT_420P14 IMGFMT_420P14_BE
#define IMGFMT_420P12 IMGFMT_420P12_BE
#define IMGFMT_420P10 IMGFMT_420P10_BE
#define IMGFMT_420P9 IMGFMT_420P9_BE
#define IMGFMT_IS_YUVP16_NE(fmt) IMGFMT_IS_YUVP16_BE(fmt)
#else
#define IMGFMT_444P16 IMGFMT_444P16_LE
#define IMGFMT_444P14 IMGFMT_444P14_LE
#define IMGFMT_444P12 IMGFMT_444P12_LE
#define IMGFMT_444P10 IMGFMT_444P10_LE
#define IMGFMT_444P9 IMGFMT_444P9_LE
#define IMGFMT_422P16 IMGFMT_422P16_LE
#define IMGFMT_422P14 IMGFMT_422P14_LE
#define IMGFMT_422P12 IMGFMT_422P12_LE
#define IMGFMT_422P10 IMGFMT_422P10_LE
#define IMGFMT_422P9  IMGFMT_422P9_LE
#define IMGFMT_420P16 IMGFMT_420P16_LE
#define IMGFMT_420P14 IMGFMT_420P14_LE
#define IMGFMT_420P12 IMGFMT_420P12_LE
#define IMGFMT_420P10 IMGFMT_420P10_LE
#define IMGFMT_420P9 IMGFMT_420P9_LE
#define IMGFMT_IS_YUVP16_NE(fmt) IMGFMT_IS_YUVP16_LE(fmt)
#endif

// These functions are misnamed - they actually match 9 to 16 bits (inclusive)
static inline bool IMGFMT_IS_YUVP16_LE(int fmt) {
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    bool le_is_ne = BYTE_ORDER == LITTLE_ENDIAN;
    return (desc.flags & MP_IMGFLAG_YUV_P) && desc.plane_bits > 8 &&
           (le_is_ne == !!(desc.flags & MP_IMGFLAG_NE));
}
static inline bool IMGFMT_IS_YUVP16_BE(int fmt) {
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    bool be_is_ne = BYTE_ORDER == BIG_ENDIAN;
    return (desc.flags & MP_IMGFLAG_YUV_P) && desc.plane_bits > 8 &&
           (be_is_ne == !!(desc.flags & MP_IMGFLAG_NE));
}

#define IMGFMT_IS_YUVP16(fmt)    (IMGFMT_IS_YUVP16_LE(fmt) || IMGFMT_IS_YUVP16_BE(fmt))

/* Packed YUV Formats */

#define IMGFMT_IUYV 0x56595549 // Interlaced UYVY
#define IMGFMT_IY41 0x31435949 // Interlaced Y41P
#define IMGFMT_IYU1 0x31555949
#define IMGFMT_IYU2 0x32555949
#define IMGFMT_UYVY 0x59565955
#define IMGFMT_UYNV 0x564E5955 // Exactly same as UYVY
#define IMGFMT_cyuv 0x76757963 // upside-down UYVY
#define IMGFMT_Y422 0x32323459 // Exactly same as UYVY
#define IMGFMT_YUY2 0x32595559
#define IMGFMT_YUNV 0x564E5559 // Exactly same as YUY2
#define IMGFMT_YVYU 0x55595659
#define IMGFMT_Y41P 0x50313459
#define IMGFMT_Y211 0x31313259
#define IMGFMT_Y41T 0x54313459 // Y41P, Y lsb = transparency
#define IMGFMT_Y42T 0x54323459 // UYVY, Y lsb = transparency
#define IMGFMT_V422 0x32323456 // upside-down UYVY?
#define IMGFMT_V655 0x35353656
#define IMGFMT_CLJR 0x524A4C43
#define IMGFMT_YUVP 0x50565559 // 10-bit YUYV
#define IMGFMT_UYVP 0x50565955 // 10-bit UYVY

/* Compressed Formats */
#define IMGFMT_MJPEG (('M')|('J'<<8)|('P'<<16)|('G'<<24))

// VDPAU specific format.
#define IMGFMT_VDPAU               0x1DC80000
#define IMGFMT_VDPAU_MASK          0xFFFF0000
#define IMGFMT_IS_VDPAU(fmt)       (((fmt)&IMGFMT_VDPAU_MASK)==IMGFMT_VDPAU)
#define IMGFMT_VDPAU_MPEG1         (IMGFMT_VDPAU|0x01)
#define IMGFMT_VDPAU_MPEG2         (IMGFMT_VDPAU|0x02)
#define IMGFMT_VDPAU_H264          (IMGFMT_VDPAU|0x03)
#define IMGFMT_VDPAU_WMV3          (IMGFMT_VDPAU|0x04)
#define IMGFMT_VDPAU_VC1           (IMGFMT_VDPAU|0x05)
#define IMGFMT_VDPAU_MPEG4         (IMGFMT_VDPAU|0x06)

#define IMGFMT_IS_HWACCEL(fmt) IMGFMT_IS_VDPAU(fmt)

typedef struct {
    void* data;
    int size;
    int id;        // stream id. usually 0x1E0
    int timestamp; // pts, 90000 Hz counter based
} vo_mpegpes_t;

const char *vo_format_name(int format);

/**
 * Calculates the scale shifts for the chroma planes for planar YUV
 *
 * \param component_bits bits per component
 * \return bits-per-pixel for format if successful (i.e. format is 3 or 4-planes planar YUV), 0 otherwise
 */
int mp_get_chroma_shift(int format, int *x_shift, int *y_shift, int *component_bits);

struct mp_imgfmt_entry {
    const char *name;
    unsigned int fmt;
};

extern struct mp_imgfmt_entry mp_imgfmt_list[];

unsigned int mp_imgfmt_from_name(bstr name, bool allow_hwaccel);
const char *mp_imgfmt_to_name(unsigned int fmt);

#endif /* MPLAYER_IMG_FORMAT_H */
