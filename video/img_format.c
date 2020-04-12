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

#include <assert.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>

#include "config.h"

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
    {"rgb30",           IMGFMT_RGB30},
    {"yap8",            IMGFMT_YAP8},
    {"yap16",           IMGFMT_YAP16},
    // FFmpeg names have an annoying "_vld" suffix
    {"videotoolbox",    IMGFMT_VIDEOTOOLBOX},
    {"vaapi",           IMGFMT_VAAPI},
    {"none",            0},
    {0}
};

char **mp_imgfmt_name_list(void)
{
    int count = IMGFMT_END - IMGFMT_START;
    char **list = talloc_zero_array(NULL, char *, count + 1);
    int num = 0;
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        const char *name = mp_imgfmt_to_name(n);
        if (strcmp(name, "unknown") != 0)
            list[num++] = talloc_strdup(list, name);
    }
    return list;
}

int mp_imgfmt_from_name(bstr name)
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
    case IMGFMT_RGB30:
        return (struct mp_imgfmt_desc) {
            .id = mpfmt,
            .avformat = AV_PIX_FMT_NONE,
            .flags = MP_IMGFLAG_BYTE_ALIGNED | MP_IMGFLAG_NE | MP_IMGFLAG_RGB,
            .num_planes = 1,
            .align_x = 1,
            .align_y = 1,
            .bytes = {4},
            .bpp = {32},
            .plane_bits = 30,
            .component_bits = 10,
        };
    case IMGFMT_YAP8:
        return (struct mp_imgfmt_desc) {
            .id = mpfmt,
            .avformat = AV_PIX_FMT_NONE,
            .flags = MP_IMGFLAG_BYTE_ALIGNED | MP_IMGFLAG_NE | MP_IMGFLAG_YUV |
                     MP_IMGFLAG_YUV_P,
            .num_planes = 2,
            .align_x = 1,
            .align_y = 1,
            .bytes = {1, 1},
            .bpp = {8, 8},
            .plane_bits = 8,
            .component_bits = 8,
        };
    case IMGFMT_YAP16:
        return (struct mp_imgfmt_desc) {
            .id = mpfmt,
            .avformat = AV_PIX_FMT_NONE,
            .flags = MP_IMGFLAG_BYTE_ALIGNED | MP_IMGFLAG_NE | MP_IMGFLAG_YUV |
                     MP_IMGFLAG_YUV_P,
            .num_planes = 2,
            .align_x = 1,
            .align_y = 1,
            .bytes = {2, 2},
            .bpp = {16, 16},
            .plane_bits = 16,
            .component_bits = 16,
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
    enum mp_component_type is_uint =
        mp_imgfmt_get_component_type(mpfmt) == MP_COMPONENT_TYPE_UINT;

    struct mp_imgfmt_desc desc = {
        .id = mpfmt,
        .avformat = fmt,
        .chroma_xs = pd->log2_chroma_w,
        .chroma_ys = pd->log2_chroma_h,
    };

    int planedepth[4] = {0};
    int el_size = (pd->flags & AV_PIX_FMT_FLAG_BITSTREAM) ? 1 : 8;
    bool need_endian = false; // single component is spread over >1 bytes
    int shift = -1; // shift for all components, or -1 if not uniform
    for (int c = 0; c < pd->nb_components; c++) {
        AVComponentDescriptor d = pd->comp[c];
        // multiple components per plane -> Y is definitive, ignore chroma
        if (!desc.bpp[d.plane])
            desc.bpp[d.plane] = d.step * el_size;
        planedepth[d.plane] += d.depth;
        need_endian |= (d.depth + d.shift) > 8;
        if (c == 0)
            desc.component_bits = d.depth;
        if (d.depth != desc.component_bits)
            desc.component_bits = 0;
        if (c == 0)
            shift = d.shift;
        if (shift != d.shift)
            shift = -1;
    }

    for (int p = 0; p < 4; p++) {
        if (desc.bpp[p])
            desc.num_planes++;
    }

    desc.plane_bits = planedepth[0];

    // Check whether any components overlap other components (per plane).
    // We're cheating/simplifying here: we assume that this happens if a shift
    // is set - which is wrong in general (could be needed for padding, instead
    // of overlapping bits of another component - use the "< 8" test to exclude
    // "normal" formats which use this for padding, like p010).
    // Needed for rgb444le/be.
    bool component_byte_overlap = false;
    for (int c = 0; c < pd->nb_components; c++) {
        AVComponentDescriptor d = pd->comp[c];
        component_byte_overlap |= d.shift > 0 && planedepth[d.plane] > 8 &&
                                  desc.component_bits < 8;
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

    if ((pd->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        desc.flags |= MP_IMGFLAG_HWACCEL;
    } else if (fmt == AV_PIX_FMT_XYZ12LE || fmt == AV_PIX_FMT_XYZ12BE) {
        /* nothing */
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

    if (!(pd->flags & AV_PIX_FMT_FLAG_HWACCEL) &&
        !(pd->flags & AV_PIX_FMT_FLAG_BITSTREAM))
    {
        desc.flags |= MP_IMGFLAG_BYTE_ALIGNED;
        for (int p = 0; p < desc.num_planes; p++)
            desc.bytes[p] = desc.bpp[p] / 8;
    }

    if (pd->flags & AV_PIX_FMT_FLAG_PAL)
        desc.flags |= MP_IMGFLAG_PAL;

    if ((desc.flags & (MP_IMGFLAG_YUV | MP_IMGFLAG_RGB))
        && (desc.flags & MP_IMGFLAG_BYTE_ALIGNED)
        && !(pd->flags & AV_PIX_FMT_FLAG_PAL)
        && !component_byte_overlap
        && shift >= 0 && is_uint)
    {
        bool same_depth = true;
        for (int p = 0; p < desc.num_planes; p++) {
            same_depth &= planedepth[p] == planedepth[0] &&
                          desc.bpp[p] == desc.bpp[0];
        }
        if (same_depth && pd->nb_components == desc.num_planes) {
            if (desc.flags & MP_IMGFLAG_YUV) {
                desc.flags |= MP_IMGFLAG_YUV_P;
            } else {
                desc.flags |= MP_IMGFLAG_RGB_P;
            }
        }
        if (pd->nb_components == 3 && desc.num_planes == 2 &&
            planedepth[1] == planedepth[0] * 2 &&
            desc.bpp[1] == desc.bpp[0] * 2 &&
            (desc.flags & MP_IMGFLAG_YUV))
        {

            desc.flags |= MP_IMGFLAG_YUV_NV;
        }
        if (desc.flags & (MP_IMGFLAG_YUV_P | MP_IMGFLAG_RGB_P | MP_IMGFLAG_YUV_NV))
            desc.component_bits += shift;
    }

    for (int p = 0; p < desc.num_planes; p++) {
        desc.xs[p] = (p == 1 || p == 2) ? desc.chroma_xs : 0;
        desc.ys[p] = (p == 1 || p == 2) ? desc.chroma_ys : 0;
    }

    desc.align_x = 1 << desc.chroma_xs;
    desc.align_y = 1 << desc.chroma_ys;

    if ((desc.bpp[0] % 8) != 0)
        desc.align_x = 8 / desc.bpp[0]; // expect power of 2

    if (desc.flags & MP_IMGFLAG_HWACCEL) {
        desc.component_bits = 0;
        desc.plane_bits = 0;
    }

    return desc;
}

static bool validate_regular_imgfmt(const struct mp_regular_imgfmt *fmt)
{
    bool present[MP_NUM_COMPONENTS] = {0};
    int n_comp = 0;

    for (int n = 0; n < fmt->num_planes; n++) {
        const struct mp_regular_imgfmt_plane *plane = &fmt->planes[n];
        n_comp += plane->num_components;
        if (n_comp > MP_NUM_COMPONENTS)
            return false;
        if (!plane->num_components)
            return false; // no empty planes in between allowed

        bool pad_only = true;
        int chroma_luma = 0; // luma: 1, chroma: 2, both: 3
        for (int i = 0; i < plane->num_components; i++) {
            int comp = plane->components[i];
            if (comp > MP_NUM_COMPONENTS)
                return false;
            if (comp == 0)
                continue;
            pad_only = false;
            if (present[comp - 1])
                return false; // no duplicates
            present[comp - 1] = true;
            chroma_luma |= (comp == 2 || comp == 3) ? 2 : 1;
        }
        if (pad_only)
            return false; // no planes with only padding allowed
        if ((fmt->chroma_w > 1 || fmt->chroma_h > 1) && chroma_luma == 3)
            return false; // separate chroma/luma planes required
    }

    if (!(present[0] || present[3]) ||  // at least component 1 or alpha needed
        (present[1] && !present[0]) ||  // component 2 requires component 1
        (present[2] && !present[1]))    // component 3 requires component 2
        return false;

    return true;
}

enum mp_csp mp_imgfmt_get_forced_csp(int imgfmt)
{
    if (imgfmt == IMGFMT_RGB30)
        return MP_CSP_RGB;

    enum AVPixelFormat pixfmt = imgfmt2pixfmt(imgfmt);
    const AVPixFmtDescriptor *pixdesc = av_pix_fmt_desc_get(pixfmt);

    // FFmpeg does not provide a flag for XYZ, so this is the best we can do.
    if (pixdesc && strncmp(pixdesc->name, "xyz", 3) == 0)
        return MP_CSP_XYZ;

    if (pixdesc && (pixdesc->flags & AV_PIX_FMT_FLAG_RGB))
        return MP_CSP_RGB;

    if (pixfmt == AV_PIX_FMT_PAL8 || pixfmt == AV_PIX_FMT_MONOBLACK)
        return MP_CSP_RGB;

    return MP_CSP_AUTO;
}

enum mp_component_type mp_imgfmt_get_component_type(int imgfmt)
{
    if (imgfmt == IMGFMT_RGB30 ||
        imgfmt == IMGFMT_YAP8 ||
        imgfmt == IMGFMT_YAP16)
        return MP_COMPONENT_TYPE_UINT;

    const AVPixFmtDescriptor *pixdesc =
        av_pix_fmt_desc_get(imgfmt2pixfmt(imgfmt));

    if (!pixdesc || (pixdesc->flags & AV_PIX_FMT_FLAG_HWACCEL))
        return MP_COMPONENT_TYPE_UNKNOWN;

    if (pixdesc->flags & AV_PIX_FMT_FLAG_FLOAT)
        return MP_COMPONENT_TYPE_FLOAT;

    return MP_COMPONENT_TYPE_UINT;
}

int mp_find_other_endian(int imgfmt)
{
    return pixfmt2imgfmt(av_pix_fmt_swap_endianness(imgfmt2pixfmt(imgfmt)));
}

static bool is_native_endian(const AVPixFmtDescriptor *pixdesc)
{
    enum AVPixelFormat pixfmt = av_pix_fmt_desc_get_id(pixdesc);
    enum AVPixelFormat other = av_pix_fmt_swap_endianness(pixfmt);
    if (other == AV_PIX_FMT_NONE || other == pixfmt)
        return true; // no endian nonsense
    bool is_le = *(char *)&(uint32_t){1};
    return pixdesc && (is_le != !!(pixdesc->flags & AV_PIX_FMT_FLAG_BE));
}

static bool mp_only_regular_imgfmt(struct mp_regular_imgfmt *dst, int imgfmt)
{
    switch (imgfmt) {
    case IMGFMT_YAP8:
        *dst = (struct mp_regular_imgfmt) {
            .component_type = MP_COMPONENT_TYPE_UINT,
            .component_size = 1,
            .num_planes = 2,
            .planes = {
                {.num_components = 1, .components = {1}},
                {.num_components = 1, .components = {4}},
            },
            .chroma_w = 1,
            .chroma_h = 1,
        };
        return true;
    case IMGFMT_YAP16:
        *dst = (struct mp_regular_imgfmt) {
            .component_type = MP_COMPONENT_TYPE_UINT,
            .component_size = 2,
            .num_planes = 2,
            .planes = {
                {.num_components = 1, .components = {1}},
                {.num_components = 1, .components = {4}},
            },
            .chroma_w = 1,
            .chroma_h = 1,
        };
        return true;
    }
    return false;
}

bool mp_get_regular_imgfmt(struct mp_regular_imgfmt *dst, int imgfmt)
{
    struct mp_regular_imgfmt res = {0};

    const AVPixFmtDescriptor *pixdesc =
        av_pix_fmt_desc_get(imgfmt2pixfmt(imgfmt));

    if (!pixdesc)
        return mp_only_regular_imgfmt(dst, imgfmt);

    if ((pixdesc->flags & AV_PIX_FMT_FLAG_BITSTREAM) ||
        (pixdesc->flags & AV_PIX_FMT_FLAG_HWACCEL) ||
        (pixdesc->flags & AV_PIX_FMT_FLAG_PAL) ||
        pixdesc->nb_components < 1 ||
        pixdesc->nb_components > MP_NUM_COMPONENTS ||
        !is_native_endian(pixdesc))
        return false;

    res.component_type = mp_imgfmt_get_component_type(imgfmt);
    if (!res.component_type)
        return false;

    const AVComponentDescriptor *comp0 = &pixdesc->comp[0];

    int depth = comp0->depth + comp0->shift;
    if (depth < 1 || depth > 64)
        return false;
    res.component_size = (depth + 7) / 8;

    for (int n = 0; n < pixdesc->nb_components; n++) {
        const AVComponentDescriptor *comp = &pixdesc->comp[n];

        if (comp->plane < 0 || comp->plane >= MP_MAX_PLANES)
            return false;

        res.num_planes = MPMAX(res.num_planes, comp->plane + 1);

        // We support uniform depth only.
        if (comp->depth != comp0->depth || comp->shift != comp0->shift)
            return false;

        // Uniform component size; even the padding must have same size.
        int ncomp = comp->step / res.component_size;
        if (!ncomp || ncomp * res.component_size != comp->step)
            return false;

        struct mp_regular_imgfmt_plane *plane = &res.planes[comp->plane];

        if (plane->num_components && plane->num_components != ncomp)
            return false;
        plane->num_components = ncomp;

        int pos = comp->offset / res.component_size;
        if (pos < 0 || pos >= ncomp || ncomp > MP_NUM_COMPONENTS)
            return false;
        if (plane->components[pos])
            return false;
        plane->components[pos] = n + 1;
    }

    // Make sure alpha is always component 4.
    if (pixdesc->nb_components == 2 && (pixdesc->flags & AV_PIX_FMT_FLAG_ALPHA)) {
        for (int n = 0; n < res.num_planes; n++) {
            for (int i = 0; i < res.planes[n].num_components; i++) {
                if (res.planes[n].components[i] == 2)
                    res.planes[n].components[i] = 4;
            }
        }
    }

    res.component_pad = comp0->depth - res.component_size * 8;
    if (comp0->shift) {
        // We support padding only on 1 side.
        if (comp0->shift + comp0->depth != res.component_size * 8)
            return false;
        res.component_pad = -res.component_pad;
    }

    res.chroma_w = 1 << pixdesc->log2_chroma_w;
    res.chroma_h = 1 << pixdesc->log2_chroma_h;

    if (pixdesc->flags & AV_PIX_FMT_FLAG_BAYER)
        return false; // it's satan himself

    res.forced_csp = mp_imgfmt_get_forced_csp(imgfmt);

    if (!validate_regular_imgfmt(&res))
        return false;

    *dst = res;
    return true;
}

static bool regular_imgfmt_equals(struct mp_regular_imgfmt *a,
                                  struct mp_regular_imgfmt *b)
{
    if (a->component_type != b->component_type ||
        a->component_size != b->component_size ||
        a->num_planes     != b->num_planes ||
        a->component_pad  != b->component_pad ||
        a->forced_csp     != b->forced_csp ||
        a->chroma_w       != b->chroma_w ||
        a->chroma_h       != b->chroma_h)
        return false;

    for (int n = 0; n < a->num_planes; n++) {
        int num_comps = a->planes[n].num_components;
        if (num_comps != b->planes[n].num_components)
            return false;
        for (int i = 0; i < num_comps; i++) {
            if (a->planes[n].components[i] != b->planes[n].components[i])
                return false;
        }
    }

    return true;
}

// Find a format that matches this one exactly.
int mp_find_regular_imgfmt(struct mp_regular_imgfmt *src)
{
    for (int n = IMGFMT_START + 1; n < IMGFMT_END; n++) {
        struct mp_regular_imgfmt f;
        if (mp_get_regular_imgfmt(&f, n) && regular_imgfmt_equals(src, &f))
            return n;
    }
    return 0;
}

// Find a format that has the given flags set with the following configuration.
int mp_imgfmt_find(int xs, int ys, int planes, int component_bits, int flags)
{
    for (int n = IMGFMT_START + 1; n < IMGFMT_END; n++) {
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(n);
        if (desc.id && ((desc.flags & flags) == flags)) {
            if (desc.num_planes == planes && desc.chroma_xs == xs &&
                desc.chroma_ys == ys && desc.plane_bits == component_bits &&
                (desc.flags & MP_IMGFLAG_NE))
                return desc.id;
        }
    }
    return 0;
}

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

// Same as mp_imgfmt_select_best(), but with a list of dst formats.
int mp_imgfmt_select_best_list(int *dst, int num_dst, int src)
{
    int best = 0;
    for (int n = 0; n < num_dst; n++)
        best = best ? mp_imgfmt_select_best(best, dst[n], src) : dst[n];
    return best;
}
