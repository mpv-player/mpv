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
    // Valid if flags!=0.
    // This can be incomplete, and missing fields are filled in:
    //  - sets num_planes and bpp[], derived from comps[] (rounds to bytes)
    //  - sets MP_IMGFLAG_GRAY, derived from comps[]
    //  - sets MP_IMGFLAG_ALPHA, derived from comps[]
    //  - sets align_x/y if 0, derived from chroma shift
    //  - sets xs[]/ys[] always, derived from num_planes/chroma_shift
    //  - sets MP_IMGFLAG_HAS_COMPS|MP_IMGFLAG_NE if num_planes>0
    //  - sets MP_IMGFLAG_TYPE_UINT if no other type set
    //  - sets id to mp_imgfmt_list[] implied format
    struct mp_imgfmt_desc desc;
};

#define FRINGE_GBRP(def, dname, b)                                          \
    [def - IMGFMT_CUST_BASE] = {                                            \
        .name = dname,                                                      \
        .desc = { .flags = MP_IMGFLAG_COLOR_RGB,                            \
                  .comps = { {2, 0, 8, (b) - 8}, {0, 0, 8, (b) - 8},        \
                             {1, 0, 8, (b) - 8}, }, }}

#define FLOAT_YUV(def, dname, xs, ys, a)                                    \
    [def - IMGFMT_CUST_BASE] = {                                            \
        .name = dname,                                                      \
        .desc = { .flags = MP_IMGFLAG_COLOR_YUV | MP_IMGFLAG_TYPE_FLOAT,    \
                   .chroma_xs = xs, .chroma_ys = ys,                        \
                   .comps = { {0, 0, 32}, {1, 0, 32}, {2, 0, 32},           \
                              {3 * (a), 0, 32 * (a)} }, }}

