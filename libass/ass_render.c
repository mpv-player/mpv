/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of libass.
 *
 * libass is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libass is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <assert.h>
#include <math.h>
#include <inttypes.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_GLYPH_H
#include FT_SYNTHESIS_H

#include "ass.h"
#include "ass_font.h"
#include "ass_bitmap.h"
#include "ass_cache.h"
#include "ass_utils.h"
#include "ass_fontconfig.h"
#include "ass_library.h"
#include "ass_drawing.h"
#include "ass_render.h"
#include "ass_parse.h"

#define MAX_GLYPHS_INITIAL 1024
#define MAX_LINES_INITIAL 64
#define SUBPIXEL_MASK 63
#define SUBPIXEL_ACCURACY 7    // d6 mask for subpixel accuracy adjustment
#define GLYPH_CACHE_MAX 1000
#define BITMAP_CACHE_MAX_SIZE 50 * 1048576

static void ass_lazy_track_init(ASS_Renderer *render_priv)
{
    ASS_Track *track = render_priv->track;

    if (track->PlayResX && track->PlayResY)
        return;
    if (!track->PlayResX && !track->PlayResY) {
        ass_msg(render_priv->library, MSGL_WARN,
               "Neither PlayResX nor PlayResY defined. Assuming 384x288");
        track->PlayResX = 384;
        track->PlayResY = 288;
    } else {
        if (!track->PlayResY && track->PlayResX == 1280) {
            track->PlayResY = 1024;
            ass_msg(render_priv->library, MSGL_WARN,
                   "PlayResY undefined, setting to %d", track->PlayResY);
        } else if (!track->PlayResY) {
            track->PlayResY = track->PlayResX * 3 / 4;
            ass_msg(render_priv->library, MSGL_WARN,
                   "PlayResY undefined, setting to %d", track->PlayResY);
        } else if (!track->PlayResX && track->PlayResY == 1024) {
            track->PlayResX = 1280;
            ass_msg(render_priv->library, MSGL_WARN,
                   "PlayResX undefined, setting to %d", track->PlayResX);
        } else if (!track->PlayResX) {
            track->PlayResX = track->PlayResY * 4 / 3;
            ass_msg(render_priv->library, MSGL_WARN,
                   "PlayResX undefined, setting to %d", track->PlayResX);
        }
    }
}

ASS_Renderer *ass_renderer_init(ASS_Library *library)
{
    int error;
    FT_Library ft;
    ASS_Renderer *priv = 0;
    int vmajor, vminor, vpatch;

    error = FT_Init_FreeType(&ft);
    if (error) {
        ass_msg(library, MSGL_FATAL, "%s failed", "FT_Init_FreeType");
        goto ass_init_exit;
    }

    FT_Library_Version(ft, &vmajor, &vminor, &vpatch);
    ass_msg(library, MSGL_V, "FreeType library version: %d.%d.%d",
           vmajor, vminor, vpatch);
    ass_msg(library, MSGL_V, "FreeType headers version: %d.%d.%d",
           FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH);

    priv = calloc(1, sizeof(ASS_Renderer));
    if (!priv) {
        FT_Done_FreeType(ft);
        goto ass_init_exit;
    }

    priv->synth_priv = ass_synth_init(BLUR_MAX_RADIUS);

    priv->library = library;
    priv->ftlibrary = ft;
    // images_root and related stuff is zero-filled in calloc

    priv->cache.font_cache = ass_font_cache_init(library);
    priv->cache.bitmap_cache = ass_bitmap_cache_init(library);
    priv->cache.composite_cache = ass_composite_cache_init(library);
    priv->cache.glyph_cache = ass_glyph_cache_init(library);
    priv->cache.glyph_max = GLYPH_CACHE_MAX;
    priv->cache.bitmap_max_size = BITMAP_CACHE_MAX_SIZE;

    priv->text_info.max_glyphs = MAX_GLYPHS_INITIAL;
    priv->text_info.max_lines = MAX_LINES_INITIAL;
    priv->text_info.glyphs =
        calloc(MAX_GLYPHS_INITIAL, sizeof(GlyphInfo));
    priv->text_info.lines = calloc(MAX_LINES_INITIAL, sizeof(LineInfo));

  ass_init_exit:
    if (priv)
        ass_msg(library, MSGL_INFO, "Init");
    else
        ass_msg(library, MSGL_ERR, "Init failed");

    return priv;
}

void ass_set_cache_limits(ASS_Renderer *render_priv, int glyph_max,
                          int bitmap_max)
{
    render_priv->cache.glyph_max = glyph_max ? glyph_max : GLYPH_CACHE_MAX;
    render_priv->cache.bitmap_max_size = bitmap_max ? 1048576 * bitmap_max :
                                         BITMAP_CACHE_MAX_SIZE;
}

static void free_list_clear(ASS_Renderer *render_priv)
{
    if (render_priv->free_head) {
        FreeList *item = render_priv->free_head;
        while(item) {
            FreeList *oi = item;
            free(item->object);
            item = item->next;
            free(oi);
        }
        render_priv->free_head = NULL;
    }
}

static void ass_free_images(ASS_Image *img);

void ass_renderer_done(ASS_Renderer *render_priv)
{
    ass_font_cache_done(render_priv->cache.font_cache);
    ass_bitmap_cache_done(render_priv->cache.bitmap_cache);
    ass_composite_cache_done(render_priv->cache.composite_cache);
    ass_glyph_cache_done(render_priv->cache.glyph_cache);

    ass_free_images(render_priv->images_root);
    ass_free_images(render_priv->prev_images_root);

    if (render_priv->state.stroker) {
        FT_Stroker_Done(render_priv->state.stroker);
        render_priv->state.stroker = 0;
    }
    if (render_priv && render_priv->ftlibrary)
        FT_Done_FreeType(render_priv->ftlibrary);
    if (render_priv && render_priv->fontconfig_priv)
        fontconfig_done(render_priv->fontconfig_priv);
    if (render_priv && render_priv->synth_priv)
        ass_synth_done(render_priv->synth_priv);
    if (render_priv && render_priv->eimg)
        free(render_priv->eimg);
    free(render_priv->text_info.glyphs);
    free(render_priv->text_info.lines);

    free(render_priv->settings.default_font);
    free(render_priv->settings.default_family);

    free_list_clear(render_priv);
    free(render_priv);
}

/**
 * \brief Create a new ASS_Image
 * Parameters are the same as ASS_Image fields.
 */
static ASS_Image *my_draw_bitmap(unsigned char *bitmap, int bitmap_w,
                                 int bitmap_h, int stride, int dst_x,
                                 int dst_y, uint32_t color)
{
    ASS_Image *img = calloc(1, sizeof(ASS_Image));

    img->w = bitmap_w;
    img->h = bitmap_h;
    img->stride = stride;
    img->bitmap = bitmap;
    img->color = color;
    img->dst_x = dst_x;
    img->dst_y = dst_y;

    return img;
}

static double x2scr_pos(ASS_Renderer *render_priv, double x);
static double y2scr_pos(ASS_Renderer *render_priv, double y);

/*
 * \brief Convert bitmap glyphs into ASS_Image list with inverse clipping
 *
 * Inverse clipping with the following strategy:
 * - find rectangle from (x0, y0) to (cx0, y1)
 * - find rectangle from (cx0, y0) to (cx1, cy0)
 * - find rectangle from (cx0, cy1) to (cx1, y1)
 * - find rectangle from (cx1, y0) to (x1, y1)
 * These rectangles can be invalid and in this case are discarded.
 * Afterwards, they are clipped against the screen coordinates.
 * In an additional pass, the rectangles need to be split up left/right for
 * karaoke effects.  This can result in a lot of bitmaps (6 to be exact).
 */
static ASS_Image **render_glyph_i(ASS_Renderer *render_priv,
                                  Bitmap *bm, int dst_x, int dst_y,
                                  uint32_t color, uint32_t color2, int brk,
                                  ASS_Image **tail)
{
    int i, j, x0, y0, x1, y1, cx0, cy0, cx1, cy1, sx, sy, zx, zy;
    Rect r[4];
    ASS_Image *img;

    dst_x += bm->left;
    dst_y += bm->top;

    // we still need to clip against screen boundaries
    zx = x2scr_pos(render_priv, 0);
    zy = y2scr_pos(render_priv, 0);
    sx = x2scr_pos(render_priv, render_priv->track->PlayResX);
    sy = y2scr_pos(render_priv, render_priv->track->PlayResY);

    x0 = 0;
    y0 = 0;
    x1 = bm->w;
    y1 = bm->h;
    cx0 = render_priv->state.clip_x0 - dst_x;
    cy0 = render_priv->state.clip_y0 - dst_y;
    cx1 = render_priv->state.clip_x1 - dst_x;
    cy1 = render_priv->state.clip_y1 - dst_y;

    // calculate rectangles and discard invalid ones while we're at it.
    i = 0;
    r[i].x0 = x0;
    r[i].y0 = y0;
    r[i].x1 = (cx0 > x1) ? x1 : cx0;
    r[i].y1 = y1;
    if (r[i].x1 > r[i].x0 && r[i].y1 > r[i].y0) i++;
    r[i].x0 = (cx0 < 0) ? x0 : cx0;
    r[i].y0 = y0;
    r[i].x1 = (cx1 > x1) ? x1 : cx1;
    r[i].y1 = (cy0 > y1) ? y1 : cy0;
    if (r[i].x1 > r[i].x0 && r[i].y1 > r[i].y0) i++;
    r[i].x0 = (cx0 < 0) ? x0 : cx0;
    r[i].y0 = (cy1 < 0) ? y0 : cy1;
    r[i].x1 = (cx1 > x1) ? x1 : cx1;
    r[i].y1 = y1;
    if (r[i].x1 > r[i].x0 && r[i].y1 > r[i].y0) i++;
    r[i].x0 = (cx1 < 0) ? x0 : cx1;
    r[i].y0 = y0;
    r[i].x1 = x1;
    r[i].y1 = y1;
    if (r[i].x1 > r[i].x0 && r[i].y1 > r[i].y0) i++;

    // clip each rectangle to screen coordinates
    for (j = 0; j < i; j++) {
        r[j].x0 = (r[j].x0 + dst_x < zx) ? zx - dst_x : r[j].x0;
        r[j].y0 = (r[j].y0 + dst_y < zy) ? zy - dst_y : r[j].y0;
        r[j].x1 = (r[j].x1 + dst_x > sx) ? sx - dst_x : r[j].x1;
        r[j].y1 = (r[j].y1 + dst_y > sy) ? sy - dst_y : r[j].y1;
    }

    // draw the rectangles
    for (j = 0; j < i; j++) {
        int lbrk = brk;
        // kick out rectangles that are invalid now
        if (r[j].x1 <= r[j].x0 || r[j].y1 <= r[j].y0)
            continue;
        // split up into left and right for karaoke, if needed
        if (lbrk > r[j].x0) {
            if (lbrk > r[j].x1) lbrk = r[j].x1;
            img = my_draw_bitmap(bm->buffer + r[j].y0 * bm->w + r[j].x0,
                lbrk - r[j].x0, r[j].y1 - r[j].y0,
                bm->w, dst_x + r[j].x0, dst_y + r[j].y0, color);
            *tail = img;
            tail = &img->next;
        }
        if (lbrk < r[j].x1) {
            if (lbrk < r[j].x0) lbrk = r[j].x0;
            img = my_draw_bitmap(bm->buffer + r[j].y0 * bm->w + lbrk,
                r[j].x1 - lbrk, r[j].y1 - r[j].y0,
                bm->w, dst_x + lbrk, dst_y + r[j].y0, color2);
            *tail = img;
            tail = &img->next;
        }
    }

    return tail;
}

