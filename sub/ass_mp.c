/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
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

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include <ass/ass.h>
#include <ass/ass_types.h>

#include "common/common.h"
#include "common/msg.h"
#include "options/path.h"
#include "ass_mp.h"
#include "img_convert.h"
#include "osd.h"
#include "stream/stream.h"
#include "options/options.h"
#include "video/out/bitmap_packer.h"
#include "video/mp_image.h"

// res_y should be track->PlayResY
// It determines scaling of font sizes and more.
void mp_ass_set_style(ASS_Style *style, double res_y,
                      const struct osd_style_opts *opts)
{
    if (!style)
        return;

    if (opts->font) {
        if (!style->FontName || strcmp(style->FontName, opts->font) != 0) {
            free(style->FontName);
            style->FontName = strdup(opts->font);
        }
    }

    // libass_font_size = FontSize * (window_height / res_y)
    // scale translates parameters from PlayResY=720 to res_y
    double scale = res_y / 720.0;

    style->FontSize = opts->font_size * scale;
    style->PrimaryColour = MP_ASS_COLOR(opts->color);
    style->SecondaryColour = style->PrimaryColour;
    style->OutlineColour = MP_ASS_COLOR(opts->outline_color);
    style->BackColour = MP_ASS_COLOR(opts->back_color);
    style->BorderStyle = opts->border_style;
    style->Outline = opts->outline_size * scale;
    style->Shadow = opts->shadow_offset * scale;
    style->Spacing = opts->spacing * scale;
    style->MarginL = opts->margin_x * scale;
    style->MarginR = style->MarginL;
    style->MarginV = opts->margin_y * scale;
    style->ScaleX = 1.;
    style->ScaleY = 1.;
    style->Alignment = 1 + (opts->align_x + 1) + (opts->align_y + 2) % 3 * 4;
#ifdef ASS_JUSTIFY_LEFT
    style->Justify = opts->justify;
#endif
    style->Blur = opts->blur;
    style->Bold = opts->bold;
    style->Italic = opts->italic;
}

void mp_ass_configure_fonts(ASS_Renderer *priv, struct osd_style_opts *opts,
                            struct mpv_global *global, struct mp_log *log)
{
    void *tmp = talloc_new(NULL);
    char *default_font = mp_find_config_file(tmp, global, "subfont.ttf");
    char *config       = mp_find_config_file(tmp, global, "fonts.conf");

    if (default_font && !mp_path_exists(default_font))
        default_font = NULL;

    int font_provider = ASS_FONTPROVIDER_AUTODETECT;
    if (opts->font_provider == 1)
        font_provider = ASS_FONTPROVIDER_NONE;
    if (opts->font_provider == 2)
        font_provider = ASS_FONTPROVIDER_FONTCONFIG;

    mp_verbose(log, "Setting up fonts...\n");
    ass_set_fonts(priv, default_font, opts->font, font_provider, config, 1);
    mp_verbose(log, "Done.\n");

    talloc_free(tmp);
}

static const int map_ass_level[] = {
    MSGL_ERR,           // 0 "FATAL errors"
    MSGL_WARN,
    MSGL_INFO,
    MSGL_V,
    MSGL_V,
    MSGL_DEBUG,         // 5 application recommended level
    MSGL_TRACE,
    MSGL_TRACE,         // 7 "verbose DEBUG"
};

PRINTF_ATTRIBUTE(2, 0)
static void message_callback(int level, const char *format, va_list va, void *ctx)
{
    struct mp_log *log = ctx;
    if (!log)
        return;
    level = map_ass_level[level];
    mp_msg_va(log, level, format, va);
    // libass messages lack trailing \n
    mp_msg(log, level, "\n");
}

ASS_Library *mp_ass_init(struct mpv_global *global,
                         struct osd_style_opts *opts, struct mp_log *log)
{
    char *path = opts->fonts_dir && opts->fonts_dir[0] ?
                 mp_get_user_path(NULL, global, opts->fonts_dir) :
                 mp_find_config_file(NULL, global, "fonts");
    mp_dbg(log, "ASS library version: 0x%x (runtime 0x%x)\n",
           (unsigned)LIBASS_VERSION, ass_library_version());
    ASS_Library *priv = ass_library_init();
    if (!priv)
        abort();
    ass_set_message_cb(priv, message_callback, log);
    if (path)
        ass_set_fonts_dir(priv, path);
    talloc_free(path);
    return priv;
}

