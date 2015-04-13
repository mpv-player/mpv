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

#include <assert.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"

struct mp_imgfmt_entry {
    const char *name;
    int fmt;
};

static const struct mp_imgfmt_entry mp_imgfmt_list[] = {
    // not in ffmpeg
    {"vdpau_output",    IMGFMT_VDPAU_OUTPUT},
    // FFmpeg names have an annoying "_vld" suffix
    {"vda",             IMGFMT_VDA},
    {"vaapi",           IMGFMT_VAAPI},
    // names below this are not preferred over the FFmpeg names
    // the "none" entry makes mp_imgfmt_to_name prefer FFmpeg names
    {"none",            0},
    // endian-specific aliases (not in FFmpeg)
    {"rgb32",           IMGFMT_RGB32},
    {"bgr32",           IMGFMT_BGR32},
    // old names we keep around
    {"y8",              IMGFMT_Y8},
    {"420p",            IMGFMT_420P},
    {"yv12",            IMGFMT_420P},
    {"420p16",          IMGFMT_420P16},
    {"420p10",          IMGFMT_420P10},
    {"444p",            IMGFMT_444P},
    {"444p9",           IMGFMT_444P9},
    {"444p10",          IMGFMT_444P10},
    {"422p",            IMGFMT_422P},
    {"422p9",           IMGFMT_422P9},
    {"422p10",          IMGFMT_422P10},
    {0}
};

char **mp_imgfmt_name_list(void)
{
    int count = IMGFMT_END - IMGFMT_START;
    char **list = talloc_zero_array(NULL, char *, count + 1);
    int num = 0;
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        const char *name = mp_imgfmt_to_name(n);
        if (strcmp(name, "none") != 0 && strcmp(name, "unknown") != 0)
            list[num++] = talloc_strdup(list, name);
    }
    return list;
}

int mp_imgfmt_from_name(bstr name, bool allow_hwaccel)
{
    int img_fmt = 0;
    for (const struct mp_imgfmt_entry *p = mp_imgfmt_list; p->name; ++p) {
        if (bstr_equals0(name, p->name)) {
            img_fmt = p->fmt;
            break;
        }
    }
    if (!img_fmt) {
        char *t = bstrdup0(NULL, name);
        img_fmt = pixfmt2imgfmt(av_get_pix_fmt(t));
        talloc_free(t);
    }
    if (!allow_hwaccel && IMGFMT_IS_HWACCEL(img_fmt))
        return 0;
    return img_fmt;
}

char *mp_imgfmt_to_name_buf(char *buf, size_t buf_size, int fmt)
{
    const char *name = NULL;
    const struct mp_imgfmt_entry *p = mp_imgfmt_list;
    for (; p->fmt; p++) {
        if (p->name && p->fmt == fmt) {
            name = p->name;
            break;
        }
    }
    if (!name) {
        const AVPixFmtDescriptor *pixdesc = av_pix_fmt_desc_get(imgfmt2pixfmt(fmt));
        if (pixdesc)
            name = pixdesc->name;
    }
    if (!name)
        name = "unknown";
    snprintf(buf, buf_size, "%s", name);
    int len = strlen(buf);
    if (len > 2 && buf[len - 2] == MP_SELECT_LE_BE('l', 'b') && buf[len - 1] == 'e')
        buf[len - 2] = '\0';
    return buf;
}

static struct mp_imgfmt_desc mp_only_imgfmt_desc(int mpfmt)
{
    switch (mpfmt) {
    case IMGFMT_VDPAU_OUTPUT:
        return (struct mp_imgfmt_desc) {
            .id = mpfmt,
            .avformat = AV_PIX_FMT_NONE,
            .flags = MP_IMGFLAG_BE | MP_IMGFLAG_LE | MP_IMGFLAG_RGB |
                     MP_IMGFLAG_HWACCEL,
        };
    }
    return (struct mp_imgfmt_desc) {0};
}

struct mp_imgfmt_desc mp_imgfmt_get_desc(int mpfmt)
{
    enum AVPixelFormat fmt = imgfmt2pixfmt(mpfmt);
    const AVPixFmtDescriptor *pd = av_pix_fmt_desc_get(fmt);
    if (!pd || pd->nb_components > 4 || fmt == AV_PIX_FMT_NONE ||
        fmt == AV_PIX_FMT_UYYVYY411)
        return mp_only_imgfmt_desc(mpfmt);

    struct mp_imgfmt_desc desc = {
        .id = mpfmt,
        .avformat = fmt,
        .chroma_xs = pd->log2_chroma_w,
        .chroma_ys = pd->log2_chroma_h,
        .component_bits = pd->comp[0].depth_minus1 + 1,
    };