/**
 * \brief convert bitmap glyph into ASS_Image struct(s)
 * \param bit freetype bitmap glyph, FT_PIXEL_MODE_GRAY
 * \param dst_x bitmap x coordinate in video frame
 * \param dst_y bitmap y coordinate in video frame
 * \param color first color, RGBA
 * \param color2 second color, RGBA
 * \param brk x coordinate relative to glyph origin, color is used to the left of brk, color2 - to the right
 * \param tail pointer to the last image's next field, head of the generated list should be stored here
 * \return pointer to the new list tail
 * Performs clipping. Uses my_draw_bitmap for actual bitmap convertion.
 */
static ASS_Image **
render_glyph(ASS_Renderer *render_priv, Bitmap *bm, int dst_x, int dst_y,
             uint32_t color, uint32_t color2, int brk, ASS_Image **tail)
{
    // Inverse clipping in use?
    if (render_priv->state.clip_mode)
        return render_glyph_i(render_priv, bm, dst_x, dst_y, color, color2,
                              brk, tail);

    // brk is relative to dst_x
    // color = color left of brk
    // color2 = color right of brk
    int b_x0, b_y0, b_x1, b_y1; // visible part of the bitmap
    int clip_x0, clip_y0, clip_x1, clip_y1;
    int tmp;
    ASS_Image *img;

    dst_x += bm->left;
    dst_y += bm->top;
    brk -= bm->left;

    // clipping
    clip_x0 = FFMINMAX(render_priv->state.clip_x0, 0, render_priv->width);
    clip_y0 = FFMINMAX(render_priv->state.clip_y0, 0, render_priv->height);
    clip_x1 = FFMINMAX(render_priv->state.clip_x1, 0, render_priv->width);
    clip_y1 = FFMINMAX(render_priv->state.clip_y1, 0, render_priv->height);
    b_x0 = 0;
    b_y0 = 0;
    b_x1 = bm->w;
    b_y1 = bm->h;

    tmp = dst_x - clip_x0;
    if (tmp < 0) {
        ass_msg(render_priv->library, MSGL_DBG2, "clip left");
        b_x0 = -tmp;
    }
    tmp = dst_y - clip_y0;
    if (tmp < 0) {
        ass_msg(render_priv->library, MSGL_DBG2, "clip top");
        b_y0 = -tmp;
    }
    tmp = clip_x1 - dst_x - bm->w;
    if (tmp < 0) {
        ass_msg(render_priv->library, MSGL_DBG2, "clip right");
        b_x1 = bm->w + tmp;
    }
    tmp = clip_y1 - dst_y - bm->h;
    if (tmp < 0) {
        ass_msg(render_priv->library, MSGL_DBG2, "clip bottom");
        b_y1 = bm->h + tmp;
    }

    if ((b_y0 >= b_y1) || (b_x0 >= b_x1))
        return tail;

    if (brk > b_x0) {           // draw left part
        if (brk > b_x1)
            brk = b_x1;
        img = my_draw_bitmap(bm->buffer + bm->w * b_y0 + b_x0,
                             brk - b_x0, b_y1 - b_y0, bm->w,
                             dst_x + b_x0, dst_y + b_y0, color);
        *tail = img;
        tail = &img->next;
    }
    if (brk < b_x1) {           // draw right part
        if (brk < b_x0)
            brk = b_x0;
        img = my_draw_bitmap(bm->buffer + bm->w * b_y0 + brk,
                             b_x1 - brk, b_y1 - b_y0, bm->w,
                             dst_x + brk, dst_y + b_y0, color2);
        *tail = img;
        tail = &img->next;
    }
    return tail;
}

/**
 * \brief Replace the bitmap buffer in ASS_Image with a copy
 * \param img ASS_Image to operate on
 * \return pointer to old bitmap buffer
 */
static unsigned char *clone_bitmap_buffer(ASS_Image *img)
{
    unsigned char *old_bitmap = img->bitmap;
    int size = img->stride * (img->h - 1) + img->w;
    img->bitmap = malloc(size);
    memcpy(img->bitmap, old_bitmap, size);
    return old_bitmap;
}

/**
 * \brief Calculate overlapping area of two consecutive bitmaps and in case they
 * overlap, blend them together
 * Mainly useful for translucent glyphs and especially borders, to avoid the
 * luminance adding up where they overlap (which looks ugly)
 */
static void
render_overlap(ASS_Renderer *render_priv, ASS_Image **last_tail,
               ASS_Image **tail)
{
    int left, top, bottom, right;
    int old_left, old_top, w, h, cur_left, cur_top;
    int x, y, opos, cpos;
    char m;
    CompositeHashKey hk;
    CompositeHashValue *hv;
    CompositeHashValue chv;
    int ax = (*last_tail)->dst_x;
    int ay = (*last_tail)->dst_y;
    int aw = (*last_tail)->w;
    int as = (*last_tail)->stride;
    int ah = (*last_tail)->h;
    int bx = (*tail)->dst_x;
    int by = (*tail)->dst_y;
    int bw = (*tail)->w;
    int bs = (*tail)->stride;
    int bh = (*tail)->h;
    unsigned char *a;
    unsigned char *b;

    if ((*last_tail)->bitmap == (*tail)->bitmap)
        return;

    if ((*last_tail)->color != (*tail)->color)
        return;

    // Calculate overlap coordinates
    left = (ax > bx) ? ax : bx;
    top = (ay > by) ? ay : by;
    right = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
    bottom = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);
    if ((right <= left) || (bottom <= top))
        return;
    old_left = left - ax;
    old_top = top - ay;
    w = right - left;
    h = bottom - top;
    cur_left = left - bx;
    cur_top = top - by;

    // Query cache
    memset(&hk, 0, sizeof(hk));
    hk.a = (*last_tail)->bitmap;
    hk.b = (*tail)->bitmap;
    hk.aw = aw;
    hk.ah = ah;
    hk.bw = bw;
    hk.bh = bh;
    hk.ax = ax;
    hk.ay = ay;
    hk.bx = bx;
    hk.by = by;
    hk.as = as;
    hk.bs = bs;
    hv = cache_find_composite(render_priv->cache.composite_cache, &hk);
    if (hv) {
        (*last_tail)->bitmap = hv->a;
        (*tail)->bitmap = hv->b;
        return;
    }
    // Allocate new bitmaps and copy over data
    a = clone_bitmap_buffer(*last_tail);
    b = clone_bitmap_buffer(*tail);

    // Blend overlapping area
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            opos = (old_top + y) * (as) + (old_left + x);
            cpos = (cur_top + y) * (bs) + (cur_left + x);
            m = FFMIN(a[opos] + b[cpos], 0xff);
            (*last_tail)->bitmap[opos] = 0;
            (*tail)->bitmap[cpos] = m;
        }

    // Insert bitmaps into the cache
    chv.a = (*last_tail)->bitmap;
    chv.b = (*tail)->bitmap;
    cache_add_composite(render_priv->cache.composite_cache, &hk, &chv);
}

static void free_list_add(ASS_Renderer *render_priv, void *object)
{
    if (!render_priv->free_head) {
        render_priv->free_head = calloc(1, sizeof(FreeList));
        render_priv->free_head->object = object;
        render_priv->free_tail = render_priv->free_head;
    } else {
        FreeList *l = calloc(1, sizeof(FreeList));
        l->object = object;
        render_priv->free_tail->next = l;
        render_priv->free_tail = render_priv->free_tail->next;
    }
}

/**
 * Iterate through a list of bitmaps and blend with clip vector, if
 * applicable. The blended bitmaps are added to a free list which is freed
 * at the start of a new frame.
 */
static void blend_vector_clip(ASS_Renderer *render_priv,
                              ASS_Image *head)
{
    FT_Glyph glyph;
    FT_BitmapGlyph clip_bm;
    ASS_Image *cur;
    ASS_Drawing *drawing = render_priv->state.clip_drawing;
    int error;

    if (!drawing)
        return;

    // Rasterize it
    FT_Glyph_Copy((FT_Glyph) drawing->glyph, &glyph);
    error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, 0, 1);
    if (error) {
        ass_msg(render_priv->library, MSGL_V,
            "Clip vector rasterization failed: %d. Skipping.", error);
        goto blend_vector_exit;
    }
    clip_bm = (FT_BitmapGlyph) glyph;
    clip_bm->top = -clip_bm->top;

    assert(clip_bm->bitmap.pitch >= 0);

    // Iterate through bitmaps and blend/clip them
    for (cur = head; cur; cur = cur->next) {
        int left, top, right, bottom, apos, bpos, y, x, w, h;
        int ax, ay, aw, ah, as;
        int bx, by, bw, bh, bs;
        int aleft, atop, bleft, btop;
        unsigned char *abuffer, *bbuffer, *nbuffer;

        abuffer = cur->bitmap;
        bbuffer = clip_bm->bitmap.buffer;
        ax = cur->dst_x;
        ay = cur->dst_y;
        aw = cur->w;
        ah = cur->h;
        as = cur->stride;
        bx = clip_bm->left;
        by = clip_bm->top;
        bw = clip_bm->bitmap.width;
        bh = clip_bm->bitmap.rows;
        bs = clip_bm->bitmap.pitch;

        // Calculate overlap coordinates
        left = (ax > bx) ? ax : bx;
        top = (ay > by) ? ay : by;
        right = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
        bottom = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);
        aleft = left - ax;
        atop = top - ay;
        w = right - left;
        h = bottom - top;
        bleft = left - bx;
        btop = top - by;

        if (render_priv->state.clip_drawing_mode) {
            // Inverse clip
            if (ax + aw < bx || ay + ah < by || ax > bx + bw ||
                ay > by + bh) {
                continue;
            }

            // Allocate new buffer and add to free list
            nbuffer = malloc(as * ah);
            free_list_add(render_priv, nbuffer);

            // Blend together
            memcpy(nbuffer, abuffer, as * (ah - 1) + aw);
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    apos = (atop + y) * as + aleft + x;
                    bpos = (btop + y) * bs + bleft + x;
                    nbuffer[apos] = FFMAX(0, abuffer[apos] - bbuffer[bpos]);
                }
        } else {
            // Regular clip
            if (ax + aw < bx || ay + ah < by || ax > bx + bw ||
                ay > by + bh) {
                cur->w = cur->h = 0;
                continue;
            }

            // Allocate new buffer and add to free list
            nbuffer = calloc(as, ah);
            free_list_add(render_priv, nbuffer);

            // Blend together
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    apos = (atop + y) * as + aleft + x;
                    bpos = (btop + y) * bs + bleft + x;
                    nbuffer[apos] = (abuffer[apos] * bbuffer[bpos] + 255) >> 8;
                }
        }
        cur->bitmap = nbuffer;
    }

    // Free clip vector and its bitmap, we don't need it anymore
    FT_Done_Glyph(glyph);
blend_vector_exit:
    ass_drawing_free(render_priv->state.clip_drawing);
    render_priv->state.clip_drawing = 0;
}

/**
 * \brief Convert TextInfo struct to ASS_Image list
 * Splits glyphs in halves when needed (for \kf karaoke).
 */
