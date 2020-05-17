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

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <inttypes.h>

#include "common/common.h"
#include "draw_bmp.h"
#include "img_convert.h"
#include "video/mp_image.h"
#include "video/repack.h"
#include "video/sws_utils.h"
#include "video/img_format.h"
#include "video/csputils.h"

const bool mp_draw_sub_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = true,
    [SUBBITMAP_RGBA] = true,
};

struct part {
    int change_id;
    // Sub-bitmaps scaled to final sizes.
    int num_imgs;
    struct mp_image **imgs;
};

// Must be a power of 2. Height is 1, but mark_rect() effectively operates on
// multiples of chroma sized macro-pixels. (E.g. 4:2:0 -> every second line is
// the same as the previous one, and x0%2==x1%2==0.)
#define SLICE_W 256u

// Whether to scale in tiles. Faster, but can't use correct chroma position.
// Should be a runtime option. SLICE_W is used as tile width. The tile size
// should probably be small; too small or too big will cause overhead when
// scaling.
#define SCALE_IN_TILES 1
#define TILE_H 4u

struct slice {
    uint16_t x0, x1;
};

struct mp_draw_sub_cache
{
    struct mpv_global *global;

    // Possibly cached parts. Also implies what's in the video_overlay.
    struct part parts[MAX_OSD_PARTS];
    int64_t change_id;

    struct mp_image_params params;  // target image params

    int w, h;                       // like params.w/h, but rounded up to chroma
    unsigned align_x, align_y;      // alignment for all video pixels

    struct mp_image *rgba_overlay;  // all OSD in RGBA
    struct mp_image *video_overlay; // rgba_overlay converted to video colorspace
    struct mp_image *alpha_overlay; // alpha plane ref. to video_overlay
    struct mp_image *calpha_overlay; // alpha_overlay scaled to chroma plane size

    unsigned s_w;                   // number of slices per line
    struct slice *slices;           // slices[y * s_w + x / SLICE_W]
    bool any_osd;

    struct mp_sws_context *rgba_to_overlay; // scaler for rgba -> video csp.
    struct mp_sws_context *alpha_to_calpha; // scaler for overlay -> calpha
    bool scale_in_tiles;

    struct mp_sws_context *sub_scale; // scaler for SUBBITMAP_RGBA

    struct mp_repack *overlay_to_f32; // convert video_overlay to float
    struct mp_image *overlay_tmp;   // slice in float32

    struct mp_repack *calpha_to_f32; // convert video_overlay to float
    struct mp_image *calpha_tmp;    // slice in float32

    struct mp_repack *video_to_f32; // convert video to float
    struct mp_repack *video_from_f32; // convert float back to video
    struct mp_image *video_tmp;     // slice in float32

    struct mp_sws_context *premul;  // video -> premultiplied video
    struct mp_sws_context *unpremul; // reverse
    struct mp_image *premul_tmp;

    // Function that works on the _f32 data.
    void (*blend_line)(void *dst, void *src, void *src_a, int w);

    struct mp_image res_overlay;    // returned by mp_draw_sub_overlay()
};

static void blend_line_f32(void *dst, void *src, void *src_a, int w)
{
    float *dst_f = dst;
    float *src_f = src;
    float *src_a_f = src_a;

    for (int x = 0; x < w; x++)
        dst_f[x] = src_f[x] + dst_f[x] * (1.0f - src_a_f[x]);
}

static void blend_line_u8(void *dst, void *src, void *src_a, int w)
{
    uint8_t *dst_i = dst;
    uint8_t *src_i = src;
    uint8_t *src_a_i = src_a;

    for (int x = 0; x < w; x++)
        dst_i[x] = src_i[x] + dst_i[x] * (255u - src_a_i[x]) / 255u;
}

static void blend_slice(struct mp_draw_sub_cache *p)
{
    struct mp_image *ov = p->overlay_tmp;
    struct mp_image *ca = p->calpha_tmp;
    struct mp_image *vid = p->video_tmp;

    for (int plane = 0; plane < vid->num_planes; plane++) {
        int xs = vid->fmt.xs[plane];
        int ys = vid->fmt.ys[plane];
        int h = (1 << vid->fmt.chroma_ys) - (1 << ys) + 1;
        int cw = mp_chroma_div_up(vid->w, xs);
        for (int y = 0; y < h; y++) {
            p->blend_line(mp_image_pixel_ptr_ny(vid, plane, 0, y),
                          mp_image_pixel_ptr_ny(ov, plane, 0, y),
                          xs || ys ? mp_image_pixel_ptr_ny(ca, 0, 0, y)
                            : mp_image_pixel_ptr_ny(ov, ov->num_planes - 1, 0, y),
                          cw);
        }
    }
}

