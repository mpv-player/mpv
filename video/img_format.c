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
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>

#include "config.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"

struct mp_imgfmt_entry {
    const char *name;
    // valid if desc.id is set
    struct mp_imgfmt_desc desc;
    // valid if reg_desc.component_size is set
    struct mp_regular_imgfmt reg_desc;
    // valid if bits!=0
    struct mp_imgfmt_layout layout;
    // valid if non-0 and no reg_desc
    enum mp_csp forced_csp;
    enum mp_component_type ctype;
};

#define FRINGE_GBRP(def, dname, bits)                                       \
    [def - IMGFMT_CUST_BASE] = {                                            \
        .name = dname,                                                      \
        .reg_desc = { .component_type = MP_COMPONENT_TYPE_UINT,             \
                      .component_size = 1, .component_pad = bits - 8,       \
                      .num_planes = 3, .forced_csp = MP_CSP_RGB,            \
                      .planes = { {1, {2}}, {1, {3}}, {1, {1}} }, }, }

#define FLOAT_YUV(def, dname, xs, ys, a_planes)                             \
    [def - IMGFMT_CUST_BASE] = {                                            \
        .name = dname,                                                      \
        .reg_desc = { .component_type = MP_COMPONENT_TYPE_FLOAT,            \
                      .component_size = 4, .num_planes = a_planes,          \
                      .planes = { {1, {1}}, {1, {2}}, {1, {3}}, {1, {4}} }, \
                      .chroma_xs = xs, .chroma_ys = ys, }}

static const struct mp_imgfmt_entry mp_imgfmt_list[] = {
    // not in ffmpeg
    [IMGFMT_VDPAU_OUTPUT - IMGFMT_CUST_BASE] = {
        .name = "vdpau_output",
        .desc = {
            .id = IMGFMT_VDPAU_OUTPUT,
            .avformat = AV_PIX_FMT_NONE,
            .flags = MP_IMGFLAG_BE | MP_IMGFLAG_LE | MP_IMGFLAG_RGB |
                     MP_IMGFLAG_HWACCEL,
        },
    },
    [IMGFMT_RGB30 - IMGFMT_CUST_BASE] = {
        .name = "rgb30",
        .desc = {
            .id = IMGFMT_RGB30,
            .avformat = AV_PIX_FMT_NONE,
            .flags = MP_IMGFLAG_BYTE_ALIGNED | MP_IMGFLAG_NE | MP_IMGFLAG_RGB,
            .num_planes = 1,
            .align_x = 1,
            .align_y = 1,
            .bpp = {32},
        },
        .layout = { {32}, { {0, 20, 10}, {0, 10, 10}, {0, 0, 10} } },
        .forced_csp = MP_CSP_RGB,
        .ctype = MP_COMPONENT_TYPE_UINT,
    },
    [IMGFMT_YAP8 - IMGFMT_CUST_BASE] = {
        .name = "yap8",
        .reg_desc = {
            .component_type = MP_COMPONENT_TYPE_UINT,
            .component_size = 1,
            .num_planes = 2,
            .planes = { {1, {1}}, {1, {4}} },
        },
    },
    [IMGFMT_YAP16 - IMGFMT_CUST_BASE] = {
        .name = "yap16",
        .reg_desc = {
            .component_type = MP_COMPONENT_TYPE_UINT,
            .component_size = 2,
            .num_planes = 2,
            .planes = { {1, {1}}, {1, {4}} },
        },
    },
    [IMGFMT_Y1 - IMGFMT_CUST_BASE] = {
        .name = "y1",
        .reg_desc = {
            .component_type = MP_COMPONENT_TYPE_UINT,
            .component_size = 1,
            .component_pad = -7,
            .num_planes = 1,
            .forced_csp = MP_CSP_RGB,
            .planes = { {1, {1}} },
        },
    },
    [IMGFMT_YAPF - IMGFMT_CUST_BASE] = {
        .name = "grayaf32", // try to mimic ffmpeg naming convention
        .reg_desc = {
            .component_type = MP_COMPONENT_TYPE_FLOAT,
            .component_size = 4,
            .num_planes = 2,
            .planes = { {1, {1}}, {1, {4}} },
        },
    },
    FLOAT_YUV(IMGFMT_444PF,  "yuv444pf",  0, 0, 3),
    FLOAT_YUV(IMGFMT_444APF, "yuva444pf", 0, 0, 4),
    FLOAT_YUV(IMGFMT_420PF,  "yuv420pf",  1, 1, 3),
    FLOAT_YUV(IMGFMT_420APF, "yuva420pf", 1, 1, 4),
    FLOAT_YUV(IMGFMT_422PF,  "yuv422pf",  1, 0, 3),
    FLOAT_YUV(IMGFMT_422APF, "yuva422pf", 1, 0, 4),
    FLOAT_YUV(IMGFMT_440PF,  "yuv440pf",  0, 1, 3),
    FLOAT_YUV(IMGFMT_440APF, "yuva440pf", 0, 1, 4),
    FLOAT_YUV(IMGFMT_410PF,  "yuv410pf",  2, 2, 3),
    FLOAT_YUV(IMGFMT_410APF, "yuva410pf", 2, 2, 4),
    FLOAT_YUV(IMGFMT_411PF,  "yuv411pf",  2, 0, 3),
    FLOAT_YUV(IMGFMT_411APF, "yuva411pf", 2, 0, 4),
    FRINGE_GBRP(IMGFMT_GBRP1, "gbrp1", 1),
    FRINGE_GBRP(IMGFMT_GBRP2, "gbrp2", 2),
    FRINGE_GBRP(IMGFMT_GBRP3, "gbrp3", 3),
    FRINGE_GBRP(IMGFMT_GBRP4, "gbrp4", 4),
    FRINGE_GBRP(IMGFMT_GBRP5, "gbrp5", 5),
    FRINGE_GBRP(IMGFMT_GBRP6, "gbrp6", 6),
    // in FFmpeg, but FFmpeg names have an annoying "_vld" suffix
    [IMGFMT_VIDEOTOOLBOX - IMGFMT_CUST_BASE] = {
        .name = "videotoolbox",
    },
    [IMGFMT_VAAPI - IMGFMT_CUST_BASE] = {
        .name = "vaapi",
    },
};