static ASS_Image *render_text(ASS_Renderer *render_priv, int dst_x,
                                int dst_y)
{
    int pen_x, pen_y;
    int i;
    Bitmap *bm;
    ASS_Image *head;
    ASS_Image **tail = &head;
    ASS_Image **last_tail = 0;
    ASS_Image **here_tail = 0;
    TextInfo *text_info = &render_priv->text_info;

    for (i = 0; i < text_info->length; ++i) {
        GlyphInfo *info = text_info->glyphs + i;
        if ((info->symbol == 0) || (info->symbol == '\n') || !info->bm_s
            || (info->shadow_x == 0 && info->shadow_y == 0) || info->skip)
            continue;

        pen_x =
            dst_x + (info->pos.x >> 6) +
            (int) (info->shadow_x * render_priv->border_scale);
        pen_y =
            dst_y + (info->pos.y >> 6) +
            (int) (info->shadow_y * render_priv->border_scale);
        bm = info->bm_s;

        here_tail = tail;
        tail =
            render_glyph(render_priv, bm, pen_x, pen_y, info->c[3], 0,
                         1000000, tail);
        if (last_tail && tail != here_tail && ((info->c[3] & 0xff) > 0))
            render_overlap(render_priv, last_tail, here_tail);

        last_tail = here_tail;
    }

    last_tail = 0;
    for (i = 0; i < text_info->length; ++i) {
        GlyphInfo *info = text_info->glyphs + i;
        if ((info->symbol == 0) || (info->symbol == '\n') || !info->bm_o
            || info->skip)
            continue;

        pen_x = dst_x + (info->pos.x >> 6);
        pen_y = dst_y + (info->pos.y >> 6);
        bm = info->bm_o;

        if ((info->effect_type == EF_KARAOKE_KO)
            && (info->effect_timing <= (info->bbox.xMax >> 6))) {
            // do nothing
        } else {
            here_tail = tail;
            tail =
                render_glyph(render_priv, bm, pen_x, pen_y, info->c[2],
                             0, 1000000, tail);
            if (last_tail && tail != here_tail && ((info->c[2] & 0xff) > 0))
                render_overlap(render_priv, last_tail, here_tail);

            last_tail = here_tail;
        }
    }

    for (i = 0; i < text_info->length; ++i) {
        GlyphInfo *info = text_info->glyphs + i;
        if ((info->symbol == 0) || (info->symbol == '\n') || !info->bm
            || info->skip)
            continue;

        pen_x = dst_x + (info->pos.x >> 6);
        pen_y = dst_y + (info->pos.y >> 6);
        bm = info->bm;

        if ((info->effect_type == EF_KARAOKE)
            || (info->effect_type == EF_KARAOKE_KO)) {
            if (info->effect_timing > (info->bbox.xMax >> 6))
                tail =
                    render_glyph(render_priv, bm, pen_x, pen_y,
                                 info->c[0], 0, 1000000, tail);
            else
                tail =
                    render_glyph(render_priv, bm, pen_x, pen_y,
                                 info->c[1], 0, 1000000, tail);
        } else if (info->effect_type == EF_KARAOKE_KF) {
            tail =
                render_glyph(render_priv, bm, pen_x, pen_y, info->c[0],
                             info->c[1], info->effect_timing, tail);
        } else
            tail =
                render_glyph(render_priv, bm, pen_x, pen_y, info->c[0],
                             0, 1000000, tail);
    }

    *tail = 0;
    blend_vector_clip(render_priv, head);

    return head;
}

/**
 * \brief Mapping between script and screen coordinates
 */
static double x2scr(ASS_Renderer *render_priv, double x)
{
    return x * render_priv->orig_width_nocrop /
        render_priv->track->PlayResX +
        FFMAX(render_priv->settings.left_margin, 0);
}
static double x2scr_pos(ASS_Renderer *render_priv, double x)
{
    return x * render_priv->orig_width / render_priv->track->PlayResX +
        render_priv->settings.left_margin;
}

/**
 * \brief Mapping between script and screen coordinates
 */
static double y2scr(ASS_Renderer *render_priv, double y)
{
    return y * render_priv->orig_height_nocrop /
        render_priv->track->PlayResY +
        FFMAX(render_priv->settings.top_margin, 0);
}
static double y2scr_pos(ASS_Renderer *render_priv, double y)
{
    return y * render_priv->orig_height / render_priv->track->PlayResY +
        render_priv->settings.top_margin;
}

// the same for toptitles
static double y2scr_top(ASS_Renderer *render_priv, double y)
{
    if (render_priv->settings.use_margins)
        return y * render_priv->orig_height_nocrop /
            render_priv->track->PlayResY;
    else
        return y * render_priv->orig_height_nocrop /
            render_priv->track->PlayResY +
            FFMAX(render_priv->settings.top_margin, 0);
}

// the same for subtitles
static double y2scr_sub(ASS_Renderer *render_priv, double y)
{
    if (render_priv->settings.use_margins)
        return y * render_priv->orig_height_nocrop /
            render_priv->track->PlayResY +
            FFMAX(render_priv->settings.top_margin,
                  0) + FFMAX(render_priv->settings.bottom_margin, 0);
    else
        return y * render_priv->orig_height_nocrop /
            render_priv->track->PlayResY +
            FFMAX(render_priv->settings.top_margin, 0);
}

static void compute_string_bbox(TextInfo *info, DBBox *bbox)
{
    int i;

    if (info->length > 0) {
        bbox->xMin = 32000;
        bbox->xMax = -32000;
        bbox->yMin = -1 * info->lines[0].asc + d6_to_double(info->glyphs[0].pos.y);
        bbox->yMax = info->height - info->lines[0].asc +
                     d6_to_double(info->glyphs[0].pos.y);

        for (i = 0; i < info->length; ++i) {
            if (info->glyphs[i].skip) continue;
            double s = d6_to_double(info->glyphs[i].pos.x);
            double e = s + d6_to_double(info->glyphs[i].advance.x);
            bbox->xMin = FFMIN(bbox->xMin, s);
            bbox->xMax = FFMAX(bbox->xMax, e);
        }
    } else
        bbox->xMin = bbox->xMax = bbox->yMin = bbox->yMax = 0.;
}

/**
 * \brief partially reset render_context to style values
 * Works like {\r}: resets some style overrides
 */
void reset_render_context(ASS_Renderer *render_priv)
{
    render_priv->state.c[0] = render_priv->state.style->PrimaryColour;
    render_priv->state.c[1] = render_priv->state.style->SecondaryColour;
    render_priv->state.c[2] = render_priv->state.style->OutlineColour;
    render_priv->state.c[3] = render_priv->state.style->BackColour;
    render_priv->state.flags =
        (render_priv->state.style->Underline ? DECO_UNDERLINE : 0) |
        (render_priv->state.style->StrikeOut ? DECO_STRIKETHROUGH : 0);
    render_priv->state.font_size = render_priv->state.style->FontSize;

    free(render_priv->state.family);
    render_priv->state.family = NULL;
    render_priv->state.family = strdup(render_priv->state.style->FontName);
    render_priv->state.treat_family_as_pattern =
        render_priv->state.style->treat_fontname_as_pattern;
    render_priv->state.bold = render_priv->state.style->Bold;
    render_priv->state.italic = render_priv->state.style->Italic;
    update_font(render_priv);

    change_border(render_priv, -1., -1.);
    render_priv->state.scale_x = render_priv->state.style->ScaleX;
    render_priv->state.scale_y = render_priv->state.style->ScaleY;
    render_priv->state.hspacing = render_priv->state.style->Spacing;
    render_priv->state.be = 0;
    render_priv->state.blur = 0.0;
    render_priv->state.shadow_x = render_priv->state.style->Shadow;
    render_priv->state.shadow_y = render_priv->state.style->Shadow;
    render_priv->state.frx = render_priv->state.fry = 0.;
    render_priv->state.frz = M_PI * render_priv->state.style->Angle / 180.;
    render_priv->state.fax = render_priv->state.fay = 0.;
    render_priv->state.wrap_style = render_priv->track->WrapStyle;

    // FIXME: does not reset unsupported attributes.
}

/**
 * \brief Start new event. Reset render_priv->state.
 */
static void
init_render_context(ASS_Renderer *render_priv, ASS_Event *event)
{
    render_priv->state.event = event;
    render_priv->state.style = render_priv->track->styles + event->Style;

    reset_render_context(render_priv);

    render_priv->state.evt_type = EVENT_NORMAL;
    render_priv->state.alignment = render_priv->state.style->Alignment;
    render_priv->state.pos_x = 0;
    render_priv->state.pos_y = 0;
    render_priv->state.org_x = 0;
    render_priv->state.org_y = 0;
    render_priv->state.have_origin = 0;
    render_priv->state.clip_x0 = 0;
    render_priv->state.clip_y0 = 0;
    render_priv->state.clip_x1 = render_priv->track->PlayResX;
    render_priv->state.clip_y1 = render_priv->track->PlayResY;
    render_priv->state.clip_mode = 0;
    render_priv->state.detect_collisions = 1;
    render_priv->state.fade = 0;
    render_priv->state.drawing_mode = 0;
    render_priv->state.effect_type = EF_NONE;
    render_priv->state.effect_timing = 0;
    render_priv->state.effect_skip_timing = 0;
    render_priv->state.drawing =
        ass_drawing_new(render_priv->fontconfig_priv,
                        render_priv->state.font,
                        render_priv->settings.hinting,
                        render_priv->ftlibrary);

    apply_transition_effects(render_priv, event);
}

static void free_render_context(ASS_Renderer *render_priv)
{
    free(render_priv->state.family);
    ass_drawing_free(render_priv->state.drawing);

    render_priv->state.family = NULL;
    render_priv->state.drawing = NULL;
}

// Calculate the cbox of a series of points
static void
get_contour_cbox(FT_BBox *box, FT_Vector *points, int start, int end)
{
    box->xMin = box->yMin = INT_MAX;
    box->xMax = box->yMax = INT_MIN;
    int i;

    for (i = start; i < end; i++) {
        box->xMin = (points[i].x < box->xMin) ? points[i].x : box->xMin;
        box->xMax = (points[i].x > box->xMax) ? points[i].x : box->xMax;
        box->yMin = (points[i].y < box->yMin) ? points[i].y : box->yMin;
        box->yMax = (points[i].y > box->yMax) ? points[i].y : box->yMax;
    }
}

/**
 * \brief Fix-up stroker result for huge borders by removing the contours from
 * the outline that are harmful.
*/
static void fix_freetype_stroker(FT_OutlineGlyph glyph, int border_x,
                                 int border_y)
{
    int nc = glyph->outline.n_contours;
    int begin, stop;
    char modified = 0;
    char *valid_cont;
    int start = 0;
    int end = -1;
    FT_BBox *boxes = calloc(nc, sizeof(FT_BBox));
    int i, j;

    // Create a list of cboxes of the contours
    for (i = 0; i < nc; i++) {
        start = end + 1;
        end = glyph->outline.contours[i];
        get_contour_cbox(&boxes[i], glyph->outline.points, start, end);
    }

    // if a) contour's cbox is contained in another contours cbox
    //    b) contour's height or width is smaller than the border*2
    // the contour can be safely removed.
    valid_cont = calloc(1, nc);
    for (i = 0; i < nc; i++) {
        valid_cont[i] = 1;
        for (j = 0; j < nc; j++) {
            if (i == j)
                continue;
            if (boxes[i].xMin >= boxes[j].xMin &&
                boxes[i].xMax <= boxes[j].xMax &&
                boxes[i].yMin >= boxes[j].yMin &&
                boxes[i].yMax <= boxes[j].yMax) {
                int width = boxes[i].xMax - boxes[i].xMin;
                int height = boxes[i].yMax - boxes[i].yMin;
                if (width < border_x * 2 || height < border_y * 2) {
                    valid_cont[i] = 0;
                    modified = 1;
                    break;
                }
            }
        }
    }

    // Zero-out contours that can be removed; much simpler than copying
    if (modified) {
        for (i = 0; i < nc; i++) {
            if (valid_cont[i])
                continue;
            begin = (i == 0) ? 0 : glyph->outline.contours[i - 1] + 1;
            stop = glyph->outline.contours[i];
            for (j = begin; j <= stop; j++) {
                glyph->outline.points[j].x = 0;
                glyph->outline.points[j].y = 0;
                glyph->outline.tags[j] = 0;
            }
        }
    }

    free(boxes);
    free(valid_cont);
}

/*
 * Replace the outline of a glyph by a contour which makes up a simple
 * opaque rectangle.
 */
