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

#include "misc/bstr.h"
#include "video/csputils.h"

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
#define MP_IMGFLAG_NE MP_IMGFLAG_LE
// Carries a palette in plane[1] (see IMGFMT_PAL8 for format of the palette).
// Note that some non-paletted formats have this flag set, because FFmpeg
// mysteriously expects some formats to carry a palette plane for no apparent
// reason. FFmpeg developer braindeath?
// The only real paletted format we support is IMGFMT_PAL8, so check for that
// format directly if you want an actual paletted format.
#define MP_IMGFLAG_PAL 0x8000
// planes don't contain real data
#define MP_IMGFLAG_HWACCEL 0x10000
// Like MP_IMGFLAG_YUV_P, but RGB. This can be e.g. AV_PIX_FMT_GBRP. The planes
// are always shuffled (G - B - R [- A]).
#define MP_IMGFLAG_RGB_P 0x40000
// Semi-planar YUV formats, like AV_PIX_FMT_NV12.
// The flag MP_IMGFLAG_YUV_NV_SWAP is set for AV_PIX_FMT_NV21.
#define MP_IMGFLAG_YUV_NV 0x80000
#define MP_IMGFLAG_YUV_NV_SWAP 0x100000

// Exactly one of these bits is set in mp_imgfmt_desc.flags
#define MP_IMGFLAG_COLOR_CLASS_MASK \
    (MP_IMGFLAG_YUV | MP_IMGFLAG_RGB | MP_IMGFLAG_XYZ)

struct mp_imgfmt_desc {
    int id;                 // IMGFMT_*
    int avformat;           // AV_PIX_FMT_* (or AV_PIX_FMT_NONE)
    int flags;              // MP_IMGFLAG_* bitfield
    int8_t num_planes;
    int8_t chroma_xs, chroma_ys; // chroma shift (i.e. log2 of chroma pixel size)
    int8_t align_x, align_y;     // pixel size to get byte alignment and to get
                                 // to a pixel pos where luma & chroma aligns
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
    // 1 is luminance/red/gray, 2 is green/Cb, 3 is blue/Cr, 4 is alpha.
    // 0 is used for padding (undefined contents).
    uint8_t components[MP_NUM_COMPONENTS];
};

// This describes pixel formats that are byte aligned, have byte aligned
// components, native endian, etc.
struct mp_regular_imgfmt {
    // Type of each component.
    enum mp_component_type component_type;

    // Size of each component in bytes.
    uint8_t component_size;

    // If >0, LSB padding, if <0, MSB padding. The padding bits are always 0.
    // This applies: bit_depth = component_size * 8 - abs(component_pad)
    int8_t component_pad;

    uint8_t num_planes;
    struct mp_regular_imgfmt_plane planes[MP_MAX_PLANES];

    // Chroma pixel size (1x1 is 4:4:4)
    uint8_t chroma_w, chroma_h;
};

bool mp_get_regular_imgfmt(struct mp_regular_imgfmt *dst, int imgfmt);

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

    IMGFMT_RGB0_START = IMGFMT_0RGB,
    IMGFMT_RGB0_END = IMGFMT_RGB0,

    // Like IMGFMT_RGBA, but 2 bytes per component.
    IMGFMT_RGBA64,

    // Accessed with bit-shifts after endian-swapping the uint16_t pixel
    IMGFMT_RGB565,              // 5r 6g 5b (MSB to LSB)

    // Hardware accelerated formats. Plane data points to special data
    // structures, instead of pixel data.
    IMGFMT_VDPAU,           // VdpVideoSurface
    IMGFMT_VDPAU_OUTPUT,    // VdpOutputSurface
    IMGFMT_VAAPI,
    // plane 0: ID3D11Texture2D
    // plane 1: slice index casted to pointer
    IMGFMT_D3D11,
    IMGFMT_DXVA2,           // IDirect3DSurface9 (NV12/P010/P016)
    IMGFMT_MMAL,            // MMAL_BUFFER_HEADER_T
    IMGFMT_VIDEOTOOLBOX,    // CVPixelBufferRef
    IMGFMT_MEDIACODEC,      // AVMediaCodecBuffer
    IMGFMT_DRMPRIME,        // AVDRMFrameDescriptor
    IMGFMT_CUDA,            // CUDA Buffer

    // Generic pass-through of AV_PIX_FMT_*. Used for formats which don't have
    // a corresponding IMGFMT_ value.
    IMGFMT_AVPIXFMT_START,
    IMGFMT_AVPIXFMT_END = IMGFMT_AVPIXFMT_START + 500,

    IMGFMT_END,

    // Redundant format aliases for native endian access

    // The IMGFMT_RGB32 and IMGFMT_BGR32 formats provide bit-shift access to
    // normally byte-accessed formats:
    // IMGFMT_RGB32 = r | (g << 8) | (b << 16) | (a << 24)
    // IMGFMT_BGR32 = b | (g << 8) | (r << 16) | (a << 24)
    IMGFMT_RGB32   = IMGFMT_RGBA,
    IMGFMT_BGR32   = IMGFMT_BGRA,
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