void mp_ass_flush_old_events(ASS_Track *track, long long ts)
{
    int n = 0;
    for (; n < track->n_events; n++) {
        if ((track->events[n].Start + track->events[n].Duration) >= ts)
            break;
        ass_free_event(track, n);
        track->n_events--;
    }
    for (int i = 0; n > 0 && i < track->n_events; i++) {
        track->events[i] = track->events[i+n];
    }
}

static void draw_ass_rgba(unsigned char *src, int src_w, int src_h,
                          int src_stride, unsigned char *dst, size_t dst_stride,
                          int dst_x, int dst_y, uint32_t color)
{
    const unsigned int r = (color >> 24) & 0xff;
    const unsigned int g = (color >> 16) & 0xff;
    const unsigned int b = (color >>  8) & 0xff;
    const unsigned int a = 0xff - (color & 0xff);

    dst += dst_y * dst_stride + dst_x * 4;

    for (int y = 0; y < src_h; y++, dst += dst_stride, src += src_stride) {
        uint32_t *dstrow = (uint32_t *) dst;
        for (int x = 0; x < src_w; x++) {
            const unsigned int v = src[x];
            int rr = (r * a * v);
            int gg = (g * a * v);
            int bb = (b * a * v);
            int aa =      a * v;
            uint32_t dstpix = dstrow[x];
            unsigned int dstb =  dstpix        & 0xFF;
            unsigned int dstg = (dstpix >>  8) & 0xFF;
            unsigned int dstr = (dstpix >> 16) & 0xFF;
            unsigned int dsta = (dstpix >> 24) & 0xFF;
            dstb = (bb       + dstb * (255 * 255 - aa)) / (255 * 255);
            dstg = (gg       + dstg * (255 * 255 - aa)) / (255 * 255);
            dstr = (rr       + dstr * (255 * 255 - aa)) / (255 * 255);
            dsta = (aa * 255 + dsta * (255 * 255 - aa)) / (255 * 255);
            dstrow[x] = dstb | (dstg << 8) | (dstr << 16) | (dsta << 24);
        }
    }
}

struct mp_ass_packer {
    struct sub_bitmap *cached_parts; // only for the array memory
    struct mp_image *cached_img;
    struct sub_bitmaps cached_subs;
    bool cached_subs_valid;
    struct sub_bitmap rgba_imgs[MP_SUB_BB_LIST_MAX];
    struct bitmap_packer *packer;
};

// Free with talloc_free().
struct mp_ass_packer *mp_ass_packer_alloc(void *ta_parent)
{
    struct mp_ass_packer *p = talloc_zero(ta_parent, struct mp_ass_packer);
    p->packer = talloc_zero(p, struct bitmap_packer);
    p->packer->padding = 1; // assume bilinear sampling
    return p;
}

static bool pack(struct mp_ass_packer *p, struct sub_bitmaps *res, int imgfmt)
{
    packer_set_size(p->packer, res->num_parts);

    for (int n = 0; n < res->num_parts; n++)
        p->packer->in[n] = (struct pos){res->parts[n].w, res->parts[n].h};

    if (p->packer->count == 0 || packer_pack(p->packer) < 0)
        return false;

    struct pos bb[2];
    packer_get_bb(p->packer, bb);

    res->packed_w = bb[1].x;
    res->packed_h = bb[1].y;

    if (!p->cached_img || p->cached_img->w < res->packed_w ||
                          p->cached_img->h < res->packed_h ||
                          p->cached_img->imgfmt != imgfmt)
    {
        talloc_free(p->cached_img);
        p->cached_img = mp_image_alloc(imgfmt, p->packer->w, p->packer->h);
        if (!p->cached_img) {
            packer_reset(p->packer);
            return false;
        }
        talloc_steal(p, p->cached_img);
    }

    if (!mp_image_make_writeable(p->cached_img)) {
        packer_reset(p->packer);
        return false;
    }

    res->packed = p->cached_img;

    for (int n = 0; n < res->num_parts; n++) {
        struct sub_bitmap *b = &res->parts[n];
        struct pos pos = p->packer->result[n];

        b->src_x = pos.x;
        b->src_y = pos.y;
    }

    return true;
}