    int planedepth[4] = {0};
    int el_size = (pd->flags & AV_PIX_FMT_FLAG_BITSTREAM) ? 1 : 8;
    bool need_endian = false; // single component is spread over >1 bytes
    for (int c = 0; c < pd->nb_components; c++) {
        AVComponentDescriptor d = pd->comp[c];
        // multiple components per plane -> Y is definitive, ignore chroma
        if (!desc.bpp[d.plane])
            desc.bpp[d.plane] = (d.step_minus1 + 1) * el_size;
        planedepth[d.plane] += d.depth_minus1 + 1;
        need_endian |= (d.depth_minus1 + 1 + d.shift) > 8;
        if (d.depth_minus1 + 1 != desc.component_bits)
            desc.component_bits = 0;
    }

    for (int p = 0; p < 4; p++) {
        if (desc.bpp[p])
            desc.num_planes++;
    }

    desc.plane_bits = planedepth[0];

    // Check whether any components overlap other components (per plane).
    // We're cheating/simplifying here: we assume that this happens if a shift
    // is set - which is wrong in general (could be needed for padding, instead
    // of overlapping bits of another component). Needed for rgb444le/be.
    bool component_byte_overlap = false;
    for (int c = 0; c < pd->nb_components; c++) {
        AVComponentDescriptor d = pd->comp[c];
        component_byte_overlap |= d.shift > 0 && planedepth[d.plane] > 8;
    }

    // If every component sits in its own byte, or all components are within
    // a single byte, no endian-dependent access is needed. If components
    // stride bytes (like with packed 2 byte RGB formats), endian-dependent
    // access is needed.
    need_endian |= component_byte_overlap;

    if (!need_endian) {
        desc.flags |= MP_IMGFLAG_LE | MP_IMGFLAG_BE;
    } else {
        desc.flags |= (pd->flags & AV_PIX_FMT_FLAG_BE)
                      ? MP_IMGFLAG_BE : MP_IMGFLAG_LE;
    }

    if (fmt == AV_PIX_FMT_XYZ12LE || fmt == AV_PIX_FMT_XYZ12BE) {
        desc.flags |= MP_IMGFLAG_XYZ;
    } else if (!(pd->flags & AV_PIX_FMT_FLAG_RGB) &&
               fmt != AV_PIX_FMT_MONOBLACK &&
               fmt != AV_PIX_FMT_PAL8)
    {
        desc.flags |= MP_IMGFLAG_YUV;
    } else {
        desc.flags |= MP_IMGFLAG_RGB;
    }

    if (pd->flags & AV_PIX_FMT_FLAG_ALPHA)
        desc.flags |= MP_IMGFLAG_ALPHA;

    if (mpfmt >= IMGFMT_RGB0_START && mpfmt <= IMGFMT_RGB0_END)
        desc.flags &= ~MP_IMGFLAG_ALPHA;

    if (desc.num_planes == pd->nb_components)
        desc.flags |= MP_IMGFLAG_PLANAR;

    if (!(pd->flags & AV_PIX_FMT_FLAG_HWACCEL) &&
        !(pd->flags & AV_PIX_FMT_FLAG_BITSTREAM))
    {
        desc.flags |= MP_IMGFLAG_BYTE_ALIGNED;
        for (int p = 0; p < desc.num_planes; p++)
            desc.bytes[p] = desc.bpp[p] / 8;
    }

    // PSEUDOPAL is a complete braindeath nightmare, however it seems various
    // parts of FFmpeg expect that it has a palette allocated.
    if (pd->flags & (AV_PIX_FMT_FLAG_PAL | AV_PIX_FMT_FLAG_PSEUDOPAL))
        desc.flags |= MP_IMGFLAG_PAL;

    if ((desc.flags & MP_IMGFLAG_YUV) && (desc.flags & MP_IMGFLAG_BYTE_ALIGNED))
    {
        bool same_depth = true;
        for (int p = 0; p < desc.num_planes; p++) {
            same_depth &= planedepth[p] == planedepth[0] &&
                          desc.bpp[p] == desc.bpp[0];
        }
        if (same_depth && pd->nb_components == desc.num_planes)
            desc.flags |= MP_IMGFLAG_YUV_P;
    }

    for (int p = 0; p < desc.num_planes; p++) {
        desc.xs[p] = (p == 1 || p == 2) ? desc.chroma_xs : 0;
        desc.ys[p] = (p == 1 || p == 2) ? desc.chroma_ys : 0;
    }

    desc.align_x = 1 << desc.chroma_xs;
    desc.align_y = 1 << desc.chroma_ys;

    if ((desc.bpp[0] % 8) != 0)
        desc.align_x = 8 / desc.bpp[0]; // expect power of 2