static bool blend_overlay_with_video(struct mp_draw_sub_cache *p,
                                     struct mp_image *dst)
{
    if (!repack_config_buffers(p->video_to_f32, 0, p->video_tmp, 0, dst, NULL))
        return false;
    if (!repack_config_buffers(p->video_from_f32, 0, dst, 0, p->video_tmp, NULL))
        return false;

    int xs = dst->fmt.chroma_xs;
    int ys = dst->fmt.chroma_ys;

    for (int y = 0; y < dst->h; y += p->align_y) {
        struct slice *line = &p->slices[y * p->s_w];

        for (int sx = 0; sx < p->s_w; sx++) {
            struct slice *s = &line[sx];

            int w = s->x1 - s->x0;
            if (w <= 0)
                continue;
            int x = sx * SLICE_W + s->x0;

            assert(MP_IS_ALIGNED(x, p->align_x));
            assert(MP_IS_ALIGNED(w, p->align_x));
            assert(x + w <= p->w);

            repack_line(p->overlay_to_f32, 0, 0, x, y, w);
            repack_line(p->video_to_f32, 0, 0, x, y, w);
            if (p->calpha_to_f32)
                repack_line(p->calpha_to_f32, 0, 0, x >> xs, y >> ys, w >> xs);

            blend_slice(p);

            repack_line(p->video_from_f32, x, y, 0, 0, w);
        }
    }

    return true;
}

static bool convert_overlay_part(struct mp_draw_sub_cache *p,
                                 int x0, int y0, int w, int h)
{
    struct mp_image src = *p->rgba_overlay;
    struct mp_image dst = *p->video_overlay;

    mp_image_crop(&src, x0, y0, x0 + w, y0 + h);
    mp_image_crop(&dst, x0, y0, x0 + w, y0 + h);

    if (mp_sws_scale(p->rgba_to_overlay, &dst, &src) < 0)
        return false;

    if (p->calpha_overlay) {
        src = *p->alpha_overlay;
        dst = *p->calpha_overlay;

        int xs = p->video_overlay->fmt.chroma_xs;
        int ys = p->video_overlay->fmt.chroma_ys;
        mp_image_crop(&src, x0, y0, x0 + w, y0 + h);
        mp_image_crop(&dst, x0 >> xs, y0 >> ys, (x0 + w) >> xs, (y0 + h) >> ys);

        if (mp_sws_scale(p->alpha_to_calpha, &dst, &src) < 0)
            return false;
    }

    return true;
}

static bool convert_to_video_overlay(struct mp_draw_sub_cache *p)
{
    if (!p->video_overlay)
        return true;

    if (p->scale_in_tiles) {
        int t_h = p->rgba_overlay->h / TILE_H;
        for (int ty = 0; ty < t_h; ty++) {
            for (int sx = 0; sx < p->s_w; sx++) {
                struct slice *s = &p->slices[ty * TILE_H * p->s_w + sx];
                bool pixels_set = false;
                for (int y = 0; y < TILE_H; y++) {
                    if (s[0].x0 < s[0].x1) {
                        pixels_set = true;
                        break;
                    }
                    s += p->s_w;
                }
                if (!pixels_set)
                    continue;
                if (!convert_overlay_part(p, sx * SLICE_W, ty * TILE_H,
                                          SLICE_W, TILE_H))
                    return false;
            }
        }
    } else {
        if (!convert_overlay_part(p, 0, 0, p->rgba_overlay->w, p->rgba_overlay->h))
            return false;
    }

    return true;
}

// Mark the given rectangle of pixels as possibly non-transparent.
// The rectangle must have been pre-clipped.
static void mark_rect(struct mp_draw_sub_cache *p, int x0, int y0, int x1, int y1)
{
    x0 = MP_ALIGN_DOWN(x0, p->align_x);
    y0 = MP_ALIGN_DOWN(y0, p->align_y);
    x1 = MP_ALIGN_UP(x1, p->align_x);
    y1 = MP_ALIGN_UP(y1, p->align_y);

    assert(x0 >= 0 && x0 <= x1 && x1 <= p->w);
    assert(y0 >= 0 && y0 <= y1 && y1 <= p->h);

    int sx0 = x0 / SLICE_W;
    int sx1 = x1 / SLICE_W;

    for (int y = y0; y < y1; y++) {
        struct slice *line = &p->slices[y * p->s_w];

        struct slice *s0 = &line[sx0];
        struct slice *s1 = &line[sx1];

        s0->x0 = MPMIN(s0->x0, x0 % SLICE_W);
        s1->x1 = MPMAX(s1->x1, x1 % SLICE_W);

        if (s0 != s1) {
            s0->x1 = SLICE_W;
            s1->x0 = 0;

            for (int x = sx0 + 1; x < sx1; x++) {
                struct slice *s = &line[x];
                s->x0 = 0;
                s->x1 = SLICE_W;
            }
        }

        p->any_osd = true;
    }
}

