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

#ifndef MPLAYER_IMG_FORMAT_H
#define MPLAYER_IMG_FORMAT_H

#include <inttypes.h>

#include "osdep/endian.h"
#include "misc/bstr.h"
#include "video/csputils.h"

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
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x200
// set if it's RGB colorspace
#define MP_IMGFLAG_RGB 0x400
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
// Carries a palette in plane[1] (see AV_PIX_FMT_PAL8 for format of the palette).
#define MP_IMGFLAG_PAL 0x8000
// planes don't contain real data
#define MP_IMGFLAG_HWACCEL 0x10000
// Like MP_IMGFLAG_YUV_P, but RGB. This can be e.g. AV_PIX_FMT_GBRP. The planes
// are always shuffled (G - B - R [- A]).
#define MP_IMGFLAG_RGB_P 0x40000
// Semi-planar YUV formats, like AV_PIX_FMT_NV12.
#define MP_IMGFLAG_YUV_NV 0x80000

struct mp_imgfmt_desc {
    int id;                 // IMGFMT_*
    int avformat;           // AV_PIX_FMT_* (or AV_PIX_FMT_NONE)
    int flags;              // MP_IMGFLAG_* bitfield
    int8_t num_planes;
    int8_t chroma_xs, chroma_ys; // chroma shift (i.e. log2 of chroma pixel size)
    int8_t align_x, align_y;     // pixel count to get byte alignment and to get
                                 // to a pixel pos where luma & chroma aligns
                                 // always power of 2
    int8_t bytes[MP_MAX_PLANES]; // bytes per pixel (MP_IMGFLAG_BYTE_ALIGNED)
    int8_t bpp[MP_MAX_PLANES];   // bits per pixel
    int8_t plane_bits;           // number of bits in use for plane 0
    int8_t component_bits;       // number of bits per component (0 if uneven)
    // chroma shifts per plane (provided for convenience with planar formats)
    int8_t xs[MP_MAX_PLANES];
    int8_t ys[MP_MAX_PLANES];
};

struct mp_imgfmt_desc mp_imgfmt_get_desc(int imgfmt);

// MP_CSP_AUTO for YUV, MP_CSP_RGB or MP_CSP_XYZ otherwise.
// (Because IMGFMT/AV_PIX_FMT conflate format and csp for RGB and XYZ.)
enum mp_csp mp_imgfmt_get_forced_csp(int imgfmt);

enum mp_component_type {
    MP_COMPONENT_TYPE_UNKNOWN = 0,
    MP_COMPONENT_TYPE_UINT,
    MP_COMPONENT_TYPE_FLOAT,
};

enum mp_component_type mp_imgfmt_get_component_type(int imgfmt);

#define MP_NUM_COMPONENTS 4

struct mp_regular_imgfmt_plane {
    uint8_t num_components;
    // 1 is red/luminance/gray, 2 is green/Cb, 3 is blue/Cr, 4 is alpha.
    // 0 is used for padding (undefined contents).
    // It is guaranteed that non-0 values occur only once in the whole format.
    uint8_t components[MP_NUM_COMPONENTS];
};

// This describes pixel formats that are byte aligned, have byte aligned
// components, native endian, etc.
struct mp_regular_imgfmt {
    // Type of each component.
    enum mp_component_type component_type;

    // See mp_imgfmt_get_forced_csp(). Normally code should use
    // mp_image_params.colors. This field is only needed to map the format
    // unambiguously to FFmpeg formats.
    enum mp_csp forced_csp;

    // Size of each component in bytes.
    uint8_t component_size;

    // If >0, LSB padding, if <0, MSB padding. The padding bits are always 0.
    // This applies: bit_depth = component_size * 8 - abs(component_pad)
    //               bit_size  = component_size * 8 + MPMIN(0, component_pad)
    //  E.g. P010: component_pad=6 (LSB always implied 0, all data in MSB)
    //          => has a "depth" of 10 bit, but usually treated as 16 bit value
    //       yuv420p10: component_pad=-6 (like a 10 bit value 0-extended to 16)
    //          => has depth of 10 bit, needs <<6 to get a 16 bit value
    int8_t component_pad;

    uint8_t num_planes;
    struct mp_regular_imgfmt_plane planes[MP_MAX_PLANES];

    // Chroma shifts for chroma planes. 0/0 is 4:4:4 YUV or RGB. If not 0/0,
    // then this is always a yuv format, with components 2/3 on separate planes
    // (reduced by the shift), and planes for components 1/4 are full sized.
    uint8_t chroma_xs, chroma_ys;
};

bool mp_get_regular_imgfmt(struct mp_regular_imgfmt *dst, int imgfmt);
int mp_find_regular_imgfmt(struct mp_regular_imgfmt *src);

// If imgfmt is valid, and there exists a format that is exactly the same, but
// has inverse endianness, return this other format. Otherwise return 0.
int mp_find_other_endian(int imgfmt);

enum mp_imgfmt {
    IMGFMT_NONE = 0,

    // Offset to make confusing with ffmpeg formats harder
    IMGFMT_START = 1000,