static void fill_padding_1(uint8_t *base, int w, int h, int stride, int padding)
{
    for (int row = 0; row < h; ++row) {
        uint8_t *row_ptr = base + row * stride;
        uint8_t left_pixel = row_ptr[0];
        uint8_t right_pixel = row_ptr[w - 1];

        for (int i = 1; i <= padding; ++i)
            row_ptr[-i] = left_pixel;

        for (int i = 0; i < padding; ++i)
            row_ptr[w + i] = right_pixel;
    }

    int row_bytes = (w + 2 * padding);
    uint8_t *top_row = base - padding;
    for (int i = 1; i <= padding; ++i)
        memcpy(base - i * stride - padding, top_row, row_bytes);

    uint8_t *last_row = base + (h - 1) * stride - padding;
    for (int i = 0; i < padding; ++i)
        memcpy(base + (h + i) * stride - padding, last_row, row_bytes);
}

static void fill_padding_4(uint8_t *base, int w, int h, int stride, int padding)
{
    for (int row = 0; row < h; ++row) {
        uint32_t *row_ptr = (uint32_t *)(base + row * stride);
        uint32_t left_pixel = row_ptr[0];
        uint32_t right_pixel = row_ptr[w - 1];

        for (int i = 1; i <= padding; ++i)
            row_ptr[-i] = left_pixel;

        for (int i = 0; i < padding; ++i)
            row_ptr[w + i] = right_pixel;
    }

    int row_bytes = (w + 2 * padding) * 4;
    uint8_t *top_row = base - padding * 4;
    for (int i = 1; i <= padding; ++i)
        memcpy(base - i * stride - padding * 4, top_row, row_bytes);

    uint8_t *last_row = base + (h - 1) * stride - padding * 4;
    for (int i = 0; i < padding; ++i)
        memcpy(base + (h + i) * stride - padding * 4, last_row, row_bytes);
}

static bool pack_libass(struct mp_ass_packer *p, struct sub_bitmaps *res)
{
    if (!pack(p, res, IMGFMT_Y8))
        return false;

    int padding = p->packer->padding;
    uint8_t *base = res->packed->planes[0];
    int stride = res->packed->stride[0];

    for (int n = 0; n < res->num_parts; n++) {
        struct sub_bitmap *b = &res->parts[n];
        void *pdata = base + b->src_y * stride + b->src_x;
        memcpy_pic(pdata, b->bitmap, b->w, b->h, stride, b->stride);
        fill_padding_1(pdata, b->w, b->h, stride, padding);

        b->bitmap = pdata;
        b->stride = stride;
    }

    return true;
}

static bool pack_rgba(struct mp_ass_packer *p, struct sub_bitmaps *res)
{
    struct mp_rect bb_list[MP_SUB_BB_LIST_MAX];
    int num_bb = mp_get_sub_bb_list(res, bb_list, MP_SUB_BB_LIST_MAX);

    struct sub_bitmaps imgs = {
        .change_id = res->change_id,
        .format = SUBBITMAP_BGRA,
        .parts = p->rgba_imgs,
        .num_parts = num_bb,
    };

    for (int n = 0; n < imgs.num_parts; n++) {
        imgs.parts[n].w = bb_list[n].x1 - bb_list[n].x0;
        imgs.parts[n].h = bb_list[n].y1 - bb_list[n].y0;
    }

    if (!pack(p, &imgs, IMGFMT_BGRA))
        return false;

    int padding = p->packer->padding;
    uint8_t *base = imgs.packed->planes[0];
    int stride = imgs.packed->stride[0];

    for (int n = 0; n < num_bb; n++) {
        struct mp_rect bb = bb_list[n];
        struct sub_bitmap *b = &imgs.parts[n];

        b->x = bb.x0;
        b->y = bb.y0;
        b->w = b->dw = mp_rect_w(bb);
        b->h = b->dh = mp_rect_h(bb);
        b->stride = stride;
        b->bitmap = base + b->stride * b->src_y + b->src_x * 4;
        memset_pic(b->bitmap, 0, b->w * 4, b->h, b->stride);

        for (int i = 0; i < res->num_parts; i++) {
            struct sub_bitmap *s = &res->parts[i];

            // Assume mp_get_sub_bb_list() never splits sub bitmaps
            // So we don't clip/adjust the size of the sub bitmap
            if (s->x >= b->x + b->w || s->x + s->w <= b->x ||
                s->y >= b->y + b->h || s->y + s->h <= b->y)
                continue;

            draw_ass_rgba(s->bitmap, s->w, s->h, s->stride,
                          b->bitmap, b->stride,
                          s->x - b->x, s->y - b->y,
                          s->libass.color);
        }
        fill_padding_4(b->bitmap, b->w, b->h, b->stride, padding);
    }

    *res = imgs;
    return true;
}