static void draw_ass_rgba(uint8_t *dst, ptrdiff_t dst_stride,
                          uint8_t *src, ptrdiff_t src_stride,
                          int w, int h, uint32_t color)
{
    const unsigned int r = (color >> 24) & 0xff;
    const unsigned int g = (color >> 16) & 0xff;
    const unsigned int b = (color >>  8) & 0xff;
    const unsigned int a = 0xff - (color & 0xff);

    for (int y = 0; y < h; y++) {
        uint32_t *dstrow = (uint32_t *) dst;
        for (int x = 0; x < w; x++) {
            const unsigned int v = src[x];
            unsigned int aa = a * v;
            uint32_t dstpix = dstrow[x];
            unsigned int dstb =  dstpix        & 0xFF;
            unsigned int dstg = (dstpix >>  8) & 0xFF;
            unsigned int dstr = (dstpix >> 16) & 0xFF;
            unsigned int dsta = (dstpix >> 24) & 0xFF;
            dstb = (v * b * a   + dstb * (255 * 255 - aa)) / (255 * 255);
            dstg = (v * g * a   + dstg * (255 * 255 - aa)) / (255 * 255);
            dstr = (v * r * a   + dstr * (255 * 255 - aa)) / (255 * 255);
            dsta = (aa * 255    + dsta * (255 * 255 - aa)) / (255 * 255);
            dstrow[x] = dstb | (dstg << 8) | (dstr << 16) | (dsta << 24);
        }
        dst += dst_stride;
        src += src_stride;
    }
}

static void render_ass(struct mp_draw_sub_cache *p, struct sub_bitmaps *sb)
{
    assert(sb->format == SUBBITMAP_LIBASS);

    for (int i = 0; i < sb->num_parts; i++) {
        struct sub_bitmap *s = &sb->parts[i];

        draw_ass_rgba(mp_image_pixel_ptr(p->rgba_overlay, 0, s->x, s->y),
                      p->rgba_overlay->stride[0], s->bitmap, s->stride,
                      s->w, s->h, s->libass.color);

        mark_rect(p, s->x, s->y, s->x + s->w, s->y + s->h);
    }
}

static void draw_rgba(uint8_t *dst, ptrdiff_t dst_stride,
                      uint8_t *src, ptrdiff_t src_stride, int w, int h)
{
    for (int y = 0; y < h; y++) {
        uint32_t *srcrow = (uint32_t *)src;
        uint32_t *dstrow = (uint32_t *)dst;
        for (int x = 0; x < w; x++) {
            uint32_t srcpix = srcrow[x];
            uint32_t dstpix = dstrow[x];
            unsigned int srcb =  srcpix        & 0xFF;
            unsigned int srcg = (srcpix >>  8) & 0xFF;
            unsigned int srcr = (srcpix >> 16) & 0xFF;
            unsigned int srca = (srcpix >> 24) & 0xFF;
            unsigned int dstb =  dstpix        & 0xFF;
            unsigned int dstg = (dstpix >>  8) & 0xFF;
            unsigned int dstr = (dstpix >> 16) & 0xFF;
            unsigned int dsta = (dstpix >> 24) & 0xFF;
            dstb = srcb + dstb * (255 * 255 - srca) / (255 * 255);
            dstg = srcg + dstg * (255 * 255 - srca) / (255 * 255);
            dstr = srcr + dstr * (255 * 255 - srca) / (255 * 255);
            dsta = srca + dsta * (255 * 255 - srca) / (255 * 255);
            dstrow[x] = dstb | (dstg << 8) | (dstr << 16) | (dsta << 24);
        }
        dst += dst_stride;
        src += src_stride;
    }
}

static bool render_rgba(struct mp_draw_sub_cache *p, struct part *part,
                        struct sub_bitmaps *sb)
{
    assert(sb->format == SUBBITMAP_RGBA);

    if (part->change_id != sb->change_id) {
        for (int n = 0; n < part->num_imgs; n++)
            talloc_free(part->imgs[n]);
        part->num_imgs = sb->num_parts;
        MP_TARRAY_GROW(p, part->imgs, part->num_imgs);
        for (int n = 0; n < part->num_imgs; n++)
            part->imgs[n] = NULL;

        part->change_id = sb->change_id;
    }