static void draw_opaque_box(ASS_Renderer *render_priv, uint32_t ch,
                            FT_Glyph glyph, int sx, int sy)
{
    int asc = 0, desc = 0;
    int i;
    int adv = d16_to_d6(glyph->advance.x);
    double scale_y = render_priv->state.scale_y;
    double scale_x = render_priv->state.scale_x
                     * render_priv->font_scale_x;
    FT_OutlineGlyph og = (FT_OutlineGlyph) glyph;
    FT_Outline *ol;

    // to avoid gaps
    sx = FFMAX(64, sx);
    sy = FFMAX(64, sy);

    if (ch == -1) {
        asc = render_priv->state.drawing->asc;
        desc = render_priv->state.drawing->desc;
    } else {
        ass_font_get_asc_desc(render_priv->state.font, ch, &asc, &desc);
        asc  *= scale_y;
        desc *= scale_y;
    }

    // Emulate the WTFish behavior of VSFilter, i.e. double-scale
    // the sizes of the opaque box.
    adv += double_to_d6(render_priv->state.hspacing * render_priv->font_scale
                        * scale_x);
    adv *= scale_x;
    sx *= scale_x;
    sy *= scale_y;
    desc *= scale_y;
    desc += asc * (scale_y - 1.0);

    FT_Vector points[4] = {
        { .x = -sx,         .y = asc + sy },
        { .x = adv + sx,    .y = asc + sy },
        { .x = adv + sx,    .y = -desc - sy },
        { .x = -sx,         .y = -desc - sy },
    };

    FT_Outline_Done(render_priv->ftlibrary, &og->outline);
    FT_Outline_New(render_priv->ftlibrary, 4, 1, &og->outline);

    ol = &og->outline;
    ol->n_points = ol->n_contours = 0;
    for (i = 0; i < 4; i++) {
        ol->points[ol->n_points] = points[i];
        ol->tags[ol->n_points++] = 1;
    }
    ol->contours[ol->n_contours++] = ol->n_points - 1;
}

/*
 * Stroke an outline glyph in x/y direction.  Applies various fixups to get
 * around limitations of the FreeType stroker.
 */
static void stroke_outline_glyph(ASS_Renderer *render_priv,
                                 FT_OutlineGlyph *glyph, int sx, int sy)
{
    if (sx <= 0 && sy <= 0)
        return;

    fix_freetype_stroker(*glyph, sx, sy);

    // Borders are equal; use the regular stroker
    if (sx == sy && render_priv->state.stroker) {
        int error;
        error = FT_Glyph_StrokeBorder((FT_Glyph *) glyph,
                                      render_priv->state.stroker, 0, 1);
        if (error)
            ass_msg(render_priv->library, MSGL_WARN,
                    "FT_Glyph_Stroke error: %d", error);

    // "Stroke" with the outline emboldener in two passes.
    // The outlines look uglier, but the emboldening never adds any points
    } else {
        int i;
        FT_Outline *ol = &(*glyph)->outline;
        FT_Outline nol;
        FT_Outline_New(render_priv->ftlibrary, ol->n_points,
                       ol->n_contours, &nol);
        FT_Outline_Copy(ol, &nol);

        FT_Outline_Embolden(ol, sx * 2);
        FT_Outline_Translate(ol, -sx, -sx);
        FT_Outline_Embolden(&nol, sy * 2);
        FT_Outline_Translate(&nol, -sy, -sy);

        for (i = 0; i < ol->n_points; i++)
            ol->points[i].y = nol.points[i].y;

        FT_Outline_Done(render_priv->ftlibrary, &nol);
    }
}

/**
 * \brief Get normal and outline (border) glyphs
 * \param symbol ucs4 char
 * \param info out: struct filled with extracted data
 * Tries to get both glyphs from cache.
 * If they can't be found, gets a glyph from font face, generates outline with FT_Stroker,
 * and add them to cache.
 * The glyphs are returned in info->glyph and info->outline_glyph
 */
static void
get_outline_glyph(ASS_Renderer *render_priv, int symbol, GlyphInfo *info,
                  ASS_Drawing *drawing)
{
    GlyphHashValue *val;
    GlyphHashKey key;
    memset(&key, 0, sizeof(key));

    if (drawing->hash) {
        key.scale_x = double_to_d16(render_priv->state.scale_x);
        key.scale_y = double_to_d16(render_priv->state.scale_y);
        key.outline.x = render_priv->state.border_x * 0xFFFF;
        key.outline.y = render_priv->state.border_y * 0xFFFF;
        key.border_style = render_priv->state.style->BorderStyle;
        key.drawing_hash = drawing->hash;
    } else {
        key.font = render_priv->state.font;
        key.size = render_priv->state.font_size;
        key.ch = symbol;
        key.bold = render_priv->state.bold;
        key.italic = render_priv->state.italic;
        key.scale_x = double_to_d16(render_priv->state.scale_x);
        key.scale_y = double_to_d16(render_priv->state.scale_y);
        key.outline.x = render_priv->state.border_x * 0xFFFF;
        key.outline.y = render_priv->state.border_y * 0xFFFF;
        key.flags = render_priv->state.flags;
        key.border_style = render_priv->state.style->BorderStyle;
    }
    memset(info, 0, sizeof(GlyphInfo));

    val = cache_find_glyph(render_priv->cache.glyph_cache, &key);
    if (val) {
        FT_Glyph_Copy(val->glyph, &info->glyph);
        if (val->outline_glyph)
            FT_Glyph_Copy(val->outline_glyph, &info->outline_glyph);
        info->bbox = val->bbox_scaled;
        info->advance.x = val->advance.x;
        info->advance.y = val->advance.y;
        if (drawing->hash) {
            drawing->asc = val->asc;
            drawing->desc = val->desc;
        }
    } else {
        GlyphHashValue v;
        if (drawing->hash) {
            if(!ass_drawing_parse(drawing, 0))
                return;
            FT_Glyph_Copy((FT_Glyph) drawing->glyph, &info->glyph);
        } else {
            info->glyph =
                ass_font_get_glyph(render_priv->fontconfig_priv,
                                   render_priv->state.font, symbol,
                                   render_priv->settings.hinting,
                                   render_priv->state.flags);
        }
        if (!info->glyph)
            return;
        info->advance.x = d16_to_d6(info->glyph->advance.x);
        info->advance.y = d16_to_d6(info->glyph->advance.y);
        FT_Glyph_Get_CBox(info->glyph, FT_GLYPH_BBOX_SUBPIXELS, &info->bbox);

        if (render_priv->state.style->BorderStyle == 3 &&
            (render_priv->state.border_x > 0||
             render_priv->state.border_y > 0)) {
            FT_Glyph_Copy(info->glyph, &info->outline_glyph);
            draw_opaque_box(render_priv, symbol, info->outline_glyph,
                            double_to_d6(render_priv->state.border_x *
                                         render_priv->border_scale),
                            double_to_d6(render_priv->state.border_y *
                                         render_priv->border_scale));
        } else if (render_priv->state.border_x > 0 ||
                   render_priv->state.border_y > 0) {

            FT_Glyph_Copy(info->glyph, &info->outline_glyph);
            stroke_outline_glyph(render_priv,
                                 (FT_OutlineGlyph *) &info->outline_glyph,
                                 double_to_d6(render_priv->state.border_x *
                                              render_priv->border_scale),
                                 double_to_d6(render_priv->state.border_y *
                                              render_priv->border_scale));
        }

        memset(&v, 0, sizeof(v));
        FT_Glyph_Copy(info->glyph, &v.glyph);
        if (info->outline_glyph)
            FT_Glyph_Copy(info->outline_glyph, &v.outline_glyph);
        v.advance = info->advance;
        v.bbox_scaled = info->bbox;
        if (drawing->hash) {
            v.asc = drawing->asc;
            v.desc = drawing->desc;
        }
        cache_add_glyph(render_priv->cache.glyph_cache, &key, &v);
    }
}

static void transform_3d(FT_Vector shift, FT_Glyph *glyph,
                         FT_Glyph *glyph2, double frx, double fry,
                         double frz, double fax, double fay, double scale,
                         int yshift);

/**
 * \brief Get bitmaps for a glyph
 * \param info glyph info
 * Tries to get glyph bitmaps from bitmap cache.
 * If they can't be found, they are generated by rotating and rendering the glyph.
 * After that, bitmaps are added to the cache.
 * They are returned in info->bm (glyph), info->bm_o (outline) and info->bm_s (shadow).
 */
static void
get_bitmap_glyph(ASS_Renderer *render_priv, GlyphInfo *info)
{
    BitmapHashValue *val;
    BitmapHashKey *key = &info->hash_key;

    val = cache_find_bitmap(render_priv->cache.bitmap_cache, key);

    if (val) {
        info->bm = val->bm;
        info->bm_o = val->bm_o;
        info->bm_s = val->bm_s;
    } else {
        FT_Vector shift;
        BitmapHashValue hash_val;
        int error;
        double fax_scaled, fay_scaled;
        info->bm = info->bm_o = info->bm_s = 0;
        if (info->glyph && info->symbol != '\n' && info->symbol != 0
            && !info->skip) {
            // calculating rotation shift vector (from rotation origin to the glyph basepoint)
            shift.x = info->hash_key.shift_x;
            shift.y = info->hash_key.shift_y;
            fax_scaled = info->fax * render_priv->font_scale_x *
                         render_priv->state.scale_x;
            fay_scaled = info->fay * render_priv->state.scale_y;
            // apply rotation
            transform_3d(shift, &info->glyph, &info->outline_glyph,
                         info->frx, info->fry, info->frz, fax_scaled,
                         fay_scaled, render_priv->font_scale, info->asc);

            // subpixel shift
            if (info->glyph)
                FT_Outline_Translate(
                    &((FT_OutlineGlyph) info->glyph)->outline,
                    info->hash_key.advance.x,
                    -info->hash_key.advance.y);
            if (info->outline_glyph)
                FT_Outline_Translate(
                    &((FT_OutlineGlyph) info->outline_glyph)->outline,
                    info->hash_key.advance.x,
                    -info->hash_key.advance.y);

            // render glyph
            error = glyph_to_bitmap(render_priv->library,
                                    render_priv->synth_priv,
                                    info->glyph, info->outline_glyph,
                                    &info->bm, &info->bm_o,
                                    &info->bm_s, info->be,
                                    info->blur * render_priv->border_scale,
                                    info->hash_key.shadow_offset,
                                    info->hash_key.border_style);
            if (error)
                info->symbol = 0;

            // add bitmaps to cache
            hash_val.bm_o = info->bm_o;
            hash_val.bm = info->bm;
            hash_val.bm_s = info->bm_s;
            cache_add_bitmap(render_priv->cache.bitmap_cache,
                             &(info->hash_key), &hash_val);
        }
    }
    // deallocate glyphs
    if (info->glyph)
        FT_Done_Glyph(info->glyph);
    if (info->outline_glyph)
        FT_Done_Glyph(info->outline_glyph);
}

/**
 * This function goes through text_info and calculates text parameters.
 * The following text_info fields are filled:
 *   height
 *   lines[].height
 *   lines[].asc
 *   lines[].desc
 */
static void measure_text(ASS_Renderer *render_priv)
{
    TextInfo *text_info = &render_priv->text_info;
    int cur_line = 0;
    double max_asc = 0., max_desc = 0.;
    GlyphInfo *last = NULL;
    int i;
    int empty_line = 1;
    text_info->height = 0.;
    for (i = 0; i < text_info->length + 1; ++i) {
        if ((i == text_info->length) || text_info->glyphs[i].linebreak) {
            if (empty_line && cur_line > 0 && last && i < text_info->length) {
                max_asc = d6_to_double(last->asc) / 2.0;
                max_desc = d6_to_double(last->desc) / 2.0;
            }
            text_info->lines[cur_line].asc = max_asc;
            text_info->lines[cur_line].desc = max_desc;
            text_info->height += max_asc + max_desc;
            cur_line++;
            max_asc = max_desc = 0.;
            empty_line = 1;
        } else
            empty_line = 0;
        if (i < text_info->length) {
            GlyphInfo *cur = text_info->glyphs + i;
            if (d6_to_double(cur->asc) > max_asc)
                max_asc = d6_to_double(cur->asc);
            if (d6_to_double(cur->desc) > max_desc)
                max_desc = d6_to_double(cur->desc);
            if (cur->symbol != '\n' && cur->symbol != 0)
                last = cur;
        }
    }
    text_info->height +=
        (text_info->n_lines -
         1) * render_priv->settings.line_spacing;
}

