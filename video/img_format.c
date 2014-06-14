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

#include "compat/libav.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"

#define FMT(string, id)                                 \
    {string,            id},

#define FMT_ENDIAN(string, id)                          \
    {string,            id},                            \
    {string "le",       MP_CONCAT(id, _LE)},            \
    {string "be",       MP_CONCAT(id, _BE)},            \

struct mp_imgfmt_entry {
    const char *name;
    int fmt;
};

static const struct mp_imgfmt_entry mp_imgfmt_list[] = {
    // not in ffmpeg
    FMT("vdpau_output",         IMGFMT_VDPAU_OUTPUT)
    // these names are weirdly different from FFmpeg's
    FMT_ENDIAN("rgb12",         IMGFMT_RGB12)
    FMT_ENDIAN("rgb15",         IMGFMT_RGB15)
    FMT_ENDIAN("rgb16",         IMGFMT_RGB16)
    FMT_ENDIAN("bgr12",         IMGFMT_BGR12)
    FMT_ENDIAN("bgr15",         IMGFMT_BGR15)
    FMT_ENDIAN("bgr16",         IMGFMT_BGR16)
    // the MPlayer derived names have components in reverse order
    FMT("rgb8",                 IMGFMT_RGB8)
    FMT("bgr8",                 IMGFMT_BGR8)
    FMT("rgb4_byte",            IMGFMT_RGB4_BYTE)
    FMT("bgr4_byte",            IMGFMT_BGR4_BYTE)
    FMT("rgb4",                 IMGFMT_RGB4)
    FMT("bgr4",                 IMGFMT_BGR4)
    // FFmpeg names have an annoying "_vld" suffix
    FMT("vda",                  IMGFMT_VDA)
    FMT("vaapi",                IMGFMT_VAAPI)
    // names below this are not preferred over the FFmpeg names
    // the "none" entry makes mp_imgfmt_to_name prefer FFmpeg names
    FMT("none",                 0)
    // endian-specific aliases (not in FFmpeg)
    FMT("rgb32",                IMGFMT_RGB32)
    FMT("bgr32",                IMGFMT_BGR32)
    // old names we keep around
    FMT("y8",                   IMGFMT_Y8)
    FMT("420p",                 IMGFMT_420P)
    FMT("yv12",                 IMGFMT_420P)
    FMT("420p16",               IMGFMT_420P16)
    FMT("420p10",               IMGFMT_420P10)
    FMT("444p",                 IMGFMT_444P)
    FMT("444p9",                IMGFMT_444P9)
    FMT("444p10",               IMGFMT_444P10)
    FMT("422p",                 IMGFMT_422P)
    FMT("422p9",                IMGFMT_422P9)
    FMT("422p10",               IMGFMT_422P10)
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
            .flags = MP_IMGFLAG_BE | MP_IMGFLAG_LE | MP_IMGFLAG_RGB,
        };
    }
    return (struct mp_imgfmt_desc) {0};
}

struct mp_imgfmt_desc mp_imgfmt_get_desc(int mpfmt)
{
    enum AVPixelFormat fmt = imgfmt2pixfmt(mpfmt);
    const AVPixFmtDescriptor *pd = av_pix_fmt_desc_get(fmt);
    if (!pd || fmt == AV_PIX_FMT_NONE)
        return mp_only_imgfmt_desc(mpfmt);

    struct mp_imgfmt_desc desc = {
        .id = mpfmt,
        .avformat = fmt,
        .chroma_xs = pd->log2_chroma_w,
        .chroma_ys = pd->log2_chroma_h,
    };

    int planedepth[4] = {0};
    int el_size = (pd->flags & AV_PIX_FMT_FLAG_BITSTREAM) ? 1 : 8;
    for (int c = 0; c < pd->nb_components; c++) {
        AVComponentDescriptor d = pd->comp[c];
        // multiple components per plane -> Y is definitive, ignore chroma
        if (!desc.bpp[d.plane])
            desc.bpp[d.plane] = (d.step_minus1 + 1) * el_size;
        planedepth[d.plane] += d.depth_minus1 + 1;
    }

    for (int p = 0; p < 4; p++) {
        if (desc.bpp[p])
            desc.num_planes++;
    }

    // Packed RGB formats are the only formats that have less than 8 bits per
    // component, and still require endian dependent access.
    if (pd->comp[0].depth_minus1 + 1 <= 8 &&
        !(mpfmt >= IMGFMT_RGB12_LE && mpfmt <= IMGFMT_BGR16_BE))
    {
        desc.flags |= MP_IMGFLAG_LE | MP_IMGFLAG_BE;
    } else {
        desc.flags |= (pd->flags & AV_PIX_FMT_FLAG_BE)
                      ? MP_IMGFLAG_BE : MP_IMGFLAG_LE;
    }

    desc.plane_bits = planedepth[0];

    if (mpfmt == IMGFMT_XYZ12_LE || mpfmt == IMGFMT_XYZ12_BE) {
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