    for (int i = 0; i < sb->num_parts; i++) {
        struct sub_bitmap *s = &sb->parts[i];

        // Clipping is rare but necessary.
        int sx0 = s->x;
        int sy0 = s->y;
        int sx1 = s->x + s->dw;
        int sy1 = s->y + s->dh;

        int x0 = MPCLAMP(sx0, 0, p->w);
        int y0 = MPCLAMP(sy0, 0, p->h);
        int x1 = MPCLAMP(sx1, 0, p->w);
        int y1 = MPCLAMP(sy1, 0, p->h);

        int dw = x1 - x0;
        int dh = y1 - y0;
        if (dw <= 0 || dh <= 0)
            continue;

        // We clip the source instead of the scaled image, because that might
        // avoid excessive memory usage when applying a ridiculous scale factor,
        // even if that stretches it to up to 1 pixel due to integer rounding.
        int sx = 0;
        int sy = 0;
        int sw = s->w;
        int sh = s->h;
        if (x0 != sx0 || y0 != sy0 || x1 != sx1 || y1 != sy1) {
            double fx = s->dw / (double)s->w;
            double fy = s->dh / (double)s->h;
            sx = MPCLAMP((x0 - sx0) / fx, 0, s->w);
            sy = MPCLAMP((y0 - sy0) / fy, 0, s->h);
            sw = MPCLAMP(dw / fx, 1, s->w);
            sh = MPCLAMP(dh / fy, 1, s->h);
        }

        assert(sx >= 0 && sw > 0 && sx + sw <= s->w);
        assert(sy >= 0 && sh > 0 && sy + sh <= s->h);

        ptrdiff_t s_stride = s->stride;
        void *s_ptr = (char *)s->bitmap + s_stride * sy + sx * 4;

        if (dw != sw || dh != sh) {
            struct mp_image *scaled = part->imgs[i];

            if (!scaled) {
                struct mp_image src_img = {0};
                mp_image_setfmt(&src_img, IMGFMT_BGRA);
                mp_image_set_size(&src_img, sw, sh);
                src_img.planes[0] = s_ptr;
                src_img.stride[0] = s_stride;
                src_img.params.alpha = MP_ALPHA_PREMUL;

                scaled = mp_image_alloc(IMGFMT_BGRA, dw, dh);
                if (!scaled)
                    return false;
                part->imgs[i] = talloc_steal(p, scaled);
                mp_image_copy_attributes(scaled, &src_img);

                if (mp_sws_scale(p->sub_scale, scaled, &src_img) < 0)
                    return false;
            }

            assert(scaled->w == dw);
            assert(scaled->h == dh);

            s_stride = scaled->stride[0];
            s_ptr = scaled->planes[0];
        }

        draw_rgba(mp_image_pixel_ptr(p->rgba_overlay, 0, x0, y0),
                  p->rgba_overlay->stride[0], s_ptr, s_stride, dw, dh);

        mark_rect(p, x0, y0, x1, y1);
    }

    return true;
}

static bool render_sb(struct mp_draw_sub_cache *p, struct sub_bitmaps *sb)
{
    struct part *part = &p->parts[sb->render_index];

    switch (sb->format) {
    case SUBBITMAP_LIBASS:
        render_ass(p, sb);
        return true;
    case SUBBITMAP_RGBA:
        return render_rgba(p, part, sb);
    }

    return false;
}

static void clear_rgba_overlay(struct mp_draw_sub_cache *p)
{
    assert(p->rgba_overlay->imgfmt == IMGFMT_BGRA);

    for (int y = 0; y < p->rgba_overlay->h; y++) {
        uint32_t *px = mp_image_pixel_ptr(p->rgba_overlay, 0, 0, y);
        struct slice *line = &p->slices[y * p->s_w];

        for (int sx = 0; sx < p->s_w; sx++) {
            struct slice *s = &line[sx];

            if (s->x0 <= s->x1) {
                memset(px + s->x0, 0, (s->x1 - s->x0) * 4);
                *s = (struct slice){SLICE_W, 0};
            }

            px += SLICE_W;
        }
    }

    p->any_osd = false;
}

static struct mp_sws_context *alloc_scaler(struct mp_draw_sub_cache *p)
{
    struct mp_sws_context *s = mp_sws_alloc(p);
    mp_sws_enable_cmdline_opts(s, p->global);
    return s;
}

static void init_general(struct mp_draw_sub_cache *p)
{
    p->sub_scale = alloc_scaler(p);

    p->s_w = MP_ALIGN_UP(p->rgba_overlay->w, SLICE_W) / SLICE_W;

    p->slices = talloc_zero_array(p, struct slice, p->s_w * p->rgba_overlay->h);

    mp_image_clear(p->rgba_overlay, 0, 0, p->w, p->h);
    clear_rgba_overlay(p);
}