/**
 * Mark extra whitespace for later removal.
 */
#define IS_WHITESPACE(x) ((x->symbol == ' ' || x->symbol == '\n') \
                          && !x->linebreak)
static void trim_whitespace(ASS_Renderer *render_priv)
{
    int i, j;
    GlyphInfo *cur;
    TextInfo *ti = &render_priv->text_info;

    // Mark trailing spaces
    i = ti->length - 1;
    cur = ti->glyphs + i;
    while (i && IS_WHITESPACE(cur)) {
        cur->skip++;
        cur = ti->glyphs + --i;
    }

    // Mark leading whitespace
    i = 0;
    cur = ti->glyphs;
    while (i < ti->length && IS_WHITESPACE(cur)) {
        cur->skip++;
        cur = ti->glyphs + ++i;
    }

    // Mark all extraneous whitespace inbetween
    for (i = 0; i < ti->length; ++i) {
        cur = ti->glyphs + i;
        if (cur->linebreak) {
            // Mark whitespace before
            j = i - 1;
            cur = ti->glyphs + j;
            while (j && IS_WHITESPACE(cur)) {
                cur->skip++;
                cur = ti->glyphs + --j;
            }
            // A break itself can contain a whitespace, too
            cur = ti->glyphs + i;
            if (cur->symbol == ' ')
                cur->skip++;
            // Mark whitespace after
            j = i + 1;
            cur = ti->glyphs + j;
            while (j < ti->length && IS_WHITESPACE(cur)) {
                cur->skip++;
                cur = ti->glyphs + ++j;
            }
            i = j - 1;
        }
    }
}
#undef IS_WHITESPACE

/**
 * \brief rearrange text between lines
 * \param max_text_width maximal text line width in pixels
 * The algo is similar to the one in libvo/sub.c:
 * 1. Place text, wrapping it when current line is full
 * 2. Try moving words from the end of a line to the beginning of the next one while it reduces
 * the difference in lengths between this two lines.
 * The result may not be optimal, but usually is good enough.
 *
 * FIXME: implement style 0 and 3 correctly, add support for style 1
 */
static void
wrap_lines_smart(ASS_Renderer *render_priv, double max_text_width)
{
    int i;
    GlyphInfo *cur, *s1, *e1, *s2, *s3, *w;
    int last_space;
    int break_type;
    int exit;
    double pen_shift_x;
    double pen_shift_y;
    int cur_line;
    TextInfo *text_info = &render_priv->text_info;

    last_space = -1;
    text_info->n_lines = 1;
    break_type = 0;
    s1 = text_info->glyphs;     // current line start
    for (i = 0; i < text_info->length; ++i) {
        int break_at;
        double s_offset, len;
        cur = text_info->glyphs + i;
        break_at = -1;
        s_offset = d6_to_double(s1->bbox.xMin + s1->pos.x);
        len = d6_to_double(cur->bbox.xMax + cur->pos.x) - s_offset;

        if (cur->symbol == '\n') {
            break_type = 2;
            break_at = i;
            ass_msg(render_priv->library, MSGL_DBG2,
                    "forced line break at %d", break_at);
        }

        if ((len >= max_text_width)
            && (render_priv->state.wrap_style != 2)) {
            break_type = 1;
            break_at = last_space;
            if (break_at == -1)
                break_at = i - 1;
            if (break_at == -1)
                break_at = 0;
            ass_msg(render_priv->library, MSGL_DBG2, "overfill at %d", i);
            ass_msg(render_priv->library, MSGL_DBG2, "line break at %d",
                    break_at);
        }

        if (break_at != -1) {
            // need to use one more line
            // marking break_at+1 as start of a new line
            int lead = break_at + 1;    // the first symbol of the new line
            if (text_info->n_lines >= text_info->max_lines) {
                // Raise maximum number of lines
                text_info->max_lines *= 2;
                text_info->lines = realloc(text_info->lines,
                                           sizeof(LineInfo) *
                                           text_info->max_lines);
            }
            if (lead < text_info->length)
                text_info->glyphs[lead].linebreak = break_type;
            last_space = -1;
            s1 = text_info->glyphs + lead;
            s_offset = d6_to_double(s1->bbox.xMin + s1->pos.x);
            text_info->n_lines++;
        }

        if (cur->symbol == ' ')
            last_space = i;

        // make sure the hard linebreak is not forgotten when
        // there was a new soft linebreak just inserted
        if (cur->symbol == '\n' && break_type == 1)
            i--;
    }
#define DIFF(x,y) (((x) < (y)) ? (y - x) : (x - y))
    exit = 0;
    while (!exit && render_priv->state.wrap_style != 1) {
        exit = 1;
        w = s3 = text_info->glyphs;
        s1 = s2 = 0;
        for (i = 0; i <= text_info->length; ++i) {
            cur = text_info->glyphs + i;
            if ((i == text_info->length) || cur->linebreak) {
                s1 = s2;
                s2 = s3;
                s3 = cur;
                if (s1 && (s2->linebreak == 1)) {       // have at least 2 lines, and linebreak is 'soft'
                    double l1, l2, l1_new, l2_new;

                    w = s2;
                    do {
                        --w;
                    } while ((w > s1) && (w->symbol == ' '));
                    while ((w > s1) && (w->symbol != ' ')) {
                        --w;
                    }
                    e1 = w;
                    while ((e1 > s1) && (e1->symbol == ' ')) {
                        --e1;
                    }
                    if (w->symbol == ' ')
                        ++w;

                    l1 = d6_to_double(((s2 - 1)->bbox.xMax + (s2 - 1)->pos.x) -
                        (s1->bbox.xMin + s1->pos.x));
                    l2 = d6_to_double(((s3 - 1)->bbox.xMax + (s3 - 1)->pos.x) -
                        (s2->bbox.xMin + s2->pos.x));
                    l1_new = d6_to_double(
                        (e1->bbox.xMax + e1->pos.x) -
                        (s1->bbox.xMin + s1->pos.x));
                    l2_new = d6_to_double(
                        ((s3 - 1)->bbox.xMax + (s3 - 1)->pos.x) -
                        (w->bbox.xMin + w->pos.x));

                    if (DIFF(l1_new, l2_new) < DIFF(l1, l2)) {
                        w->linebreak = 1;
                        s2->linebreak = 0;
                        exit = 0;
                    }
                }
            }
            if (i == text_info->length)
                break;
        }

    }
    assert(text_info->n_lines >= 1);
#undef DIFF

    measure_text(render_priv);
    trim_whitespace(render_priv);

    pen_shift_x = 0.;
    pen_shift_y = 0.;
    cur_line = 1;

    i = 0;
    cur = text_info->glyphs + i;
    while (i < text_info->length && cur->skip)
        cur = text_info->glyphs + ++i;
    pen_shift_x = d6_to_double(-cur->pos.x);

    for (i = 0; i < text_info->length; ++i) {
        cur = text_info->glyphs + i;
        if (cur->linebreak) {
            while (i < text_info->length && cur->skip && cur->symbol != '\n')
                cur = text_info->glyphs + ++i;
            double height =
                text_info->lines[cur_line - 1].desc +
                text_info->lines[cur_line].asc;
            cur_line++;
            pen_shift_x = d6_to_double(-cur->pos.x);
            pen_shift_y += height + render_priv->settings.line_spacing;
            ass_msg(render_priv->library, MSGL_DBG2,
                   "shifting from %d to %d by (%f, %f)", i,
                   text_info->length - 1, pen_shift_x, pen_shift_y);
        }
        cur->pos.x += double_to_d6(pen_shift_x);
        cur->pos.y += double_to_d6(pen_shift_y);
    }
}

/**
 * \brief determine karaoke effects
 * Karaoke effects cannot be calculated during parse stage (get_next_char()),
 * so they are done in a separate step.
 * Parse stage: when karaoke style override is found, its parameters are stored in the next glyph's
 * (the first glyph of the karaoke word)'s effect_type and effect_timing.
 * This function:
 * 1. sets effect_type for all glyphs in the word (_karaoke_ word)
 * 2. sets effect_timing for all glyphs to x coordinate of the border line between the left and right karaoke parts
 * (left part is filled with PrimaryColour, right one - with SecondaryColour).
 */
static void process_karaoke_effects(ASS_Renderer *render_priv)
{
    GlyphInfo *cur, *cur2;
    GlyphInfo *s1, *e1;      // start and end of the current word
    GlyphInfo *s2;           // start of the next word
    int i;
    int timing;                 // current timing
    int tm_start, tm_end;       // timings at start and end of the current word
    int tm_current;
    double dt;
    int x;
    int x_start, x_end;

    tm_current = render_priv->time - render_priv->state.event->Start;
    timing = 0;
    s1 = s2 = 0;
    for (i = 0; i <= render_priv->text_info.length; ++i) {
        cur = render_priv->text_info.glyphs + i;
        if ((i == render_priv->text_info.length)
            || (cur->effect_type != EF_NONE)) {
            s1 = s2;
            s2 = cur;
            if (s1) {
                e1 = s2 - 1;
                tm_start = timing + s1->effect_skip_timing;
                tm_end = tm_start + s1->effect_timing;
                timing = tm_end;
                x_start = 1000000;
                x_end = -1000000;
                for (cur2 = s1; cur2 <= e1; ++cur2) {
                    x_start = FFMIN(x_start, d6_to_int(cur2->bbox.xMin + cur2->pos.x));
                    x_end = FFMAX(x_end, d6_to_int(cur2->bbox.xMax + cur2->pos.x));
                }

                dt = (tm_current - tm_start);
                if ((s1->effect_type == EF_KARAOKE)
                    || (s1->effect_type == EF_KARAOKE_KO)) {
                    if (dt > 0)
                        x = x_end + 1;
                    else
                        x = x_start;
                } else if (s1->effect_type == EF_KARAOKE_KF) {
                    dt /= (tm_end - tm_start);
                    x = x_start + (x_end - x_start) * dt;
                } else {
                    ass_msg(render_priv->library, MSGL_ERR,
                            "Unknown effect type");
                    continue;
                }

                for (cur2 = s1; cur2 <= e1; ++cur2) {
                    cur2->effect_type = s1->effect_type;
                    cur2->effect_timing = x - d6_to_int(cur2->pos.x);
                }
            }
        }
    }
}

/**
 * \brief Calculate base point for positioning and rotation
 * \param bbox text bbox
 * \param alignment alignment
 * \param bx, by out: base point coordinates
 */
static void get_base_point(DBBox *bbox, int alignment, double *bx, double *by)
{
    const int halign = alignment & 3;
    const int valign = alignment & 12;
    if (bx)
        switch (halign) {
        case HALIGN_LEFT:
            *bx = bbox->xMin;
            break;
        case HALIGN_CENTER:
            *bx = (bbox->xMax + bbox->xMin) / 2.0;
            break;
        case HALIGN_RIGHT:
            *bx = bbox->xMax;
            break;
        }
    if (by)
        switch (valign) {
        case VALIGN_TOP:
            *by = bbox->yMin;
            break;
        case VALIGN_CENTER:
            *by = (bbox->yMax + bbox->yMin) / 2.0;
            break;
        case VALIGN_SUB:
            *by = bbox->yMax;
            break;
        }
}

/**
 * \brief Apply transformation to outline points of a glyph
 * Applies rotations given by frx, fry and frz and projects the points back
 * onto the screen plane.
 */