// Pack the contents of image_lists[0] to image_lists[num_image_lists-1] into
// a single image, and make *out point to it. *out is completely overwritten.
// If libass reported any change, image_lists_changed must be set (it then
// repacks all images). preferred_osd_format can be set to a desired
// sub_bitmap_format. Currently, only SUBBITMAP_LIBASS is supported.
void mp_ass_packer_pack(struct mp_ass_packer *p, ASS_Image **image_lists,
                        int num_image_lists, bool image_lists_changed, bool video_color_space,
                        int preferred_osd_format, struct sub_bitmaps *out)
{
    int format = preferred_osd_format == SUBBITMAP_BGRA ? SUBBITMAP_BGRA
                                                        : SUBBITMAP_LIBASS;

    if (p->cached_subs_valid && !image_lists_changed &&
        p->cached_subs.format == format)
    {
        *out = p->cached_subs;
        return;
    }

    *out = (struct sub_bitmaps){.change_id = 1};
    p->cached_subs_valid = false;

    struct sub_bitmaps res = {
        .change_id = image_lists_changed,
        .format = SUBBITMAP_LIBASS,
        .parts = p->cached_parts,
        .video_color_space = video_color_space,
    };

    for (int n = 0; n < num_image_lists; n++) {
        for (struct ass_image *img = image_lists[n]; img; img = img->next) {
            if (img->w == 0 || img->h == 0)
                continue;
            MP_TARRAY_GROW(p, p->cached_parts, res.num_parts);
            res.parts = p->cached_parts;
            struct sub_bitmap *b = &res.parts[res.num_parts];
            b->bitmap = img->bitmap;
            b->stride = img->stride;
            b->libass.color = img->color;
            b->dw = b->w = img->w;
            b->dh = b->h = img->h;
            b->x = img->dst_x;
            b->y = img->dst_y;
            res.num_parts++;
        }
    }

    bool r = false;
    if (format == SUBBITMAP_BGRA) {
        r = pack_rgba(p, &res);
    } else {
        r = pack_libass(p, &res);
    }

    if (!r)
        return;

    *out = res;
    p->cached_subs = res;
    p->cached_subs.change_id = 0;
    p->cached_subs_valid = true;
}

// Set *out_rc to [x0, y0, x1, y1] of the graphical bounding box in script
// coordinates.
// Set it to [inf, inf, -inf, -inf] if empty.
void mp_ass_get_bb(ASS_Image *image_list, ASS_Track *track,
                   struct mp_osd_res *res, double *out_rc)
{
    double rc[4] = {INFINITY, INFINITY, -INFINITY, -INFINITY};

    for (ASS_Image *img = image_list; img; img = img->next) {
        if (img->w == 0 || img->h == 0)
            continue;
        rc[0] = MPMIN(rc[0], img->dst_x);
        rc[1] = MPMIN(rc[1], img->dst_y);
        rc[2] = MPMAX(rc[2], img->dst_x + img->w);
        rc[3] = MPMAX(rc[3], img->dst_y + img->h);
    }

    double scale = track->PlayResY / (double)MPMAX(res->h, 1);
    if (scale > 0) {
        for (int i = 0; i < 4; i++)
            out_rc[i] = rc[i] * scale;
    }
}