static bool reinit_to_video(struct mp_draw_sub_cache *p)
{
    struct mp_image_params *params = &p->params;
    mp_image_params_guess_csp(params);

    bool need_premul = params->alpha != MP_ALPHA_PREMUL &&
        (mp_imgfmt_get_desc(params->imgfmt).flags & MP_IMGFLAG_ALPHA);

    // Intermediate format for video_overlay. Requirements:
    //  - same subsampling as video
    //  - uses video colorspace
    //  - has alpha
    //  - repacker support (to the format used in p->blend_line)
    //  - probably 8 bit per component rather than something wasteful or strange
    struct mp_regular_imgfmt vfdesc = {0};

    int rflags = REPACK_CREATE_EXPAND_8BIT;
    bool use_shortcut = false;

    p->video_to_f32 = mp_repack_create_planar(params->imgfmt, false, rflags);
    talloc_steal(p, p->video_to_f32);
    if (!p->video_to_f32)
        return false;
    mp_get_regular_imgfmt(&vfdesc, mp_repack_get_format_dst(p->video_to_f32));
    assert(vfdesc.num_planes); // must have succeeded

    if (params->color.space == MP_CSP_RGB && vfdesc.num_planes >= 3) {
        use_shortcut = true;

        if (vfdesc.component_type == MP_COMPONENT_TYPE_UINT &&
            vfdesc.component_size == 1 && vfdesc.component_pad == 0)
            p->blend_line = blend_line_u8;
    }

    // If no special blender is available, blend in float.
    if (!p->blend_line) {
        TA_FREEP(&p->video_to_f32);

        rflags |= REPACK_CREATE_PLANAR_F32;

        p->video_to_f32 = mp_repack_create_planar(params->imgfmt, false, rflags);
        talloc_steal(p, p->video_to_f32);
        if (!p->video_to_f32)
            return false;

        mp_get_regular_imgfmt(&vfdesc, mp_repack_get_format_dst(p->video_to_f32));
        assert(vfdesc.component_type == MP_COMPONENT_TYPE_FLOAT);

        p->blend_line = blend_line_f32;
    }

    p->scale_in_tiles = SCALE_IN_TILES;

    int vid_f32_fmt = mp_repack_get_format_dst(p->video_to_f32);

    p->video_from_f32 = mp_repack_create_planar(params->imgfmt, true, rflags);
    talloc_steal(p, p->video_from_f32);
    if (!p->video_from_f32)
        return false;

    assert(mp_repack_get_format_dst(p->video_to_f32) ==
           mp_repack_get_format_src(p->video_from_f32));

    int overlay_fmt = 0;
    if (use_shortcut) {
        // No point in doing anything fancy.
        overlay_fmt = IMGFMT_BGRA;
        p->scale_in_tiles = false;
    } else {
        struct mp_regular_imgfmt odesc = vfdesc;
        // Just use 8 bit as well (should be fine, may use less memory).
        odesc.component_type = MP_COMPONENT_TYPE_UINT;
        odesc.component_size = 1;
        odesc.component_pad = 0;

        // Ensure there's alpha.
        if (odesc.planes[odesc.num_planes - 1].components[0] != 4) {
            if (odesc.num_planes >= 4)
                return false; // wat
            odesc.planes[odesc.num_planes++] =
                (struct mp_regular_imgfmt_plane){1, {4}};
        }

        overlay_fmt = mp_find_regular_imgfmt(&odesc);
        p->scale_in_tiles = odesc.chroma_xs || odesc.chroma_ys;
    }
    if (!overlay_fmt)
        return false;

    p->overlay_to_f32 = mp_repack_create_planar(overlay_fmt, false, rflags);
    talloc_steal(p, p->overlay_to_f32);
    if (!p->overlay_to_f32)
        return false;

    int render_fmt = mp_repack_get_format_dst(p->overlay_to_f32);

    struct mp_regular_imgfmt ofdesc = {0};
    mp_get_regular_imgfmt(&ofdesc, render_fmt);

    if (ofdesc.planes[ofdesc.num_planes - 1].components[0] != 4)
        return false;

    // The formats must be the same, minus possible lack of alpha in vfdesc.
    if (ofdesc.num_planes != vfdesc.num_planes &&
        ofdesc.num_planes - 1 != vfdesc.num_planes)
        return false;
    for (int n = 0; n < vfdesc.num_planes; n++) {
        if (vfdesc.planes[n].components[0] != ofdesc.planes[n].components[0])
            return false;
    }

    p->align_x = mp_repack_get_align_x(p->video_to_f32);
    p->align_y = mp_repack_get_align_y(p->video_to_f32);

    assert(p->align_x >= mp_repack_get_align_x(p->overlay_to_f32));
    assert(p->align_y >= mp_repack_get_align_y(p->overlay_to_f32));

    if (p->align_x > SLICE_W || p->align_y > TILE_H)
        return false;

    p->w = MP_ALIGN_UP(params->w, p->align_x);
    int slice_h = p->align_y;
    p->h = MP_ALIGN_UP(params->h, slice_h);

    // Size of the overlay. If scaling in tiles, round up to tiles, so we don't
    // need to reinit the scale for right/bottom tiles.
    int w = p->w;
    int h = p->h;
    if (p->scale_in_tiles) {
        w = MP_ALIGN_UP(w, SLICE_W);
        h = MP_ALIGN_UP(h, TILE_H);
    }

    p->rgba_overlay = talloc_steal(p, mp_image_alloc(IMGFMT_BGRA, w, h));
    p->overlay_tmp = talloc_steal(p, mp_image_alloc(render_fmt, SLICE_W, slice_h));
    p->video_tmp = talloc_steal(p, mp_image_alloc(vid_f32_fmt, SLICE_W, slice_h));
    if (!p->rgba_overlay || !p->overlay_tmp || !p->video_tmp)
        return false;

    mp_image_params_guess_csp(&p->rgba_overlay->params);
    p->rgba_overlay->params.alpha = MP_ALPHA_PREMUL;

    p->overlay_tmp->params.color = params->color;
    p->video_tmp->params.color = params->color;

    if (p->rgba_overlay->imgfmt == overlay_fmt) {
        if (!repack_config_buffers(p->overlay_to_f32, 0, p->overlay_tmp,
                                   0, p->rgba_overlay, NULL))
            return false;
    } else {
        // Generally non-RGB.
        p->video_overlay = talloc_steal(p, mp_image_alloc(overlay_fmt, w, h));
        if (!p->video_overlay)
            return false;

        p->video_overlay->params.color = params->color;
        p->video_overlay->params.chroma_location = params->chroma_location;
        p->video_overlay->params.alpha = MP_ALPHA_PREMUL;

        if (p->scale_in_tiles)
            p->video_overlay->params.chroma_location = MP_CHROMA_CENTER;

        p->rgba_to_overlay = alloc_scaler(p);
        p->rgba_to_overlay->allow_zimg = true;
        if (!mp_sws_supports_formats(p->rgba_to_overlay,
                            p->video_overlay->imgfmt, p->rgba_overlay->imgfmt))
            return false;

        if (!repack_config_buffers(p->overlay_to_f32, 0, p->overlay_tmp,
                                   0, p->video_overlay, NULL))
            return false;

        // Setup a scaled alpha plane if chroma-subsampling is present.
        int xs = p->video_overlay->fmt.chroma_xs;
        int ys = p->video_overlay->fmt.chroma_ys;
        if (xs || ys) {
            // Require float so format selection becomes simpler (maybe).
            assert(rflags & REPACK_CREATE_PLANAR_F32);

            // For extracting the alpha plane, construct a gray format that is
            // compatible with the alpha one.
            struct mp_regular_imgfmt odesc = {0};
            mp_get_regular_imgfmt(&odesc, overlay_fmt);
            assert(odesc.component_size);
            int aplane = odesc.num_planes - 1;
            assert(odesc.planes[aplane].num_components == 1);
            assert(odesc.planes[aplane].components[0] == 4);
            struct mp_regular_imgfmt cadesc = odesc;
            cadesc.num_planes = 1;
            cadesc.planes[0] = (struct mp_regular_imgfmt_plane){1, {1}};
            cadesc.chroma_xs = cadesc.chroma_ys = 0;

            int calpha_fmt = mp_find_regular_imgfmt(&cadesc);
            if (!calpha_fmt)
                return false;

            // Unscaled alpha plane from p->video_overlay.
            p->alpha_overlay = talloc_zero(p, struct mp_image);
            mp_image_setfmt(p->alpha_overlay, calpha_fmt);
            mp_image_set_size(p->alpha_overlay, w, h);
            p->alpha_overlay->planes[0] = p->video_overlay->planes[aplane];
            p->alpha_overlay->stride[0] = p->video_overlay->stride[aplane];

            // Full range gray always has the same range as alpha.
            p->alpha_overlay->params.color.levels = MP_CSP_LEVELS_PC;
            mp_image_params_guess_csp(&p->alpha_overlay->params);

            p->calpha_overlay =
                talloc_steal(p, mp_image_alloc(calpha_fmt, w >> xs, h >> ys));
            if (!p->calpha_overlay)
                return false;
            p->calpha_overlay->params.color = p->alpha_overlay->params.color;

            p->calpha_to_f32 = mp_repack_create_planar(calpha_fmt, false, rflags);
            talloc_steal(p, p->calpha_to_f32);
            if (!p->calpha_to_f32)
                return false;

            int af32_fmt = mp_repack_get_format_dst(p->calpha_to_f32);
            p->calpha_tmp = talloc_steal(p, mp_image_alloc(af32_fmt, SLICE_W, 1));
            if (!p->calpha_tmp)
                return false;

            if (!repack_config_buffers(p->calpha_to_f32, 0, p->calpha_tmp,
                                       0, p->calpha_overlay, NULL))
                return false;

            p->alpha_to_calpha = alloc_scaler(p);
            if (!mp_sws_supports_formats(p->alpha_to_calpha,
                                         calpha_fmt, calpha_fmt))
                return false;
        }
    }

    if (need_premul) {
        p->premul = alloc_scaler(p);
        p->unpremul = alloc_scaler(p);
        p->premul_tmp = mp_image_alloc(params->imgfmt, params->w, params->h);
        talloc_steal(p, p->premul_tmp);
        if (!p->premul_tmp)
            return false;
        mp_image_set_params(p->premul_tmp, params);
        p->premul_tmp->params.alpha = MP_ALPHA_PREMUL;

        // Only zimg supports this.
        p->premul->force_scaler = MP_SWS_ZIMG;
        p->unpremul->force_scaler = MP_SWS_ZIMG;
    }

    init_general(p);

    return true;
}