static void
transform_3d_points(FT_Vector shift, FT_Glyph glyph, double frx, double fry,
                    double frz, double fax, double fay, double scale,
                    int yshift)
{
    double sx = sin(frx);
    double sy = sin(fry);
    double sz = sin(frz);
    double cx = cos(frx);
    double cy = cos(fry);
    double cz = cos(frz);
    FT_Outline *outline = &((FT_OutlineGlyph) glyph)->outline;
    FT_Vector *p = outline->points;
    double x, y, z, xx, yy, zz;
    int i, dist;

    dist = 20000 * scale;
    for (i = 0; i < outline->n_points; i++) {
        x = (double) p[i].x + shift.x + (fax * (yshift - p[i].y));
        y = (double) p[i].y + shift.y + (-fay * p[i].x);
        z = 0.;

        xx = x * cz + y * sz;
        yy = -(x * sz - y * cz);
        zz = z;

        x = xx;
        y = yy * cx + zz * sx;
        z = yy * sx - zz * cx;

        xx = x * cy + z * sy;
        yy = y;
        zz = x * sy - z * cy;

        zz = FFMAX(zz, 1000 - dist);

        x = (xx * dist) / (zz + dist);
        y = (yy * dist) / (zz + dist);
        p[i].x = x - shift.x + 0.5;
        p[i].y = y - shift.y + 0.5;
    }
}

/**
 * \brief Apply 3d transformation to several objects
 * \param shift FreeType vector
 * \param glyph FreeType glyph
 * \param glyph2 FreeType glyph
 * \param frx x-axis rotation angle
 * \param fry y-axis rotation angle
 * \param frz z-axis rotation angle
 * Rotates both glyphs by frx, fry and frz. Shift vector is added before rotation and subtracted after it.
 */
static void
transform_3d(FT_Vector shift, FT_Glyph *glyph, FT_Glyph *glyph2,
             double frx, double fry, double frz, double fax, double fay,
             double scale, int yshift)
{
    frx = -frx;
    frz = -frz;
    if (frx != 0. || fry != 0. || frz != 0. || fax != 0. || fay != 0.) {
        if (glyph && *glyph)
            transform_3d_points(shift, *glyph, frx, fry, frz,
                                fax, fay, scale, yshift);

        if (glyph2 && *glyph2)
            transform_3d_points(shift, *glyph2, frx, fry, frz,
                                fax, fay, scale, yshift);
    }
}


/**
 * \brief Main ass rendering function, glues everything together
 * \param event event to render
 * \param event_images struct containing resulting images, will also be initialized
 * Process event, appending resulting ASS_Image's to images_root.
 */
static int
ass_render_event(ASS_Renderer *render_priv, ASS_Event *event,
                 EventImages *event_images)
{
    char *p;
    FT_UInt previous;
    FT_UInt num_glyphs;
    FT_Vector pen;
    unsigned code;
    DBBox bbox;
    int i, j;
    int MarginL, MarginR, MarginV;
    int last_break;
    int alignment, halign, valign;
    int kern = render_priv->track->Kerning;
    double device_x = 0;
    double device_y = 0;
    TextInfo *text_info = &render_priv->text_info;
    ASS_Drawing *drawing;

    if (event->Style >= render_priv->track->n_styles) {
        ass_msg(render_priv->library, MSGL_WARN, "No style found");
        return 1;
    }
    if (!event->Text) {
        ass_msg(render_priv->library, MSGL_WARN, "Empty event");
        return 1;
    }

    init_render_context(render_priv, event);

    drawing = render_priv->state.drawing;
    text_info->length = 0;
    pen.x = 0;
    pen.y = 0;
    previous = 0;
    num_glyphs = 0;
    p = event->Text;
    // Event parsing.
    while (1) {
        // get next char, executing style override
        // this affects render_context
        do {
            code = get_next_char(render_priv, &p);
            if (render_priv->state.drawing_mode && code)
                ass_drawing_add_char(drawing, (char) code);
        } while (code && render_priv->state.drawing_mode);      // skip everything in drawing mode

        // Parse drawing
        if (drawing->i) {
            drawing->scale_x = render_priv->state.scale_x *
                                     render_priv->font_scale_x *
                                     render_priv->font_scale;
            drawing->scale_y = render_priv->state.scale_y *
                                     render_priv->font_scale;
            ass_drawing_hash(drawing);
            p--;
            code = -1;
        }

        // face could have been changed in get_next_char
        if (!render_priv->state.font) {
            free_render_context(render_priv);
            return 1;
        }

        if (code == 0)
            break;

        if (text_info->length >= text_info->max_glyphs) {
            // Raise maximum number of glyphs
            text_info->max_glyphs *= 2;
            text_info->glyphs =
                realloc(text_info->glyphs,
                        sizeof(GlyphInfo) * text_info->max_glyphs);
        }

        // Add kerning to pen
        if (kern && previous && code && !drawing->hash) {
            FT_Vector delta;
            delta =
                ass_font_get_kerning(render_priv->state.font, previous,
                                     code);
            pen.x += delta.x * render_priv->state.scale_x
                     * render_priv->font_scale_x;
            pen.y += delta.y * render_priv->state.scale_y
                     * render_priv->font_scale_x;
        }

        ass_font_set_transform(render_priv->state.font,
                               render_priv->state.scale_x *
                               render_priv->font_scale_x,
                               render_priv->state.scale_y, NULL);

        get_outline_glyph(render_priv, code,
                          text_info->glyphs + text_info->length, drawing);

        // Add additional space after italic to non-italic style changes
        if (text_info->length &&
            text_info->glyphs[text_info->length - 1].hash_key.italic &&
            !render_priv->state.italic) {
            int back = text_info->length - 1;
            GlyphInfo *og = &text_info->glyphs[back];
            while (back && og->bbox.xMax - og->bbox.xMin == 0
                   && og->hash_key.italic)
                og = &text_info->glyphs[--back];
            if (og->bbox.xMax > og->advance.x) {
                // The FreeType oblique slants by 6/16
                pen.x += og->bbox.yMax * 0.375;
            }
        }

        text_info->glyphs[text_info->length].pos.x = pen.x;
        text_info->glyphs[text_info->length].pos.y = pen.y;

        pen.x += text_info->glyphs[text_info->length].advance.x;
        pen.x += double_to_d6(render_priv->state.hspacing *
                              render_priv->font_scale
                              * render_priv->state.scale_x);
        pen.y += text_info->glyphs[text_info->length].advance.y;
        pen.y += (render_priv->state.fay * render_priv->state.scale_y) *
                 text_info->glyphs[text_info->length].advance.x;

        previous = code;

        text_info->glyphs[text_info->length].symbol = code;
        text_info->glyphs[text_info->length].linebreak = 0;
        for (i = 0; i < 4; ++i) {
            uint32_t clr = render_priv->state.c[i];
            change_alpha(&clr,
                         mult_alpha(_a(clr), render_priv->state.fade), 1.);
            text_info->glyphs[text_info->length].c[i] = clr;
        }
        text_info->glyphs[text_info->length].effect_type =
            render_priv->state.effect_type;
        text_info->glyphs[text_info->length].effect_timing =
            render_priv->state.effect_timing;
        text_info->glyphs[text_info->length].effect_skip_timing =
            render_priv->state.effect_skip_timing;
        text_info->glyphs[text_info->length].be = render_priv->state.be;
        text_info->glyphs[text_info->length].blur = render_priv->state.blur;
        text_info->glyphs[text_info->length].shadow_x =
            render_priv->state.shadow_x;
        text_info->glyphs[text_info->length].shadow_y =
            render_priv->state.shadow_y;
        text_info->glyphs[text_info->length].frx = render_priv->state.frx;
        text_info->glyphs[text_info->length].fry = render_priv->state.fry;
        text_info->glyphs[text_info->length].frz = render_priv->state.frz;
        text_info->glyphs[text_info->length].fax = render_priv->state.fax;
        text_info->glyphs[text_info->length].fay = render_priv->state.fay;
        if (drawing->hash) {
            text_info->glyphs[text_info->length].asc = drawing->asc;
            text_info->glyphs[text_info->length].desc = drawing->desc;
        } else {
            ass_font_get_asc_desc(render_priv->state.font, code,
                                  &text_info->glyphs[text_info->length].asc,
                                  &text_info->glyphs[text_info->length].desc);

            text_info->glyphs[text_info->length].asc *=
                render_priv->state.scale_y;
            text_info->glyphs[text_info->length].desc *=
                render_priv->state.scale_y;
        }

        // fill bitmap_hash_key
        if (!drawing->hash) {
            text_info->glyphs[text_info->length].hash_key.font =
                render_priv->state.font;
            text_info->glyphs[text_info->length].hash_key.size =
                render_priv->state.font_size;
            text_info->glyphs[text_info->length].hash_key.bold =
                render_priv->state.bold;
            text_info->glyphs[text_info->length].hash_key.italic =
                render_priv->state.italic;
        } else
            text_info->glyphs[text_info->length].hash_key.drawing_hash =
                drawing->hash;
        text_info->glyphs[text_info->length].hash_key.ch = code;
        text_info->glyphs[text_info->length].hash_key.outline.x =
            double_to_d16(render_priv->state.border_x);
        text_info->glyphs[text_info->length].hash_key.outline.y =
            double_to_d16(render_priv->state.border_y);
        text_info->glyphs[text_info->length].hash_key.scale_x =
            double_to_d16(render_priv->state.scale_x);
        text_info->glyphs[text_info->length].hash_key.scale_y =
            double_to_d16(render_priv->state.scale_y);
        text_info->glyphs[text_info->length].hash_key.frx =
            rot_key(render_priv->state.frx);
        text_info->glyphs[text_info->length].hash_key.fry =
            rot_key(render_priv->state.fry);
        text_info->glyphs[text_info->length].hash_key.frz =
            rot_key(render_priv->state.frz);
        text_info->glyphs[text_info->length].hash_key.fax =
            double_to_d16(render_priv->state.fax);
        text_info->glyphs[text_info->length].hash_key.fay =
            double_to_d16(render_priv->state.fay);
        text_info->glyphs[text_info->length].hash_key.advance.x = pen.x;
        text_info->glyphs[text_info->length].hash_key.advance.y = pen.y;
        text_info->glyphs[text_info->length].hash_key.be =
            render_priv->state.be;
        text_info->glyphs[text_info->length].hash_key.blur =
            render_priv->state.blur;
        text_info->glyphs[text_info->length].hash_key.border_style =
            render_priv->state.style->BorderStyle;
        text_info->glyphs[text_info->length].hash_key.shadow_offset.x =
            double_to_d6(
                render_priv->state.shadow_x * render_priv->border_scale -
                (int) (render_priv->state.shadow_x *
                render_priv->border_scale));
        text_info->glyphs[text_info->length].hash_key.shadow_offset.y =
            double_to_d6(
                render_priv->state.shadow_y * render_priv->border_scale -
                (int) (render_priv->state.shadow_y *
                render_priv->border_scale));
        text_info->glyphs[text_info->length].hash_key.flags =
            render_priv->state.flags;

        text_info->length++;

        render_priv->state.effect_type = EF_NONE;
        render_priv->state.effect_timing = 0;
        render_priv->state.effect_skip_timing = 0;

        if (drawing->hash) {
            ass_drawing_free(drawing);
            drawing = render_priv->state.drawing =
                ass_drawing_new(render_priv->fontconfig_priv,
                    render_priv->state.font,
                    render_priv->settings.hinting,
                    render_priv->ftlibrary);
        }
    }


    if (text_info->length == 0) {
        // no valid symbols in the event; this can be smth like {comment}
        free_render_context(render_priv);
        return 1;
    }
    // depends on glyph x coordinates being monotonous, so it should be done before line wrap
    process_karaoke_effects(render_priv);

    // alignments
    alignment = render_priv->state.alignment;
    halign = alignment & 3;
    valign = alignment & 12;

    MarginL =
        (event->MarginL) ? event->MarginL : render_priv->state.style->
        MarginL;
    MarginR =
        (event->MarginR) ? event->MarginR : render_priv->state.style->
        MarginR;
    MarginV =
        (event->MarginV) ? event->MarginV : render_priv->state.style->
        MarginV;

    if (render_priv->state.evt_type != EVENT_HSCROLL) {
        double max_text_width;

        // calculate max length of a line
        max_text_width =
            x2scr(render_priv,
                  render_priv->track->PlayResX - MarginR) -
            x2scr(render_priv, MarginL);

        // rearrange text in several lines
        wrap_lines_smart(render_priv, max_text_width);

        // align text
        last_break = -1;
        for (i = 1; i < text_info->length + 1; ++i) {   // (text_info->length + 1) is the end of the last line
            if ((i == text_info->length)
                || text_info->glyphs[i].linebreak) {
                double width, shift = 0;
                GlyphInfo *first_glyph =
                    text_info->glyphs + last_break + 1;
                GlyphInfo *last_glyph = text_info->glyphs + i - 1;

                while (first_glyph < last_glyph && first_glyph->skip)
                    first_glyph++;

                while ((last_glyph > first_glyph)
                       && ((last_glyph->symbol == '\n')
                           || (last_glyph->symbol == 0)
                           || (last_glyph->skip)))
                    last_glyph--;

                width = d6_to_double(
                    last_glyph->pos.x + last_glyph->advance.x -
                    first_glyph->pos.x);
                if (halign == HALIGN_LEFT) {    // left aligned, no action
                    shift = 0;
                } else if (halign == HALIGN_RIGHT) {    // right aligned
                    shift = max_text_width - width;
                } else if (halign == HALIGN_CENTER) {   // centered
                    shift = (max_text_width - width) / 2.0;
                }
                for (j = last_break + 1; j < i; ++j) {
                    text_info->glyphs[j].pos.x += double_to_d6(shift);
                }
                last_break = i - 1;
            }
        }
    } else {                    // render_priv->state.evt_type == EVENT_HSCROLL
        measure_text(render_priv);
    }

    // determing text bounding box
    compute_string_bbox(text_info, &bbox);

    // determine device coordinates for text

    // x coordinate for everything except positioned events
    if (render_priv->state.evt_type == EVENT_NORMAL ||
        render_priv->state.evt_type == EVENT_VSCROLL) {
        device_x = x2scr(render_priv, MarginL);
    } else if (render_priv->state.evt_type == EVENT_HSCROLL) {
        if (render_priv->state.scroll_direction == SCROLL_RL)
            device_x =
                x2scr(render_priv,
                      render_priv->track->PlayResX -
                      render_priv->state.scroll_shift);
        else if (render_priv->state.scroll_direction == SCROLL_LR)
            device_x =
                x2scr(render_priv,
                      render_priv->state.scroll_shift) - (bbox.xMax -
                                                          bbox.xMin);
    }
    // y coordinate for everything except positioned events
    if (render_priv->state.evt_type == EVENT_NORMAL ||
        render_priv->state.evt_type == EVENT_HSCROLL) {
        if (valign == VALIGN_TOP) {     // toptitle
            device_y =
                y2scr_top(render_priv,
                          MarginV) + text_info->lines[0].asc;
        } else if (valign == VALIGN_CENTER) {   // midtitle
            double scr_y =
                y2scr(render_priv, render_priv->track->PlayResY / 2.0);
            device_y = scr_y - (bbox.yMax + bbox.yMin) / 2.0;
        } else {                // subtitle
            double scr_y;
            if (valign != VALIGN_SUB)
                ass_msg(render_priv->library, MSGL_V,
                       "Invalid valign, supposing 0 (subtitle)");
            scr_y =
                y2scr_sub(render_priv,
                          render_priv->track->PlayResY - MarginV);
            device_y = scr_y;
            device_y -= text_info->height;
            device_y += text_info->lines[0].asc;
        }
    } else if (render_priv->state.evt_type == EVENT_VSCROLL) {
        if (render_priv->state.scroll_direction == SCROLL_TB)
            device_y =
                y2scr(render_priv,
                      render_priv->state.clip_y0 +
                      render_priv->state.scroll_shift) - (bbox.yMax -
                                                          bbox.yMin);
        else if (render_priv->state.scroll_direction == SCROLL_BT)
            device_y =
                y2scr(render_priv,
                      render_priv->state.clip_y1 -
                      render_priv->state.scroll_shift);
    }
    // positioned events are totally different
    if (render_priv->state.evt_type == EVENT_POSITIONED) {
        double base_x = 0;
        double base_y = 0;
        ass_msg(render_priv->library, MSGL_DBG2, "positioned event at %f, %f",
               render_priv->state.pos_x, render_priv->state.pos_y);
        get_base_point(&bbox, alignment, &base_x, &base_y);
        device_x =
            x2scr_pos(render_priv, render_priv->state.pos_x) - base_x;
        device_y =
            y2scr_pos(render_priv, render_priv->state.pos_y) - base_y;
    }
    // fix clip coordinates (they depend on alignment)
    if (render_priv->state.evt_type == EVENT_NORMAL ||
        render_priv->state.evt_type == EVENT_HSCROLL ||
        render_priv->state.evt_type == EVENT_VSCROLL) {
        render_priv->state.clip_x0 =
            x2scr(render_priv, render_priv->state.clip_x0);
        render_priv->state.clip_x1 =
            x2scr(render_priv, render_priv->state.clip_x1);
        if (valign == VALIGN_TOP) {
            render_priv->state.clip_y0 =
                y2scr_top(render_priv, render_priv->state.clip_y0);
            render_priv->state.clip_y1 =
                y2scr_top(render_priv, render_priv->state.clip_y1);
        } else if (valign == VALIGN_CENTER) {
            render_priv->state.clip_y0 =
                y2scr(render_priv, render_priv->state.clip_y0);
            render_priv->state.clip_y1 =
                y2scr(render_priv, render_priv->state.clip_y1);
        } else if (valign == VALIGN_SUB) {
            render_priv->state.clip_y0 =
                y2scr_sub(render_priv, render_priv->state.clip_y0);
            render_priv->state.clip_y1 =
                y2scr_sub(render_priv, render_priv->state.clip_y1);
        }
    } else if (render_priv->state.evt_type == EVENT_POSITIONED) {
        render_priv->state.clip_x0 =
            x2scr_pos(render_priv, render_priv->state.clip_x0);
        render_priv->state.clip_x1 =
            x2scr_pos(render_priv, render_priv->state.clip_x1);
        render_priv->state.clip_y0 =
            y2scr_pos(render_priv, render_priv->state.clip_y0);
        render_priv->state.clip_y1 =
            y2scr_pos(render_priv, render_priv->state.clip_y1);
    }
    // calculate rotation parameters
    {
        DVector center;

        if (render_priv->state.have_origin) {
            center.x = x2scr(render_priv, render_priv->state.org_x);
            center.y = y2scr(render_priv, render_priv->state.org_y);
        } else {
            double bx = 0., by = 0.;
            get_base_point(&bbox, alignment, &bx, &by);
            center.x = device_x + bx;
            center.y = device_y + by;
        }

        for (i = 0; i < text_info->length; ++i) {
            GlyphInfo *info = text_info->glyphs + i;

            if (info->hash_key.frx || info->hash_key.fry
                || info->hash_key.frz || info->hash_key.fax
                || info->hash_key.fay) {
                info->hash_key.shift_x = info->pos.x + double_to_d6(device_x - center.x);
                info->hash_key.shift_y =
                    -(info->pos.y + double_to_d6(device_y - center.y));
            } else {
                info->hash_key.shift_x = 0;
                info->hash_key.shift_y = 0;
            }
        }
    }

    // convert glyphs to bitmaps
    for (i = 0; i < text_info->length; ++i) {
        GlyphInfo *g = text_info->glyphs + i;
        g->hash_key.advance.x =
            double_to_d6(device_x - (int) device_x +
            d6_to_double(g->pos.x & SUBPIXEL_MASK)) & ~SUBPIXEL_ACCURACY;
        g->hash_key.advance.y =
            double_to_d6(device_y - (int) device_y +
            d6_to_double(g->pos.y & SUBPIXEL_MASK)) & ~SUBPIXEL_ACCURACY;
        get_bitmap_glyph(render_priv, text_info->glyphs + i);
    }

    memset(event_images, 0, sizeof(*event_images));
    event_images->top = device_y - text_info->lines[0].asc;
    event_images->height = text_info->height;
    event_images->left = device_x + bbox.xMin + 0.5;
    event_images->width = bbox.xMax - bbox.xMin + 0.5;
    event_images->detect_collisions = render_priv->state.detect_collisions;
    event_images->shift_direction = (valign == VALIGN_TOP) ? 1 : -1;
    event_images->event = event;
    event_images->imgs = render_text(render_priv, (int) device_x, (int) device_y);

    free_render_context(render_priv);

    return 0;
}