static const struct mp_imgfmt_entry mp_imgfmt_list[] = {
    // not in ffmpeg
    [IMGFMT_VDPAU_OUTPUT - IMGFMT_CUST_BASE] = {
        .name = "vdpau_output",
        .desc = {
            .flags = MP_IMGFLAG_NE | MP_IMGFLAG_RGB | MP_IMGFLAG_HWACCEL,
        },
    },
    [IMGFMT_RGB30 - IMGFMT_CUST_BASE] = {
        .name = "rgb30",
        .desc = {
            .flags = MP_IMGFLAG_RGB,
            .comps = { {0, 20, 10}, {0, 10, 10}, {0, 0, 10} },
        },
    },
    [IMGFMT_YAP8 - IMGFMT_CUST_BASE] = {
        .name = "yap8",
        .desc = {
            .flags = MP_IMGFLAG_COLOR_YUV,
            .comps = { {0, 0, 8}, {0}, {0}, {1, 0, 8} },
        },
    },
    [IMGFMT_YAP16 - IMGFMT_CUST_BASE] = {
        .name = "yap16",
        .desc = {
            .flags = MP_IMGFLAG_COLOR_YUV,
            .comps = { {0, 0, 16}, {0}, {0}, {1, 0, 16} },
        },
    },
    [IMGFMT_Y1 - IMGFMT_CUST_BASE] = {
        .name = "y1",
        .desc = {
            .flags = MP_IMGFLAG_COLOR_RGB,
            .comps = { {0, 0, 8, -7} },
        },
    },
    [IMGFMT_YAPF - IMGFMT_CUST_BASE] = {
        .name = "grayaf32", // try to mimic ffmpeg naming convention
        .desc = {
            .flags = MP_IMGFLAG_COLOR_YUV | MP_IMGFLAG_TYPE_FLOAT,
            .comps = { {0, 0, 32}, {0}, {0}, {1, 0, 32} },
        },
    },
    FLOAT_YUV(IMGFMT_444PF,  "yuv444pf",  0, 0, 0),
    FLOAT_YUV(IMGFMT_444APF, "yuva444pf", 0, 0, 1),
    FLOAT_YUV(IMGFMT_420PF,  "yuv420pf",  1, 1, 0),
    FLOAT_YUV(IMGFMT_420APF, "yuva420pf", 1, 1, 1),
    FLOAT_YUV(IMGFMT_422PF,  "yuv422pf",  1, 0, 0),
    FLOAT_YUV(IMGFMT_422APF, "yuva422pf", 1, 0, 1),
    FLOAT_YUV(IMGFMT_440PF,  "yuv440pf",  0, 1, 0),
    FLOAT_YUV(IMGFMT_440APF, "yuva440pf", 0, 1, 1),
    FLOAT_YUV(IMGFMT_410PF,  "yuv410pf",  2, 2, 0),
    FLOAT_YUV(IMGFMT_410APF, "yuva410pf", 2, 2, 1),
    FLOAT_YUV(IMGFMT_411PF,  "yuv411pf",  2, 0, 0),
    FLOAT_YUV(IMGFMT_411APF, "yuva411pf", 2, 0, 1),
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

static void fill_pixdesc_layout(struct mp_imgfmt_desc *desc,
                                enum AVPixelFormat fmt,
                                const AVPixFmtDescriptor *pd)
{
    if (pd->flags & AV_PIX_FMT_FLAG_PAL ||
        pd->flags & AV_PIX_FMT_FLAG_HWACCEL)
        goto fail;

    bool has_alpha = pd->flags & AV_PIX_FMT_FLAG_ALPHA;
    if (pd->nb_components != 1 + has_alpha &&
        pd->nb_components != 3 + has_alpha)
        goto fail;

    // Very convenient: we assume we're always on little endian, and FFmpeg
    // explicitly marks big endian formats => don't need to guess whether a
    // format is little endian, or not affected by byte order.
    bool is_be = pd->flags & AV_PIX_FMT_FLAG_BE;
    bool is_ne = MP_SELECT_LE_BE(false, true) == is_be;

    // Packed sub-sampled YUV is very... special.
    bool is_packed_ss_yuv = pd->log2_chroma_w && !pd->log2_chroma_h &&
        pd->comp[1].plane == 0 && pd->comp[2].plane == 0 &&
        pd->nb_components == 3;

    if (is_packed_ss_yuv)
        desc->bpp[0] = pd->comp[1].step * 8;

    // Determine if there are any byte overlaps => relevant for determining
    // access unit for endian, since pixdesc does not expose this, and assumes
    // a weird model where you do separate memory fetches for each component.
    bool any_shared_bytes = !!(pd->flags & AV_PIX_FMT_FLAG_BITSTREAM);
    for (int c = 0; c < pd->nb_components; c++) {
        for (int i = 0; i < c; i++) {
            const AVComponentDescriptor *d1 = &pd->comp[c];
            const AVComponentDescriptor *d2 = &pd->comp[i];
            if (d1->plane == d2->plane) {
                if (d1->offset + (d1->depth + 7) / 8u > d2->offset &&
                    d2->offset + (d2->depth + 7) / 8u > d1->offset)
                    any_shared_bytes = true;
            }
        }
    }

    int el_bits = (pd->flags & AV_PIX_FMT_FLAG_BITSTREAM) ? 1 : 8;
    for (int c = 0; c < pd->nb_components; c++) {
        const AVComponentDescriptor *d = &pd->comp[c];
        if (d->plane >= MP_MAX_PLANES)
            goto fail;

        desc->num_planes = MPMAX(desc->num_planes, d->plane + 1);

        int plane_bits = desc->bpp[d->plane];
        int c_bits = d->step * el_bits;

        // The first component wins, because either all components result in
        // the same value, or luma wins (luma always comes before chroma).
        if (plane_bits) {
            if (c_bits > plane_bits)
                goto fail; // inconsistent
        } else {
            desc->bpp[d->plane] = plane_bits = c_bits;
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
        int word = mp_round_next_power_of_2(MPMAX(d->depth + shift, 8));
        // The purpose of this is unknown. It's an absurdity fished out of
        // av_read_image_line2()'s implementation. It seems technically
        // unnecessary, and provides no information. On the other hand, it
        // compensates for seemingly bogus packed integer pixdescs; this
        // is "why" some formats use d->offset = -1.
        if (is_be && el_bits == 8 && word == 8)
            offset += 8;
        // Pixdesc's model sometimes requires accesses with varying word-sizes,
        // as seen in bgr565 and other formats. Also, it makes you read some
        // formats with multiple endian-dependent accesses, where accessing a
        // larger unit would make more sense. (Consider X2RGB10BE, for which
        // pixdesc wants you to perform 3 * 2 byte accesses, and swap each of
        // the read 16 bit words. What you really want is to swap the entire 4
        // byte thing, and then extract the components with bit shifts).
        // This is complete bullshit, so we transform it into word swaps before
        // further processing. Care needs to be taken to not change formats like
        // P010 or YA16 (prefer component accesses for them; P010 isn't even
        // representable, because endian_shift is for all planes).
        // As a heuristic, assume that if any components share a byte, the whole
        // pixel is read as a single memory access and endian swapped at once.
        int access_size = 8;
        if (plane_bits > 8) {
            if (any_shared_bytes) {
                access_size = plane_bits;
                if (is_be && word != access_size) {
                    // Before: offset = 8*byte_offset (with word bits of data)
                    // After: offset = bit_offset into swapped endian_size word
                    offset = access_size - word - offset;
                }
            } else {
                access_size = word;
            }
        }
        int endian_size = (access_size && !is_ne) ? access_size : 8;
        int endian_shift = mp_log2(endian_size) - 3;
        if (!MP_IS_POWER_OF_2(endian_size) || endian_shift < 0 || endian_shift > 3)
            goto fail;
        if (desc->endian_shift && desc->endian_shift != endian_shift)
            goto fail;
        desc->endian_shift = endian_shift;

        // We always use bit offsets; this doesn't lose any information,
        // and pixdesc is merely more redundant.
        offset += shift;
        if (offset < 0 || offset >= (1 << 6))
            goto fail;
        if (offset + d->depth > plane_bits)
            goto fail;
        if (d->depth < 0 || d->depth >= (1 << 6))
            goto fail;
        desc->comps[c] = (struct mp_imgfmt_comp_desc){
            .plane = d->plane,
            .offset = offset,
            .size = d->depth,
        };
    }

    for (int p = 0; p < desc->num_planes; p++) {
        if (!desc->bpp[p])
            goto fail; // plane doesn't exist
    }

    // What the fuck: this is probably a pixdesc bug, so fix it.
    if (fmt == AV_PIX_FMT_RGB8) {
        desc->comps[2] = (struct mp_imgfmt_comp_desc){0, 0, 2};
        desc->comps[1] = (struct mp_imgfmt_comp_desc){0, 2, 3};
        desc->comps[0] = (struct mp_imgfmt_comp_desc){0, 5, 3};
    }

    // Overlap test. If any shared bits are happening, this is not a format we
    // can represent (or it's something like Bayer: components in the same bits,
    // but different alternating lines).
    bool any_shared_bits = false;
    for (int c = 0; c < pd->nb_components; c++) {
        for (int i = 0; i < c; i++) {
            struct mp_imgfmt_comp_desc *c1 = &desc->comps[c];
            struct mp_imgfmt_comp_desc *c2 = &desc->comps[i];
            if (c1->plane == c2->plane) {
                if (c1->offset + c1->size > c2->offset &&
                    c2->offset + c2->size > c1->offset)
                    any_shared_bits = true;
            }
        }
    }

    if (any_shared_bits) {
        for (int c = 0; c < pd->nb_components; c++)
            desc->comps[c] = (struct mp_imgfmt_comp_desc){0};
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
        struct mp_imgfmt_comp_desc *cd = &desc->comps[c];
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

    // The alpha component always has ID 4 (index 3) in our representation, so
    // move the alpha component to there.
    if (has_alpha && pd->nb_components < 4) {
        desc->comps[3] = desc->comps[pd->nb_components - 1];
        desc->comps[pd->nb_components - 1] = (struct mp_imgfmt_comp_desc){0};
    }

    if (is_packed_ss_yuv) {
        desc->flags |= MP_IMGFLAG_PACKED_SS_YUV;
        desc->bpp[0] /= 1 << pd->log2_chroma_w;
    } else if (!any_shared_bits) {
        desc->flags |= MP_IMGFLAG_HAS_COMPS;
    }

    return;

fail:
    for (int n = 0; n < 4; n++)
        desc->comps[n] = (struct mp_imgfmt_comp_desc){0};
    // Average bit size fallback.
    desc->num_planes = av_pix_fmt_count_planes(fmt);
    for (int p = 0; p < desc->num_planes; p++) {
        int ls = av_image_get_linesize(fmt, 256, p);
        desc->bpp[p] = ls > 0 ? ls * 8 / 256 : 0;
    }
}

static bool mp_imgfmt_get_desc_from_pixdesc(int mpfmt, struct mp_imgfmt_desc *out)
{
    enum AVPixelFormat fmt = imgfmt2pixfmt(mpfmt);
    const AVPixFmtDescriptor *pd = av_pix_fmt_desc_get(fmt);
    if (!pd || pd->nb_components > 4)
        return false;

    struct mp_imgfmt_desc desc = {
        .id = mpfmt,
        .chroma_xs = pd->log2_chroma_w,
        .chroma_ys = pd->log2_chroma_h,
    };

    if (pd->flags & AV_PIX_FMT_FLAG_ALPHA)
        desc.flags |= MP_IMGFLAG_ALPHA;

    if (pd->flags & AV_PIX_FMT_FLAG_HWACCEL)
        desc.flags |= MP_IMGFLAG_TYPE_HW;

    // Pixdesc does not provide a flag for XYZ, so this is the best we can do.
    if (strncmp(pd->name, "xyz", 3) == 0) {
        desc.flags |= MP_IMGFLAG_COLOR_XYZ;
    } else if (pd->flags & AV_PIX_FMT_FLAG_RGB) {
        desc.flags |= MP_IMGFLAG_COLOR_RGB;
    } else if (fmt == AV_PIX_FMT_MONOBLACK || fmt == AV_PIX_FMT_MONOWHITE) {
        desc.flags |= MP_IMGFLAG_COLOR_RGB;
    } else if (fmt == AV_PIX_FMT_PAL8) {
        desc.flags |= MP_IMGFLAG_COLOR_RGB | MP_IMGFLAG_TYPE_PAL8;
    }

    if (pd->flags & AV_PIX_FMT_FLAG_FLOAT)
        desc.flags |= MP_IMGFLAG_TYPE_FLOAT;

    // Educated guess.
    if (!(desc.flags & MP_IMGFLAG_COLOR_MASK) &&
        !(desc.flags & MP_IMGFLAG_TYPE_HW))
        desc.flags |= MP_IMGFLAG_COLOR_YUV;

    desc.align_x = 1 << desc.chroma_xs;
    desc.align_y = 1 << desc.chroma_ys;

    fill_pixdesc_layout(&desc, fmt, pd);

    if (desc.flags & (MP_IMGFLAG_HAS_COMPS | MP_IMGFLAG_PACKED_SS_YUV)) {
        if (!(desc.flags & MP_IMGFLAG_TYPE_MASK))
            desc.flags |= MP_IMGFLAG_TYPE_UINT;
    }

    if (desc.bpp[0] % 8u && (pd->flags & AV_PIX_FMT_FLAG_BITSTREAM))
        desc.align_x = 8 / desc.bpp[0]; // expect power of 2

    // Very heuristical.
    bool is_ne = !desc.endian_shift;
    bool need_endian = (desc.comps[0].size % 8u && desc.bpp[0] > 8) ||
                       desc.comps[0].size > 8;

    if (need_endian) {
        bool is_le = MP_SELECT_LE_BE(is_ne, !is_ne);
        desc.flags |= is_le ? MP_IMGFLAG_LE : MP_IMGFLAG_BE;
    } else {
        desc.flags |= MP_IMGFLAG_LE | MP_IMGFLAG_BE;
    }

    *out = desc;
    return true;
}

bool mp_imgfmt_get_packed_yuv_locations(int imgfmt, uint8_t *luma_offsets)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    if (!(desc.flags & MP_IMGFLAG_PACKED_SS_YUV))
        return false;

    assert(desc.num_planes == 1);

    // Guess at which positions the additional luma samples are. We iterate
    // starting with the first byte, and then put a luma sample at places
    // not covered by other luma/chroma.
    // Pixdesc does not and can not provide this information. This heuristic
    // may fail in certain cases. What a load of bullshit, right?
    int lsize = desc.comps[0].size;
    int cur_offset = 0;
    for (int lsample = 1; lsample < (1 << desc.chroma_xs); lsample++) {
        while (1) {
            if (cur_offset + lsize > desc.bpp[0] * desc.align_x)
                return false;
            bool free = true;
            for (int c = 0; c < 3; c++) {
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
        luma_offsets[lsample] = cur_offset;
        cur_offset += lsize;
    }

    luma_offsets[0] = desc.comps[0].offset;
    return true;
}

static bool get_native_desc(int mpfmt, struct mp_imgfmt_desc *desc)
{
    const struct mp_imgfmt_entry *p = get_mp_desc(mpfmt);
    if (!p || !p->desc.flags)
        return false;

    *desc = p->desc;

    // Fill in some fields mp_imgfmt_entry.desc is not required to set.

    desc->id = mpfmt;

    for (int n = 0; n < MP_NUM_COMPONENTS; n++) {
        struct mp_imgfmt_comp_desc *cd = &desc->comps[n];
        if (cd->size)
            desc->num_planes = MPMAX(desc->num_planes, cd->plane + 1);
        desc->bpp[cd->plane] =
            MPMAX(desc->bpp[cd->plane], MP_ALIGN_UP(cd->offset + cd->size, 8));
    }

    if (!desc->align_x && !desc->align_y) {
        desc->align_x = 1 << desc->chroma_xs;
        desc->align_y = 1 << desc->chroma_ys;
    }

    if (desc->num_planes)
        desc->flags |= MP_IMGFLAG_HAS_COMPS | MP_IMGFLAG_NE;

    if (!(desc->flags & MP_IMGFLAG_TYPE_MASK))
        desc->flags |= MP_IMGFLAG_TYPE_UINT;

    return true;
}

int mp_imgfmt_desc_get_num_comps(struct mp_imgfmt_desc *desc)
{
    int flags = desc->flags;
    if (!(flags & MP_IMGFLAG_COLOR_MASK))
        return 0;
    return 3 + (flags & MP_IMGFLAG_GRAY ? -2 : 0) + !!(flags & MP_IMGFLAG_ALPHA);
}

struct mp_imgfmt_desc mp_imgfmt_get_desc(int mpfmt)
{
    struct mp_imgfmt_desc desc;

    if (!get_native_desc(mpfmt, &desc) &&
        !mp_imgfmt_get_desc_from_pixdesc(mpfmt, &desc))
        return (struct mp_imgfmt_desc){0};

    for (int p = 0; p < desc.num_planes; p++) {
        desc.xs[p] = (p == 1 || p == 2) ? desc.chroma_xs : 0;
        desc.ys[p] = (p == 1 || p == 2) ? desc.chroma_ys : 0;
    }

    bool is_ba = desc.num_planes > 0;
    for (int p = 0; p < desc.num_planes; p++)
        is_ba = !(desc.bpp[p] % 8u);

    if (is_ba)
        desc.flags |= MP_IMGFLAG_BYTE_ALIGNED;

    if (desc.flags & MP_IMGFLAG_HAS_COMPS) {
        if (desc.comps[3].size)
            desc.flags |= MP_IMGFLAG_ALPHA;

        // Assuming all colors are (CCC+[A]) or (C+[A]), the latter being gray.
        if (!desc.comps[1].size)
            desc.flags |= MP_IMGFLAG_GRAY;

        bool bb = true;
        for (int n = 0; n < MP_NUM_COMPONENTS; n++) {
            if (desc.comps[n].offset % 8u || desc.comps[n].size % 8u)
                bb = false;
        }
        if (bb)
            desc.flags |= MP_IMGFLAG_BYTES;
    }

    if ((desc.flags & (MP_IMGFLAG_YUV | MP_IMGFLAG_RGB))
        && (desc.flags & MP_IMGFLAG_HAS_COMPS)
        && (desc.flags & MP_IMGFLAG_BYTES)
        && ((desc.flags & MP_IMGFLAG_TYPE_MASK) == MP_IMGFLAG_TYPE_UINT))
    {
        int cnt = mp_imgfmt_desc_get_num_comps(&desc);
        bool same_depth = true;
        for (int p = 0; p < desc.num_planes; p++)
            same_depth &= desc.bpp[p] == desc.bpp[0];
        if (same_depth && cnt == desc.num_planes) {
            if (desc.flags & MP_IMGFLAG_YUV) {
                desc.flags |= MP_IMGFLAG_YUV_P;
            } else {
                desc.flags |= MP_IMGFLAG_RGB_P;
            }
        }
        if (cnt == 3 && desc.num_planes == 2 &&
            desc.bpp[1] == desc.bpp[0] * 2 &&
            (desc.flags & MP_IMGFLAG_YUV))
        {

            desc.flags |= MP_IMGFLAG_YUV_NV;
        }
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
        if ((fmt->chroma_xs > 0 || fmt->chroma_ys > 0) && chroma_luma == 3)
            return false; // separate chroma/luma planes required
    }

    if (!(present[0] || present[3]) ||  // at least component 1 or alpha needed
        (present[1] && !present[0]) ||  // component 2 requires component 1
        (present[2] && !present[1]))    // component 3 requires component 2
        return false;

    return true;
}

static enum mp_csp get_forced_csp_from_flags(int flags)
{
    if (flags & MP_IMGFLAG_COLOR_XYZ)
        return MP_CSP_XYZ;

    if (flags & MP_IMGFLAG_COLOR_RGB)
        return MP_CSP_RGB;

    return MP_CSP_AUTO;
}

enum mp_csp mp_imgfmt_get_forced_csp(int imgfmt)
{
    return get_forced_csp_from_flags(mp_imgfmt_get_desc(imgfmt).flags);
}

static enum mp_component_type get_component_type_from_flags(int flags)
{
    if (flags & MP_IMGFLAG_TYPE_UINT)
        return MP_COMPONENT_TYPE_UINT;

    if (flags & MP_IMGFLAG_TYPE_FLOAT)
        return MP_COMPONENT_TYPE_FLOAT;

    return MP_COMPONENT_TYPE_UNKNOWN;
}

enum mp_component_type mp_imgfmt_get_component_type(int imgfmt)
{
    return get_component_type_from_flags(mp_imgfmt_get_desc(imgfmt).flags);
}

int mp_find_other_endian(int imgfmt)
{
    return pixfmt2imgfmt(av_pix_fmt_swap_endianness(imgfmt2pixfmt(imgfmt)));
}

bool mp_get_regular_imgfmt(struct mp_regular_imgfmt *dst, int imgfmt)
{
    struct mp_regular_imgfmt res = {0};

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    if (!desc.num_planes)
        return false;
    res.num_planes = desc.num_planes;

    if (desc.endian_shift || !(desc.flags & MP_IMGFLAG_HAS_COMPS))
        return false;

    res.component_type = get_component_type_from_flags(desc.flags);
    if (!res.component_type)
        return false;

    struct mp_imgfmt_comp_desc *comp0 = &desc.comps[0];
    if (comp0->size < 1 || comp0->size > 64 || (comp0->size % 8u))
        return false;

    res.component_size = comp0->size / 8u;
    res.component_pad = comp0->pad;

    for (int n = 0; n < res.num_planes; n++) {
        if (desc.bpp[n] % comp0->size)
            return false;
        res.planes[n].num_components = desc.bpp[n] / comp0->size;
    }

    for (int n = 0; n < MP_NUM_COMPONENTS; n++) {
        struct mp_imgfmt_comp_desc *comp = &desc.comps[n];
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

    res.forced_csp = get_forced_csp_from_flags(desc.flags);

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