static bool reinit_to_overlay(struct mp_draw_sub_cache *p)
{
    p->align_x = 1;
    p->align_y = 1;

    p->w = p->params.w;
    p->h = p->params.h;

    p->rgba_overlay = talloc_steal(p, mp_image_alloc(IMGFMT_BGRA, p->w, p->h));
    if (!p->rgba_overlay)
        return false;

    mp_image_params_guess_csp(&p->rgba_overlay->params);
    p->rgba_overlay->params.alpha = MP_ALPHA_PREMUL;

    // Some non-sense with the intention to somewhat isolate the returned image.
    mp_image_setfmt(&p->res_overlay, p->rgba_overlay->imgfmt);
    mp_image_set_size(&p->res_overlay, p->rgba_overlay->w, p->rgba_overlay->h);
    mp_image_copy_attributes(&p->res_overlay, p->rgba_overlay);
    p->res_overlay.planes[0] = p->rgba_overlay->planes[0];
    p->res_overlay.stride[0] = p->rgba_overlay->stride[0];

    init_general(p);

    // Mark all dirty (for full reinit of user state).
    for (int y = 0; y < p->rgba_overlay->h; y++) {
        for (int sx = 0; sx < p->s_w; sx++)
            p->slices[y * p->s_w + sx] = (struct slice){0, SLICE_W};
    }

    return true;
}