/**
 * \brief deallocate image list
 * \param img list pointer
 */
static void ass_free_images(ASS_Image *img)
{
    while (img) {
        ASS_Image *next = img->next;
        free(img);
        img = next;
    }
}

static void ass_reconfigure(ASS_Renderer *priv)
{
    priv->render_id++;
    priv->cache.glyph_cache =
        ass_glyph_cache_reset(priv->cache.glyph_cache);
    priv->cache.bitmap_cache =
        ass_bitmap_cache_reset(priv->cache.bitmap_cache);
    priv->cache.composite_cache =
        ass_composite_cache_reset(priv->cache.composite_cache);
    ass_free_images(priv->prev_images_root);
    priv->prev_images_root = 0;
}

void ass_set_frame_size(ASS_Renderer *priv, int w, int h)
{
    if (priv->settings.frame_width != w || priv->settings.frame_height != h) {
        priv->settings.frame_width = w;
        priv->settings.frame_height = h;
        if (priv->settings.aspect == 0.) {
            priv->settings.aspect = ((double) w) / h;
            priv->settings.storage_aspect = ((double) w) / h;
        }
        ass_reconfigure(priv);
    }
}

void ass_set_margins(ASS_Renderer *priv, int t, int b, int l, int r)
{
    if (priv->settings.left_margin != l ||
        priv->settings.right_margin != r ||
        priv->settings.top_margin != t
        || priv->settings.bottom_margin != b) {
        priv->settings.left_margin = l;
        priv->settings.right_margin = r;
        priv->settings.top_margin = t;
        priv->settings.bottom_margin = b;
        ass_reconfigure(priv);
    }
}

void ass_set_use_margins(ASS_Renderer *priv, int use)
{
    priv->settings.use_margins = use;
}

void ass_set_aspect_ratio(ASS_Renderer *priv, double dar, double sar)
{
    if (priv->settings.aspect != dar || priv->settings.storage_aspect != sar) {
        priv->settings.aspect = dar;
        priv->settings.storage_aspect = sar;
        ass_reconfigure(priv);
    }
}

void ass_set_font_scale(ASS_Renderer *priv, double font_scale)
{
    if (priv->settings.font_size_coeff != font_scale) {
        priv->settings.font_size_coeff = font_scale;
        ass_reconfigure(priv);
    }
}

void ass_set_hinting(ASS_Renderer *priv, ASS_Hinting ht)
{
    if (priv->settings.hinting != ht) {
        priv->settings.hinting = ht;
        ass_reconfigure(priv);
    }
}

void ass_set_line_spacing(ASS_Renderer *priv, double line_spacing)
{
    priv->settings.line_spacing = line_spacing;
}

void ass_set_fonts(ASS_Renderer *priv, const char *default_font,
                   const char *default_family, int fc, const char *config,
                   int update)
{
    free(priv->settings.default_font);
    free(priv->settings.default_family);
    priv->settings.default_font = default_font ? strdup(default_font) : 0;
    priv->settings.default_family =
        default_family ? strdup(default_family) : 0;

    if (priv->fontconfig_priv)
        fontconfig_done(priv->fontconfig_priv);
    priv->fontconfig_priv =
        fontconfig_init(priv->library, priv->ftlibrary, default_family,
                        default_font, fc, config, update);
}

int ass_fonts_update(ASS_Renderer *render_priv)
{
    return fontconfig_update(render_priv->fontconfig_priv);
}

/**
 * \brief Start a new frame
 */