    // Planar YUV formats
    IMGFMT_444P,                // 1x1
    IMGFMT_420P,                // 2x2

    // Gray
    IMGFMT_Y8,
    IMGFMT_Y16,

    // Packed YUV formats (components are byte-accessed)
    IMGFMT_UYVY,                // U  Y0 V  Y1

    // Y plane + packed plane for chroma
    IMGFMT_NV12,

    // Like IMGFMT_NV12, but with 10 bits per component (and 6 bits of padding)
    IMGFMT_P010,

    // RGB/BGR Formats

    // Byte accessed (low address to high address)
    IMGFMT_ARGB,
    IMGFMT_BGRA,
    IMGFMT_ABGR,
    IMGFMT_RGBA,
    IMGFMT_BGR24,               // 3 bytes per pixel
    IMGFMT_RGB24,

    // Like e.g. IMGFMT_ARGB, but has a padding byte instead of alpha
    IMGFMT_0RGB,
    IMGFMT_BGR0,
    IMGFMT_0BGR,
    IMGFMT_RGB0,

    // Like IMGFMT_RGBA, but 2 bytes per component.
    IMGFMT_RGBA64,

    // Accessed with bit-shifts after endian-swapping the uint16_t pixel
    IMGFMT_RGB565,              // 5r 6g 5b (MSB to LSB)

    // AV_PIX_FMT_PAL8
    IMGFMT_PAL8,

    // Hardware accelerated formats. Plane data points to special data
    // structures, instead of pixel data.
    IMGFMT_VDPAU,           // VdpVideoSurface
    // plane 0: ID3D11Texture2D
    // plane 1: slice index casted to pointer
    IMGFMT_D3D11,
    IMGFMT_DXVA2,           // IDirect3DSurface9 (NV12/P010/P016)
    IMGFMT_MMAL,            // MMAL_BUFFER_HEADER_T
    IMGFMT_MEDIACODEC,      // AVMediaCodecBuffer
    IMGFMT_DRMPRIME,        // AVDRMFrameDescriptor
    IMGFMT_CUDA,            // CUDA Buffer

    // Not an actual format; base for mpv-specific descriptor table.
    // Some may still map to AV_PIX_FMT_*.
    IMGFMT_CUST_BASE,

    // Planar gray/alpha.
    IMGFMT_YAP8,
    IMGFMT_YAP16,

    // Planar YUV/alpha formats. Sometimes useful for internal processing. There
    // should be one for each subsampling factor, with and without alpha, gray.
    IMGFMT_YAPF, // Note: non-alpha version exists in ffmpeg
    IMGFMT_444PF,
    IMGFMT_444APF,
    IMGFMT_420PF,
    IMGFMT_420APF,
    IMGFMT_422PF,
    IMGFMT_422APF,
    IMGFMT_440PF,
    IMGFMT_440APF,
    IMGFMT_410PF,
    IMGFMT_410APF,
    IMGFMT_411PF,
    IMGFMT_411APF,

    // Accessed with bit-shifts, uint32_t units.
    IMGFMT_RGB30,               // 2pad 10r 10g 10b (MSB to LSB)

    // Fringe formats for fringe RGB format repacking.
    IMGFMT_Y1,      // gray with 1 bit per pixel
    IMGFMT_GBRP1,   // planar RGB with N bits per color component
    IMGFMT_GBRP2,
    IMGFMT_GBRP3,
    IMGFMT_GBRP4,
    IMGFMT_GBRP5,
    IMGFMT_GBRP6,

    // Hardware accelerated formats (again).
    IMGFMT_VDPAU_OUTPUT,    // VdpOutputSurface
    IMGFMT_VAAPI,
    IMGFMT_VIDEOTOOLBOX,    // CVPixelBufferRef

    // Generic pass-through of AV_PIX_FMT_*. Used for formats which don't have
    // a corresponding IMGFMT_ value.
    IMGFMT_AVPIXFMT_START,
    IMGFMT_AVPIXFMT_END = IMGFMT_AVPIXFMT_START + 500,

    IMGFMT_END,
};

static inline bool IMGFMT_IS_RGB(int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    return (desc.flags & MP_IMGFLAG_RGB) && desc.num_planes == 1;
}

#define IMGFMT_RGB_DEPTH(fmt) (mp_imgfmt_get_desc(fmt).plane_bits)
#define IMGFMT_IS_HWACCEL(fmt) (!!(mp_imgfmt_get_desc(fmt).flags & MP_IMGFLAG_HWACCEL))

int mp_imgfmt_from_name(bstr name);
char *mp_imgfmt_to_name_buf(char *buf, size_t buf_size, int fmt);
#define mp_imgfmt_to_name(fmt) mp_imgfmt_to_name_buf((char[16]){0}, 16, (fmt))

char **mp_imgfmt_name_list(void);

#define vo_format_name mp_imgfmt_to_name

int mp_imgfmt_find(int xs, int ys, int planes, int component_bits, int flags);

int mp_imgfmt_select_best(int dst1, int dst2, int src);
int mp_imgfmt_select_best_list(int *dst, int num_dst, int src);

#endif /* MPLAYER_IMG_FORMAT_H */