static bool check_reinit(struct mp_draw_sub_cache *p,
                         struct mp_image_params *params, bool to_video)
{
    if (!mp_image_params_equal(&p->params, params) || !p->rgba_overlay) {
        talloc_free_children(p);
        *p = (struct mp_draw_sub_cache){.global = p->global, .params = *params};
        if (!(to_video ? reinit_to_video(p) : reinit_to_overlay(p))) {
            talloc_free_children(p);
            *p = (struct mp_draw_sub_cache){.global = p->global};
            return false;
        }
    }
    return true;
}

char *mp_draw_sub_get_dbg_info(struct mp_draw_sub_cache *p)
{
    assert(p);

    return talloc_asprintf(NULL,
        "align=%d:%d ov=%-7s, ov_f=%s, v_f=%s, a=%s, ca=%s, ca_f=%s",
        p->align_x, p->align_y,
        mp_imgfmt_to_name(p->video_overlay ? p->video_overlay->imgfmt : 0),
        mp_imgfmt_to_name(p->overlay_tmp->imgfmt),
        mp_imgfmt_to_name(p->video_tmp->imgfmt),
        mp_imgfmt_to_name(p->alpha_overlay ? p->alpha_overlay->imgfmt : 0),
        mp_imgfmt_to_name(p->calpha_overlay ? p->calpha_overlay->imgfmt : 0),
        mp_imgfmt_to_name(p->calpha_tmp ? p->calpha_tmp->imgfmt : 0));
}

struct mp_draw_sub_cache *mp_draw_sub_alloc(void *ta_parent, struct mpv_global *g)
{
    struct mp_draw_sub_cache *c = talloc_zero(ta_parent, struct mp_draw_sub_cache);
    c->global = g;
    return c;
}

bool mp_draw_sub_bitmaps(struct mp_draw_sub_cache *p, struct mp_image *dst,
                         struct sub_bitmap_list *sbs_list)
{
    bool ok = false;

    // dst must at least be as large as the bounding box, or you may get memory
    // corruption.
    assert(dst->w >= sbs_list->w);
    assert(dst->h >= sbs_list->h);

    if (!check_reinit(p, &dst->params, true))
        return false;

    if (p->change_id != sbs_list->change_id) {
        p->change_id = sbs_list->change_id;

        clear_rgba_overlay(p);

        for (int n = 0; n < sbs_list->num_items; n++) {
            if (!render_sb(p, sbs_list->items[n]))
                goto done;
        }

        if (!convert_to_video_overlay(p))
            goto done;
    }

    if (p->any_osd) {
        struct mp_image *target = dst;
        if (p->premul_tmp) {
            if (mp_sws_scale(p->premul, p->premul_tmp, dst) < 0)
                goto done;
            target = p->premul_tmp;
        }

        if (!blend_overlay_with_video(p, target))
            goto done;

        if (target != dst) {
            if (mp_sws_scale(p->unpremul, dst, p->premul_tmp) < 0)
                goto done;
        }
    }

    ok = true;

done:
    return ok;
}