static int
ass_start_frame(ASS_Renderer *render_priv, ASS_Track *track,
                long long now)
{
    ASS_Settings *settings_priv = &render_priv->settings;
    CacheStore *cache = &render_priv->cache;

    if (!render_priv->settings.frame_width
        && !render_priv->settings.frame_height)
        return 1;               // library not initialized

    if (render_priv->library != track->library)
        return 1;

    free_list_clear(render_priv);

    if (track->n_events == 0)
        return 1;               // nothing to do

    render_priv->width = settings_priv->frame_width;
    render_priv->height = settings_priv->frame_height;
    render_priv->orig_width =
        settings_priv->frame_width - settings_priv->left_margin -
        settings_priv->right_margin;
    render_priv->orig_height =
        settings_priv->frame_height - settings_priv->top_margin -
        settings_priv->bottom_margin;
    render_priv->orig_width_nocrop =
        settings_priv->frame_width - FFMAX(settings_priv->left_margin,
                                           0) -
        FFMAX(settings_priv->right_margin, 0);
    render_priv->orig_height_nocrop =
        settings_priv->frame_height - FFMAX(settings_priv->top_margin,
                                            0) -
        FFMAX(settings_priv->bottom_margin, 0);
    render_priv->track = track;
    render_priv->time = now;

    ass_lazy_track_init(render_priv);

    render_priv->font_scale = settings_priv->font_size_coeff *
        render_priv->orig_height / render_priv->track->PlayResY;
    if (render_priv->track->ScaledBorderAndShadow)
        render_priv->border_scale =
            ((double) render_priv->orig_height) /
            render_priv->track->PlayResY;
    else
        render_priv->border_scale = 1.;

    // PAR correction
    render_priv->font_scale_x = render_priv->settings.aspect /
                                render_priv->settings.storage_aspect;

    render_priv->prev_images_root = render_priv->images_root;
    render_priv->images_root = 0;

    if (cache->bitmap_cache->cache_size > cache->bitmap_max_size) {
        ass_msg(render_priv->library, MSGL_V,
                "Hitting hard bitmap cache limit (was: %ld bytes), "
                "resetting.", (long) cache->bitmap_cache->cache_size);
        cache->bitmap_cache = ass_bitmap_cache_reset(cache->bitmap_cache);
        cache->composite_cache = ass_composite_cache_reset(
            cache->composite_cache);
        ass_free_images(render_priv->prev_images_root);
        render_priv->prev_images_root = 0;
    }

    if (cache->glyph_cache->count > cache->glyph_max) {
        ass_msg(render_priv->library, MSGL_V,
            "Hitting hard glyph cache limit (was: %ld glyphs), resetting.",
            (long) cache->glyph_cache->count);
        cache->glyph_cache = ass_glyph_cache_reset(cache->glyph_cache);
    }

    return 0;
}

static int cmp_event_layer(const void *p1, const void *p2)
{
    ASS_Event *e1 = ((EventImages *) p1)->event;
    ASS_Event *e2 = ((EventImages *) p2)->event;
    if (e1->Layer < e2->Layer)
        return -1;
    if (e1->Layer > e2->Layer)
        return 1;
    if (e1->ReadOrder < e2->ReadOrder)
        return -1;
    if (e1->ReadOrder > e2->ReadOrder)
        return 1;
    return 0;
}

static ASS_RenderPriv *get_render_priv(ASS_Renderer *render_priv,
                                       ASS_Event *event)
{
    if (!event->render_priv)
        event->render_priv = calloc(1, sizeof(ASS_RenderPriv));
    if (render_priv->render_id != event->render_priv->render_id) {
        memset(event->render_priv, 0, sizeof(ASS_RenderPriv));
        event->render_priv->render_id = render_priv->render_id;
    }

    return event->render_priv;
}

static int overlap(Segment *s1, Segment *s2)
{
    if (s1->a >= s2->b || s2->a >= s1->b ||
        s1->ha >= s2->hb || s2->ha >= s1->hb)
        return 0;
    return 1;
}

static int cmp_segment(const void *p1, const void *p2)
{
    return ((Segment *) p1)->a - ((Segment *) p2)->a;
}

static void
shift_event(ASS_Renderer *render_priv, EventImages *ei, int shift)
{
    ASS_Image *cur = ei->imgs;
    while (cur) {
        cur->dst_y += shift;
        // clip top and bottom
        if (cur->dst_y < 0) {
            int clip = -cur->dst_y;
            cur->h -= clip;
            cur->bitmap += clip * cur->stride;
            cur->dst_y = 0;
        }
        if (cur->dst_y + cur->h >= render_priv->height) {
            int clip = cur->dst_y + cur->h - render_priv->height;
            cur->h -= clip;
        }
        if (cur->h <= 0) {
            cur->h = 0;
            cur->dst_y = 0;
        }
        cur = cur->next;
    }
    ei->top += shift;
}

// dir: 1 - move down
//      -1 - move up
static int fit_segment(Segment *s, Segment *fixed, int *cnt, int dir)
{
    int i;
    int shift = 0;

    if (dir == 1)               // move down
        for (i = 0; i < *cnt; ++i) {
            if (s->b + shift <= fixed[i].a || s->a + shift >= fixed[i].b ||
                s->hb <= fixed[i].ha || s->ha >= fixed[i].hb)
                continue;
            shift = fixed[i].b - s->a;
    } else                      // dir == -1, move up
        for (i = *cnt - 1; i >= 0; --i) {
            if (s->b + shift <= fixed[i].a || s->a + shift >= fixed[i].b ||
                s->hb <= fixed[i].ha || s->ha >= fixed[i].hb)
                continue;
            shift = fixed[i].a - s->b;
        }

    fixed[*cnt].a = s->a + shift;
    fixed[*cnt].b = s->b + shift;
    fixed[*cnt].ha = s->ha;
    fixed[*cnt].hb = s->hb;
    (*cnt)++;
    qsort(fixed, *cnt, sizeof(Segment), cmp_segment);

    return shift;
}

static void
fix_collisions(ASS_Renderer *render_priv, EventImages *imgs, int cnt)
{
    Segment *used = malloc(cnt * sizeof(*used));
    int cnt_used = 0;
    int i, j;

    // fill used[] with fixed events
    for (i = 0; i < cnt; ++i) {
        ASS_RenderPriv *priv;
        if (!imgs[i].detect_collisions)
            continue;
        priv = get_render_priv(render_priv, imgs[i].event);
        if (priv->height > 0) { // it's a fixed event
            Segment s;
            s.a = priv->top;
            s.b = priv->top + priv->height;
            s.ha = priv->left;
            s.hb = priv->left + priv->width;
            if (priv->height != imgs[i].height) {       // no, it's not
                ass_msg(render_priv->library, MSGL_WARN,
                        "Warning! Event height has changed");
                priv->top = 0;
                priv->height = 0;
                priv->left = 0;
                priv->width = 0;
            }
            for (j = 0; j < cnt_used; ++j)
                if (overlap(&s, used + j)) {    // no, it's not
                    priv->top = 0;
                    priv->height = 0;
                    priv->left = 0;
                    priv->width = 0;
                }
            if (priv->height > 0) {     // still a fixed event
                used[cnt_used].a = priv->top;
                used[cnt_used].b = priv->top + priv->height;
                used[cnt_used].ha = priv->left;
                used[cnt_used].hb = priv->left + priv->width;
                cnt_used++;
                shift_event(render_priv, imgs + i, priv->top - imgs[i].top);
            }
        }
    }
    qsort(used, cnt_used, sizeof(Segment), cmp_segment);

    // try to fit other events in free spaces
    for (i = 0; i < cnt; ++i) {
        ASS_RenderPriv *priv;
        if (!imgs[i].detect_collisions)
            continue;
        priv = get_render_priv(render_priv, imgs[i].event);
        if (priv->height == 0) {        // not a fixed event
            int shift;
            Segment s;
            s.a = imgs[i].top;
            s.b = imgs[i].top + imgs[i].height;
            s.ha = imgs[i].left;
            s.hb = imgs[i].left + imgs[i].width;
            shift = fit_segment(&s, used, &cnt_used, imgs[i].shift_direction);
            if (shift)
                shift_event(render_priv, imgs + i, shift);
            // make it fixed
            priv->top = imgs[i].top;
            priv->height = imgs[i].height;
            priv->left = imgs[i].left;
            priv->width = imgs[i].width;
        }

    }

    free(used);
}

/**
 * \brief compare two images
 * \param i1 first image
 * \param i2 second image
 * \return 0 if identical, 1 if different positions, 2 if different content
 */
static int ass_image_compare(ASS_Image *i1, ASS_Image *i2)
{
    if (i1->w != i2->w)
        return 2;
    if (i1->h != i2->h)
        return 2;
    if (i1->stride != i2->stride)
        return 2;
    if (i1->color != i2->color)
        return 2;
    if (i1->bitmap != i2->bitmap)
        return 2;
    if (i1->dst_x != i2->dst_x)
        return 1;
    if (i1->dst_y != i2->dst_y)
        return 1;
    return 0;
}

/**
 * \brief compare current and previous image list
 * \param priv library handle
 * \return 0 if identical, 1 if different positions, 2 if different content
 */
static int ass_detect_change(ASS_Renderer *priv)
{
    ASS_Image *img, *img2;
    int diff;

    img = priv->prev_images_root;
    img2 = priv->images_root;
    diff = 0;
    while (img && diff < 2) {
        ASS_Image *next, *next2;
        next = img->next;
        if (img2) {
            int d = ass_image_compare(img, img2);
            if (d > diff)
                diff = d;
            next2 = img2->next;
        } else {
            // previous list is shorter
            diff = 2;
            break;
        }
        img = next;
        img2 = next2;
    }

    // is the previous list longer?
    if (img2)
        diff = 2;

    return diff;
}

/**
 * \brief render a frame
 * \param priv library handle
 * \param track track
 * \param now current video timestamp (ms)
 * \param detect_change a value describing how the new images differ from the previous ones will be written here:
 *        0 if identical, 1 if different positions, 2 if different content.
 *        Can be NULL, in that case no detection is performed.
 */
ASS_Image *ass_render_frame(ASS_Renderer *priv, ASS_Track *track,
                            long long now, int *detect_change)
{
    int i, cnt, rc;
    EventImages *last;
    ASS_Image **tail;

    // init frame
    rc = ass_start_frame(priv, track, now);
    if (rc != 0)
        return 0;

    // render events separately
    cnt = 0;
    for (i = 0; i < track->n_events; ++i) {
        ASS_Event *event = track->events + i;
        if ((event->Start <= now)
            && (now < (event->Start + event->Duration))) {
            if (cnt >= priv->eimg_size) {
                priv->eimg_size += 100;
                priv->eimg =
                    realloc(priv->eimg,
                            priv->eimg_size * sizeof(EventImages));
            }
            rc = ass_render_event(priv, event, priv->eimg + cnt);
            if (!rc)
                ++cnt;
        }
    }

    // sort by layer
    qsort(priv->eimg, cnt, sizeof(EventImages), cmp_event_layer);

    // call fix_collisions for each group of events with the same layer
    last = priv->eimg;
    for (i = 1; i < cnt; ++i)
        if (last->event->Layer != priv->eimg[i].event->Layer) {
            fix_collisions(priv, last, priv->eimg + i - last);
            last = priv->eimg + i;
        }
    if (cnt > 0)
        fix_collisions(priv, last, priv->eimg + cnt - last);

    // concat lists
    tail = &priv->images_root;
    for (i = 0; i < cnt; ++i) {
        ASS_Image *cur = priv->eimg[i].imgs;
        while (cur) {
            *tail = cur;
            tail = &cur->next;
            cur = cur->next;
        }
    }

    if (detect_change)
        *detect_change = ass_detect_change(priv);

    // free the previous image list
    ass_free_images(priv->prev_images_root);
    priv->prev_images_root = 0;

    return priv->images_root;
}