    if (pd->flags & AV_PIX_FMT_FLAG_HWACCEL) {
        desc.flags |= MP_IMGFLAG_HWACCEL;
        desc.plane_bits = 8; // usually restricted to 8 bit; may change
    }

    if (desc.chroma_xs || desc.chroma_ys)
        desc.flags |= MP_IMGFLAG_SUBSAMPLED;

    return desc;
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

#if LIBAVUTIL_VERSION_MICRO < 100
#define avcodec_find_best_pix_fmt_of_list avcodec_find_best_pix_fmt2
#endif

// Compare the dst image formats, and return the one which can carry more data
// (e.g. higher depth, more color components, lower chroma subsampling, etc.),
// with respect to what is required to keep most of the src format.
// Returns the imgfmt, or 0 on error.
int mp_imgfmt_select_best(int dst1, int dst2, int src)
{
    enum AVPixelFormat dst1pxf = imgfmt2pixfmt(dst1);
    enum AVPixelFormat dst2pxf = imgfmt2pixfmt(dst2);
    enum AVPixelFormat srcpxf = imgfmt2pixfmt(src);
    enum AVPixelFormat dstlist[] = {dst1pxf, dst2pxf, AV_PIX_FMT_NONE};
    return pixfmt2imgfmt(avcodec_find_best_pix_fmt_of_list(dstlist, srcpxf, 1, 0));
}

#if 0

#include <libavutil/frame.h>
#include "sws_utils.h"

int main(int argc, char **argv)
{
    const AVPixFmtDescriptor *avd = av_pix_fmt_desc_next(NULL);
    for (; avd; avd = av_pix_fmt_desc_next(avd)) {
        enum AVPixelFormat fmt = av_pix_fmt_desc_get_id(avd);
        if (fmt == AV_PIX_FMT_YUVJ420P || fmt == AV_PIX_FMT_YUVJ422P ||
            fmt == AV_PIX_FMT_YUVJ444P || fmt == AV_PIX_FMT_YUVJ440P)
            continue;
        printf("%s (%d)", avd->name, (int)fmt);
        int mpfmt = pixfmt2imgfmt(fmt);
        bool generic = mpfmt >= IMGFMT_AVPIXFMT_START &&
                       mpfmt < IMGFMT_AVPIXFMT_END;
        printf(" mp=%d%s\n  ", mpfmt, generic ? " [GENERIC]" : "");
        struct mp_imgfmt_desc d = mp_imgfmt_get_desc(mpfmt);
        if (d.id)
            assert(d.avformat == fmt);
#define FLAG(t, c) if (d.flags & (t)) printf("[%s]", c);
        FLAG(MP_IMGFLAG_BYTE_ALIGNED, "BA")
        FLAG(MP_IMGFLAG_ALPHA, "a")
        FLAG(MP_IMGFLAG_PLANAR, "P")
        FLAG(MP_IMGFLAG_YUV_P, "YUVP")
        FLAG(MP_IMGFLAG_YUV, "yuv")
        FLAG(MP_IMGFLAG_RGB, "rgb")
        FLAG(MP_IMGFLAG_XYZ, "xyz")
        FLAG(MP_IMGFLAG_LE, "le")
        FLAG(MP_IMGFLAG_BE, "be")
        FLAG(MP_IMGFLAG_PAL, "pal")
        FLAG(MP_IMGFLAG_HWACCEL, "hw")
        printf("\n");
        printf("  planes=%d, chroma=%d:%d align=%d:%d bits=%d cbits=%d\n",
               d.num_planes, d.chroma_xs, d.chroma_ys, d.align_x, d.align_y,
               d.plane_bits, d.component_bits);
        printf("  {");
        for (int n = 0; n < MP_MAX_PLANES; n++)
            printf("%d/%d/[%d:%d] ", d.bytes[n], d.bpp[n], d.xs[n], d.ys[n]);
        printf("}\n");
        if (mpfmt && !(d.flags & MP_IMGFLAG_HWACCEL) && fmt != AV_PIX_FMT_UYYVYY411)
        {
            AVFrame *fr = av_frame_alloc();
            fr->format = fmt;
            fr->width = 128;
            fr->height = 128;
            int err = av_frame_get_buffer(fr, SWS_MIN_BYTE_ALIGN);
            assert(err >= 0);
            struct mp_image *mpi = mp_image_alloc(mpfmt, fr->width, fr->height);
            assert(mpi);
            // A rather fuzzy test, which might fail even if there's no bug.
            for (int n = 0; n < 4; n++) {
                assert(!!mpi->planes[n] == !!fr->data[n]);
                assert(mpi->stride[n] == fr->linesize[n]);
            }
            talloc_free(mpi);
            av_frame_free(&fr);
        }
    }
}

#endif