// Bounding boxes for mp_draw_sub_overlay() API. For simplicity, each rectangle
// covers a fixed tile on the screen, starts out empty, but is not extended
// beyond the tile. In the simplest case, there's only 1 rect/tile for everything.
struct rc_grid {
    unsigned w, h;                  // size in grid tiles
    unsigned r_w, r_h;              // size of a grid tile in pixels
    struct mp_rect *rcs;            // rcs[x * w + y]
};

static void init_rc_grid(struct rc_grid *gr, struct mp_draw_sub_cache *p,
                         struct mp_rect *rcs, int max_rcs)
{
    *gr = (struct rc_grid){ .w = max_rcs ? 1 : 0, .h = max_rcs ? 1 : 0,
                            .rcs = rcs, .r_w = p->s_w * SLICE_W, .r_h = p->h, };

    // Dumb iteration to figure out max. size because I'm stupid.
    bool more = true;
    while (more) {
        more = false;
        if (gr->r_h >= 128) {
            if (gr->w * gr->h * 2 > max_rcs)
                break;
            gr->h *= 2;
            gr->r_h = (p->h + gr->h - 1) / gr->h;
            more = true;
        }
        if (gr->r_w >= SLICE_W * 2) {
            if (gr->w * gr->h * 2 > max_rcs)
                break;
            gr->w *= 2;
            gr->r_w = (p->s_w + gr->w - 1) / gr->w * SLICE_W;
            more = true;
        }
    }

    assert(gr->r_h * gr->h >= p->h);
    assert(!(gr->r_w & (SLICE_W - 1)));
    assert(gr->r_w * gr->w >= p->w);

    // Init with empty (degenerate) rectangles.
    for (int y = 0; y < gr->h; y++) {
        for (int x = 0; x < gr->w; x++) {
            struct mp_rect *rc = &gr->rcs[y * gr->w + x];
            rc->x1 = x * gr->r_w;
            rc->y1 = y * gr->r_h;
            rc->x0 = rc->x1 + gr->r_w;
            rc->y0 = rc->y1 + gr->r_h;
        }
    }
}

// Extend given grid with contents of p->slices.
static void mark_rcs(struct mp_draw_sub_cache *p, struct rc_grid *gr)
{
    for (int y = 0; y < p->h; y++) {
        struct slice *line = &p->slices[y * p->s_w];
        struct mp_rect *rcs = &gr->rcs[y / gr->r_h * gr->w];

        for (int sx = 0; sx < p->s_w; sx++) {
            struct slice *s = &line[sx];
            if (s->x0 < s->x1) {
                unsigned xpos = sx * SLICE_W;
                struct mp_rect *rc = &rcs[xpos / gr->r_w];
                rc->y0 = MPMIN(rc->y0, y);
                rc->y1 = MPMAX(rc->y1, y + 1);
                rc->x0 = MPMIN(rc->x0, xpos + s->x0);
                rc->x1 = MPMAX(rc->x1, xpos + s->x1);
            }
        }
    }
}

// Remove empty RCs, and return rc count.
static int return_rcs(struct rc_grid *gr)
{
    int num = 0, cnt = gr->w * gr->h;
    for (int n = 0; n < cnt; n++) {
        struct mp_rect *rc = &gr->rcs[n];
        if (rc->x0 < rc->x1 && rc->y0 < rc->y1)
            gr->rcs[num++] = *rc;
    }
    return num;
}

struct mp_image *mp_draw_sub_overlay(struct mp_draw_sub_cache *p,
                                     struct sub_bitmap_list *sbs_list,
                                     struct mp_rect *act_rcs,
                                     int max_act_rcs,
                                     int *num_act_rcs,
                                     struct mp_rect *mod_rcs,
                                     int max_mod_rcs,
                                     int *num_mod_rcs)
{
    *num_act_rcs = 0;
    *num_mod_rcs = 0;

    struct mp_image_params params = {.w = sbs_list->w, .h = sbs_list->h};
    if (!check_reinit(p, &params, false))
        return NULL;

    struct rc_grid gr_act, gr_mod;
    init_rc_grid(&gr_act, p, act_rcs, max_act_rcs);
    init_rc_grid(&gr_mod, p, mod_rcs, max_mod_rcs);

    if (p->change_id != sbs_list->change_id) {
        p->change_id = sbs_list->change_id;

        mark_rcs(p, &gr_mod);

        clear_rgba_overlay(p);

        for (int n = 0; n < sbs_list->num_items; n++) {
            if (!render_sb(p, sbs_list->items[n])) {
                p->change_id = 0;
                return NULL;
            }
        }

        mark_rcs(p, &gr_mod);
    }

    mark_rcs(p, &gr_act);

    *num_act_rcs = return_rcs(&gr_act);
    *num_mod_rcs = return_rcs(&gr_mod);

    return &p->res_overlay;
}

// vim: ts=4 sw=4 et tw=80