static const struct mp_imgfmt_entry *get_mp_desc(int imgfmt)
{
    if (imgfmt < IMGFMT_CUST_BASE)
        return NULL;
    int index = imgfmt - IMGFMT_CUST_BASE;
    if (index >= MP_ARRAY_SIZE(mp_imgfmt_list))
        return NULL;
    const struct mp_imgfmt_entry *e = &mp_imgfmt_list[index];
    return e->name ? e : NULL;
}

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
    if (bstr_equals0(name, "none"))
        return 0;
    for (int n = 0; n < MP_ARRAY_SIZE(mp_imgfmt_list); n++) {
        const struct mp_imgfmt_entry *p = &mp_imgfmt_list[n];
        if (p->name && bstr_equals0(name, p->name))
            return IMGFMT_CUST_BASE + n;
    }
    return pixfmt2imgfmt(av_get_pix_fmt(mp_tprintf(80, "%.*s", BSTR_P(name))));
}

char *mp_imgfmt_to_name_buf(char *buf, size_t buf_size, int fmt)
{
    const struct mp_imgfmt_entry *p = get_mp_desc(fmt);
    const char *name = p ? p->name : NULL;
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

static struct mp_imgfmt_desc to_legacy_desc(int fmt, struct mp_regular_imgfmt reg)
{
    struct mp_imgfmt_desc desc = {
        .id = fmt,
        .avformat = AV_PIX_FMT_NONE,
        .flags = MP_IMGFLAG_BYTE_ALIGNED | MP_IMGFLAG_NE |
            (reg.forced_csp ? MP_IMGFLAG_RGB | MP_IMGFLAG_RGB_P
                            : MP_IMGFLAG_YUV | MP_IMGFLAG_YUV_P),
        .num_planes = reg.num_planes,
        .chroma_xs = reg.chroma_xs,
        .chroma_ys = reg.chroma_ys,
    };
    desc.align_x = 1 << reg.chroma_xs;
    desc.align_y = 1 << reg.chroma_ys;
    for (int p = 0; p < reg.num_planes; p++) {
        desc.bpp[p] = reg.component_size * 8;
        desc.xs[p] = p == 1 || p == 2 ? desc.chroma_xs : 0;
        desc.ys[p] = p == 1 || p == 2 ? desc.chroma_ys : 0;
        for (int c = 0; c < reg.planes[p].num_components; c++) {
            if (reg.planes[p].components[c] == 4)
                desc.flags |= MP_IMGFLAG_ALPHA;
        }
    }
    return desc;
}

void mp_imgfmt_get_layout(int mpfmt, struct mp_imgfmt_layout *p_desc)
{
    const struct mp_imgfmt_entry *mpdesc = get_mp_desc(mpfmt);
    if (mpdesc && mpdesc->reg_desc.component_size) {
        *p_desc = (struct mp_imgfmt_layout){{0}};
        return;
    }
    if (mpdesc && mpdesc->layout.bits) {
        *p_desc = mpdesc->layout;
        return;
    }

    enum AVPixelFormat fmt = imgfmt2pixfmt(mpfmt);
    const AVPixFmtDescriptor *pd = av_pix_fmt_desc_get(fmt);
    if (!pd ||
        (pd->flags & AV_PIX_FMT_FLAG_PAL) ||
        (pd->flags & AV_PIX_FMT_FLAG_HWACCEL))
        goto fail;

    bool has_alpha = pd->flags & AV_PIX_FMT_FLAG_ALPHA;
    if (pd->nb_components != 1 + has_alpha &&
        pd->nb_components != 3 + has_alpha)
        goto fail;

    struct mp_imgfmt_layout desc = {0};

    // Very convenient: we assume we're always on little endian, and FFmpeg
    // explicitly marks big endian formats => don't need to guess whether a
    // format is little endian, or not affected by byte order.
    bool is_be = pd->flags & AV_PIX_FMT_FLAG_BE;

    // Packed sub-sampled YUV is very... special.
    bool is_packed_ss_yuv = pd->log2_chroma_w && !pd->log2_chroma_h &&
        (1 << pd->log2_chroma_w) <= MP_ARRAY_SIZE(desc.extra_luma_offsets) + 1 &&
        pd->comp[1].plane == 0 && pd->comp[2].plane == 0 &&
        pd->nb_components == 3;

    if (is_packed_ss_yuv) {
        desc.extra_w = (1 << pd->log2_chroma_w) - 1;
        desc.bits[0] = pd->comp[1].step * 8;
    }

    int num_planes = 0;
    int el_bits = (pd->flags & AV_PIX_FMT_FLAG_BITSTREAM) ? 1 : 8;
    for (int c = 0; c < pd->nb_components; c++) {
        const AVComponentDescriptor *d = &pd->comp[c];
        if (d->plane >= MP_MAX_PLANES)
            goto fail;

        num_planes = MPMAX(num_planes, d->plane + 1);

        int plane_bits = desc.bits[d->plane];
        int c_bits = d->step * el_bits;

        // The first component wins, because either all components result in
        // the same value, or luma wins (luma always comes before chroma).
        if (plane_bits) {
            if (c_bits > plane_bits)
                goto fail; // inconsistent
        } else {
            desc.bits[d->plane] = plane_bits = c_bits;
        }

        int shift = d->shift;
        // What the fuck: for some inexplicable reason, MONOB uses shift=7
        // in pixdesc, which is basically out of bounds. Pixdesc bug?
        // Make it behave like MONOW. (No, the bit-order is not different.)
        if (fmt == AV_PIX_FMT_MONOBLACK)
            shift = 0;

        int offset = d->offset * el_bits;
        // The pixdesc logic for reading and endian swapping is as follows
        // (reverse engineered from av_read_image_line2()):
        // - determine a word size that will include the component fully;
        //   this includes the "active" bits and the amount "shifted" away
        //   (for example shift=7/depth=18 => 32 bit word reading [31:0])
        // - the same format can use different word sizes (e.g. bgr565: the R
        //   component at offset 0 is read as 8 bit; BG is read as 16 bits)
        // - if BE flag is set, swap the word before proceeding
        // - extract via shift and mask derived by depth
        int word = mp_round_next_power_of_2(MPMAX(d->depth + shift, 8)) / 8;
        // The purpose of this is unknown. It's an absurdity fished out of
        // av_read_image_line2()'s implementation. It seems technically
        // unnecessary, and provides no information. On the other hand, it
        // compensates for seemingly bogus packed integer pixdescs; this
        // is "why" some formats use d->offset = -1.
        if (is_be && el_bits == 8 && word == 1)
            offset += 8;
        // Pixdesc's model requires accesses with varying word-sizes. This
        // is complete bullshit, so we transform it into word swaps before
        // further processing.
        if (is_be && word == 1) {
            // Probably packed RGB formats with varying word sizes. Assume
            // the word access size is the entire pixel.
            if (plane_bits % 8 || plane_bits >= 64)
                goto fail;
            if (!desc.endian_bytes)
                desc.endian_bytes = plane_bits / 8;
            if (desc.endian_bytes != plane_bits / 8)
                goto fail;
            offset = desc.endian_bytes * 8 - 8 - offset;
        }
        if (is_be && word > 1) {
            if (desc.endian_bytes && desc.endian_bytes != word)
                goto fail; // fortunately not needed/never happens
            if (word >= 64)
                goto fail;
            desc.endian_bytes = word;
        }
        // We always use bit offsets; this doesn't lose any information,
        // and pixdesc is merely more redundant.
        offset += shift;
        if (offset < 0 || offset >= (1 << 6))
            goto fail;
        if (offset + d->depth > plane_bits)
            goto fail;
        if (d->depth < 0 || d->depth >= (1 << 6))
            goto fail;
        desc.comps[c] = (struct mp_imgfmt_comp_desc){
            .plane = d->plane,
            .offset = offset,
            .size = d->depth,
        };
    }

    for (int p = 0; p < num_planes; p++) {
        if (!desc.bits[p])
            goto fail; // plane doesn't exist
    }

    // What the fuck: this is probably a pixdesc bug, so fix it.
    if (fmt == AV_PIX_FMT_RGB8) {
        desc.comps[2] = (struct mp_imgfmt_comp_desc){0, 0, 2};
        desc.comps[1] = (struct mp_imgfmt_comp_desc){0, 2, 3};
        desc.comps[0] = (struct mp_imgfmt_comp_desc){0, 5, 3};
    }

    // Overlap test. If any shared bits are happening, this is not a format we
    // can represent (or it's something like Bayer: components in the same bits,
    // but different alternating lines).
    bool any_shared_bits = false;
    bool any_shared_bytes = false;
    for (int c = 0; c < pd->nb_components; c++) {
        for (int i = 0; i < c; i++) {
            struct mp_imgfmt_comp_desc *c1 = &desc.comps[c];
            struct mp_imgfmt_comp_desc *c2 = &desc.comps[i];
            if (c1->plane == c2->plane) {
                if (c1->offset + c1->size > c2->offset &&
                    c2->offset + c2->size > c1->offset)
                    any_shared_bits = true;
                if ((c1->offset + c1->size + 7) / 8u > c2->offset / 8u &&
                    (c2->offset + c2->size + 7) / 8u > c1->offset / 8u)
                    any_shared_bytes = true;
            }
        }
    }

    if (any_shared_bits) {
        for (int c = 0; c < pd->nb_components; c++)
            desc.comps[c] = (struct mp_imgfmt_comp_desc){0};
    }

    // Many important formats have padding within an access word. For example
    // yuv420p10 has the upper 6 bit cleared to 0; P010 has the lower 6 bits
    // cleared to 0. Pixdesc cannot represent that these bits are 0. There are
    // other formats where padding is not guaranteed to be 0, but they are
    // described in the same way.
    // Apply a heuristic that is supposed to identify formats which use
    // guaranteed 0 padding. This could fail, but nobody said this pixdesc crap
    // is robust.
    for (int c = 0; c < pd->nb_components; c++) {
        struct mp_imgfmt_comp_desc *cd = &desc.comps[c];
        // Note: rgb444 would defeat our heuristic if we checked only per comp.
        //       also, exclude "bitstream" formats due to monow/monob
        int fsize = MP_ALIGN_UP(cd->size, 8);
        if (!any_shared_bytes && el_bits == 8 && fsize != cd->size &&
            fsize - cd->size <= (1 << 3))
        {
            if (!(cd->offset % 8u)) {
                cd->pad = -(fsize - cd->size);
                cd->size = fsize;
            } else if (!((cd->offset + cd->size) % 8u)) {
                cd->pad = fsize - cd->size;
                cd->size = fsize;
                cd->offset = MP_ALIGN_DOWN(cd->offset, 8);
            }
        }
    }

    if (is_packed_ss_yuv) {
        if (num_planes > 1)
            goto fail;
        // Guess at which positions the additional luma samples are. We iterate
        // starting with the first byte, and then put a luma sample at places
        // not covered by other luma/chroma.
        // Pixdesc does not and can not provide this information. This heuristic
        // may fail in certain cases. What a load of bullshit, right?
        int lsize = desc.comps[0].size;
        int cur_offset = 0;
        for (int lsample = 1; lsample < (1 << pd->log2_chroma_w); lsample++) {
            while (1) {
                if (cur_offset + lsize > desc.bits[0])
                    goto fail;
                bool free = true;
                for (int c = 0; c < pd->nb_components; c++) {
                    struct mp_imgfmt_comp_desc *cd = &desc.comps[c];
                    if (!cd->size)
                        continue;
                    if (cd->offset + cd->size > cur_offset &&
                        cur_offset + lsize > cd->offset)
                    {
                        free = false;
                        break;
                    }
                }
                if (free)
                    break;
                cur_offset += lsize;
            }
            desc.extra_luma_offsets[lsample - 1] = cur_offset;
            cur_offset += lsize;
        }
    }

    // The alpha component always has ID 4 (index 3) in our representation, so
    // move the alpha component to there.
    if (has_alpha && pd->nb_components < 4) {
        desc.comps[3] = desc.comps[pd->nb_components - 1];
        desc.comps[pd->nb_components - 1] = (struct mp_imgfmt_comp_desc){0};
    }

    *p_desc = desc;
    return;

fail:
    *p_desc = (struct mp_imgfmt_layout){{0}};
    // Average bit size fallback.
    int num_av_planes = av_pix_fmt_count_planes(fmt);
    for (int p = 0; p < num_av_planes; p++) {
        int ls = av_image_get_linesize(fmt, 256, p);
        if (ls > 0)
            p_desc->bits[p] = ls * 8 / 256;
    }
}

struct mp_imgfmt_desc mp_imgfmt_get_desc(int mpfmt)
{
    const struct mp_imgfmt_entry *mpdesc = get_mp_desc(mpfmt);
    if (mpdesc && mpdesc->desc.id)
        return mpdesc->desc;
    if (mpdesc && mpdesc->reg_desc.component_size)
        return to_legacy_desc(mpfmt, mpdesc->reg_desc);

    enum AVPixelFormat fmt = imgfmt2pixfmt(mpfmt);
    const AVPixFmtDescriptor *pd = av_pix_fmt_desc_get(fmt);
    if (!pd || pd->nb_components > 4)
        return (struct mp_imgfmt_desc) {0};
    bool is_uint = mp_imgfmt_get_component_type(mpfmt) == MP_COMPONENT_TYPE_UINT;

    struct mp_imgfmt_desc desc = {
        .id = mpfmt,
        .avformat = fmt,
        .chroma_xs = pd->log2_chroma_w,
        .chroma_ys = pd->log2_chroma_h,
    };

    for (int c = 0; c < pd->nb_components; c++)
        desc.num_planes = MPMAX(desc.num_planes, pd->comp[c].plane + 1);

    struct mp_imgfmt_layout layout;
    mp_imgfmt_get_layout(mpfmt, &layout);

    bool is_ba = desc.num_planes > 0;
    for (int p = 0; p < desc.num_planes; p++) {
        desc.bpp[p] = layout.bits[p] / (layout.extra_w + 1);
        is_ba = !(desc.bpp[p] % 8u);
    }

    if (is_ba)
        desc.flags |= MP_IMGFLAG_BYTE_ALIGNED;

    // Very heuristical.
    bool is_be = layout.endian_bytes > 0;
    bool need_endian = (layout.comps[0].size % 8u && layout.bits[0] > 8) ||
                       layout.comps[0].size > 8;

    if (need_endian) {
        desc.flags |= is_be ? MP_IMGFLAG_BE : MP_IMGFLAG_LE;
    } else {
        desc.flags |= MP_IMGFLAG_LE | MP_IMGFLAG_BE;
    }

    enum mp_csp csp = mp_imgfmt_get_forced_csp(mpfmt);

    if ((pd->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        desc.flags |= MP_IMGFLAG_HWACCEL;
    } else if (csp == MP_CSP_XYZ) {
        /* nothing */
    } else if (csp == MP_CSP_RGB) {
        desc.flags |= MP_IMGFLAG_RGB;
    } else {
        desc.flags |= MP_IMGFLAG_YUV;
    }

    if (pd->flags & AV_PIX_FMT_FLAG_ALPHA)
        desc.flags |= MP_IMGFLAG_ALPHA;

    if (pd->flags & AV_PIX_FMT_FLAG_PAL)
        desc.flags |= MP_IMGFLAG_PAL;

    if ((desc.flags & (MP_IMGFLAG_YUV | MP_IMGFLAG_RGB))
        && (desc.flags & MP_IMGFLAG_BYTE_ALIGNED)
        && !(pd->flags & AV_PIX_FMT_FLAG_PAL)
        && is_uint)
    {
        bool same_depth = true;
        for (int p = 0; p < desc.num_planes; p++) {
            same_depth &= layout.bits[p] == layout.bits[0] &&
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
            desc.bpp[1] == desc.bpp[0] * 2 &&
            (desc.flags & MP_IMGFLAG_YUV))
        {

            desc.flags |= MP_IMGFLAG_YUV_NV;
        }
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
        if ((fmt->chroma_xs > 0 || fmt->chroma_ys > 0) && chroma_luma == 3)
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
    const struct mp_imgfmt_entry *p = get_mp_desc(imgfmt);
    if (p && p->reg_desc.component_size)
        return p->reg_desc.forced_csp;
    if (p && p->forced_csp)
        return p->forced_csp;

    enum AVPixelFormat pixfmt = imgfmt2pixfmt(imgfmt);
    const AVPixFmtDescriptor *pixdesc = av_pix_fmt_desc_get(pixfmt);

    if (pixdesc && (pixdesc->flags & AV_PIX_FMT_FLAG_HWACCEL))
        return MP_CSP_AUTO;

    // FFmpeg does not provide a flag for XYZ, so this is the best we can do.
    if (pixdesc && strncmp(pixdesc->name, "xyz", 3) == 0)
        return MP_CSP_XYZ;

    if (pixdesc && (pixdesc->flags & AV_PIX_FMT_FLAG_RGB))
        return MP_CSP_RGB;

    if (pixfmt == AV_PIX_FMT_PAL8 ||
        pixfmt == AV_PIX_FMT_MONOBLACK ||
        pixfmt == AV_PIX_FMT_MONOWHITE)
        return MP_CSP_RGB;

    return MP_CSP_AUTO;
}

enum mp_component_type mp_imgfmt_get_component_type(int imgfmt)
{
    const struct mp_imgfmt_entry *p = get_mp_desc(imgfmt);
    if (p && p->reg_desc.component_size)
        return p->reg_desc.component_type;
    if (p && p->ctype)
        return p->ctype;

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

bool mp_get_regular_imgfmt(struct mp_regular_imgfmt *dst, int imgfmt)
{
    const struct mp_imgfmt_entry *p = get_mp_desc(imgfmt);
    if (p && p->reg_desc.component_size) {
        *dst = p->reg_desc;
        return true;
    }

    struct mp_regular_imgfmt res = {0};

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    if (!desc.num_planes)
        return false;
    res.num_planes = desc.num_planes;

    struct mp_imgfmt_layout layout;
    mp_imgfmt_get_layout(imgfmt, &layout);

    if (layout.endian_bytes || layout.extra_w)
        return false;

    res.component_type = mp_imgfmt_get_component_type(imgfmt);
    if (!res.component_type)
        return false;

    struct mp_imgfmt_comp_desc *comp0 = &layout.comps[0];
    if (comp0->size < 1 || comp0->size > 64 || (comp0->size % 8u))
        return false;

    res.component_size = comp0->size / 8u;
    res.component_pad = comp0->pad;

    for (int n = 0; n < res.num_planes; n++) {
        if (layout.bits[n] % comp0->size)
            return false;
        res.planes[n].num_components = layout.bits[n] / comp0->size;
    }

    for (int n = 0; n < MP_NUM_COMPONENTS; n++) {
        struct mp_imgfmt_comp_desc *comp = &layout.comps[n];
        if (!comp->size)
            continue;

        struct mp_regular_imgfmt_plane *plane = &res.planes[comp->plane];

        res.num_planes = MPMAX(res.num_planes, comp->plane + 1);

        // We support uniform depth only.
        if (comp->size != comp0->size || comp->pad != comp0->pad)
            return false;

        // Size-aligned only.
        int pos = comp->offset / comp->size;
        if (comp->offset != pos * comp->size || pos >= MP_NUM_COMPONENTS)
            return false;

        if (plane->components[pos])
            return false;
        plane->components[pos] = n + 1;
    }

    res.chroma_xs = desc.chroma_xs;
    res.chroma_ys = desc.chroma_ys;

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
        a->chroma_xs      != b->chroma_xs ||
        a->chroma_ys      != b->chroma_ys)
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
