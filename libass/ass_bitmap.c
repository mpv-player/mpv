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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ft2build.h>
#include FT_GLYPH_H

#include "ass_utils.h"
#include "ass_bitmap.h"

struct ass_synth_priv {
    int tmp_w, tmp_h;
    unsigned short *tmp;

    int g_r;
    int g_w;

    unsigned *g;
    unsigned *gt2;

    double radius;
};

static const unsigned int maxcolor = 255;
static const unsigned base = 256;

static int generate_tables(ASS_SynthPriv *priv, double radius)
{
    double A = log(1.0 / base) / (radius * radius * 2);
    int mx, i;
    double volume_diff, volume_factor = 0;
    unsigned volume;

    if (priv->radius == radius)
        return 0;
    else
        priv->radius = radius;

    priv->g_r = ceil(radius);
    priv->g_w = 2 * priv->g_r + 1;

    if (priv->g_r) {
        priv->g = realloc(priv->g, priv->g_w * sizeof(unsigned));
        priv->gt2 = realloc(priv->gt2, 256 * priv->g_w * sizeof(unsigned));
        if (priv->g == NULL || priv->gt2 == NULL) {
            return -1;
        }
    }

    if (priv->g_r) {
        // gaussian curve with volume = 256
        for (volume_diff = 10000000; volume_diff > 0.0000001;
             volume_diff *= 0.5) {
            volume_factor += volume_diff;
            volume = 0;
            for (i = 0; i < priv->g_w; ++i) {
                priv->g[i] =
                    (unsigned) (exp(A * (i - priv->g_r) * (i - priv->g_r)) *
                                volume_factor + .5);
                volume += priv->g[i];
            }
            if (volume > 256)
                volume_factor -= volume_diff;
        }
        volume = 0;
        for (i = 0; i < priv->g_w; ++i) {
            priv->g[i] =
                (unsigned) (exp(A * (i - priv->g_r) * (i - priv->g_r)) *
                            volume_factor + .5);
            volume += priv->g[i];
        }

        // gauss table:
        for (mx = 0; mx < priv->g_w; mx++) {
            for (i = 0; i < 256; i++) {
                priv->gt2[mx + i * priv->g_w] = i * priv->g[mx];
            }
        }
    }

    return 0;
}

static void resize_tmp(ASS_SynthPriv *priv, int w, int h)
{
    if (priv->tmp_w >= w && priv->tmp_h >= h)
        return;
    if (priv->tmp_w == 0)
        priv->tmp_w = 64;
    if (priv->tmp_h == 0)
        priv->tmp_h = 64;
    while (priv->tmp_w < w)
        priv->tmp_w *= 2;
    while (priv->tmp_h < h)
        priv->tmp_h *= 2;
    if (priv->tmp)
        free(priv->tmp);
    priv->tmp = malloc((priv->tmp_w + 1) * priv->tmp_h * sizeof(short));
}

ASS_SynthPriv *ass_synth_init(double radius)
{
    ASS_SynthPriv *priv = calloc(1, sizeof(ASS_SynthPriv));
    generate_tables(priv, radius);
    return priv;
}

void ass_synth_done(ASS_SynthPriv *priv)
{
    if (priv->tmp)
        free(priv->tmp);
    if (priv->g)
        free(priv->g);
    if (priv->gt2)
        free(priv->gt2);
    free(priv);
}

static Bitmap *alloc_bitmap(int w, int h)
{
    Bitmap *bm;
    bm = calloc(1, sizeof(Bitmap));
    bm->buffer = malloc(w * h);
    bm->w = w;
    bm->h = h;
    bm->left = bm->top = 0;
    return bm;
}

void ass_free_bitmap(Bitmap *bm)
{
    if (bm) {
        if (bm->buffer)
            free(bm->buffer);
        free(bm);
    }
}

static Bitmap *copy_bitmap(const Bitmap *src)
{
    Bitmap *dst = alloc_bitmap(src->w, src->h);
    dst->left = src->left;
    dst->top = src->top;
    memcpy(dst->buffer, src->buffer, src->w * src->h);
    return dst;
}

static int check_glyph_area(ASS_Library *library, FT_Glyph glyph)
{
    FT_BBox bbox;
    long long dx, dy;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_TRUNCATE, &bbox);
    dx = bbox.xMax - bbox.xMin;
    dy = bbox.yMax - bbox.yMin;
    if (dx * dy > 8000000) {
        ass_msg(library, MSGL_WARN, "Glyph bounding box too large: %dx%dpx",
               (int) dx, (int) dy);
        return 1;
    } else
        return 0;
}

static Bitmap *glyph_to_bitmap_internal(ASS_Library *library,
                                          FT_Glyph glyph, int bord)
{
    FT_BitmapGlyph bg;
    FT_Bitmap *bit;
    Bitmap *bm;
    int w, h;
    unsigned char *src;
    unsigned char *dst;
    int i;
    int error;

    if (check_glyph_area(library, glyph))
        return 0;
    error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, 0, 0);
    if (error) {
        ass_msg(library, MSGL_WARN, "FT_Glyph_To_Bitmap error %d",
               error);
        return 0;
    }

    bg = (FT_BitmapGlyph) glyph;
    bit = &(bg->bitmap);
    if (bit->pixel_mode != FT_PIXEL_MODE_GRAY) {
        ass_msg(library, MSGL_WARN, "Unsupported pixel mode: %d",
               (int) (bit->pixel_mode));
        FT_Done_Glyph(glyph);
        return 0;
    }

    w = bit->width;
    h = bit->rows;
    bm = alloc_bitmap(w + 2 * bord, h + 2 * bord);
    memset(bm->buffer, 0, bm->w * bm->h);
    bm->left = bg->left - bord;
    bm->top = -bg->top - bord;

    src = bit->buffer;
    dst = bm->buffer + bord + bm->w * bord;
    for (i = 0; i < h; ++i) {
        memcpy(dst, src, w);
        src += bit->pitch;
        dst += bm->w;
    }

    FT_Done_Glyph(glyph);
    return bm;
}

/**
 * \brief fix outline bitmap
 *
 * The glyph bitmap is subtracted from outline bitmap. This way looks much
 * better in some cases.
 */
static void fix_outline(Bitmap *bm_g, Bitmap *bm_o)
{
    int x, y;
    const int l = bm_o->left > bm_g->left ? bm_o->left : bm_g->left;
    const int t = bm_o->top > bm_g->top ? bm_o->top : bm_g->top;
    const int r =
        bm_o->left + bm_o->w <
        bm_g->left + bm_g->w ? bm_o->left + bm_o->w : bm_g->left + bm_g->w;
    const int b =
        bm_o->top + bm_o->h <
        bm_g->top + bm_g->h ? bm_o->top + bm_o->h : bm_g->top + bm_g->h;

    unsigned char *g =
        bm_g->buffer + (t - bm_g->top) * bm_g->w + (l - bm_g->left);
    unsigned char *o =
        bm_o->buffer + (t - bm_o->top) * bm_o->w + (l - bm_o->left);

    for (y = 0; y < b - t; ++y) {
        for (x = 0; x < r - l; ++x) {
            unsigned char c_g, c_o;
            c_g = g[x];
            c_o = o[x];
            o[x] = (c_o > c_g) ? c_o - (c_g / 2) : 0;
        }
        g += bm_g->w;
        o += bm_o->w;
    }
}

/**
 * \brief Shift a bitmap by the fraction of a pixel in x and y direction
 * expressed in 26.6 fixed point
 */
static void shift_bitmap(unsigned char *buf, int w, int h, int shift_x,
                         int shift_y)
{
    int x, y, b;

    // Shift in x direction
    if (shift_x > 0) {
        for (y = 0; y < h; y++) {
            for (x = w - 1; x > 0; x--) {
                b = (buf[x + y * w - 1] * shift_x) >> 6;
                buf[x + y * w - 1] -= b;
                buf[x + y * w] += b;
            }
        }
    } else if (shift_x < 0) {
        shift_x = -shift_x;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w - 1; x++) {
                b = (buf[x + y * w + 1] * shift_x) >> 6;
                buf[x + y * w + 1] -= b;
                buf[x + y * w] += b;
            }
        }
    }

    // Shift in y direction
    if (shift_y > 0) {
        for (x = 0; x < w; x++) {
            for (y = h - 1; y > 0; y--) {
                b = (buf[x + (y - 1) * w] * shift_y) >> 6;
                buf[x + (y - 1) * w] -= b;
                buf[x + y * w] += b;
            }
        }
    } else if (shift_y < 0) {
        shift_y = -shift_y;
        for (x = 0; x < w; x++) {
            for (y = 0; y < h - 1; y++) {
                b = (buf[x + (y + 1) * w] * shift_y) >> 6;
                buf[x + (y + 1) * w] -= b;
                buf[x + y * w] += b;
            }
        }
    }
}

/*
 * Gaussian blur.  An fast pure C implementation from MPlayer.
 */
static void ass_gauss_blur(unsigned char *buffer, unsigned short *tmp2,
                           int width, int height, int stride, int *m2,
                           int r, int mwidth)
{

    int x, y;

    unsigned char *s = buffer;
    unsigned short *t = tmp2 + 1;
    for (y = 0; y < height; y++) {
        memset(t - 1, 0, (width + 1) * sizeof(short));

        for (x = 0; x < r; x++) {
            const int src = s[x];
            if (src) {
                register unsigned short *dstp = t + x - r;
                int mx;
                unsigned *m3 = (unsigned *) (m2 + src * mwidth);
                for (mx = r - x; mx < mwidth; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        for (; x < width - r; x++) {
            const int src = s[x];
            if (src) {
                register unsigned short *dstp = t + x - r;
                int mx;
                unsigned *m3 = (unsigned *) (m2 + src * mwidth);
                for (mx = 0; mx < mwidth; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        for (; x < width; x++) {
            const int src = s[x];
            if (src) {
                register unsigned short *dstp = t + x - r;
                int mx;
                const int x2 = r + width - x;
                unsigned *m3 = (unsigned *) (m2 + src * mwidth);
                for (mx = 0; mx < x2; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        s += stride;
        t += width + 1;
    }

    t = tmp2;
    for (x = 0; x < width; x++) {
        for (y = 0; y < r; y++) {
            unsigned short *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned short *dstp = srcp - 1 + width + 1;
                const int src2 = (src + 128) >> 8;
                unsigned *m3 = (unsigned *) (m2 + src2 * mwidth);

                int mx;
                *srcp = 128;
                for (mx = r - 1; mx < mwidth; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        for (; y < height - r; y++) {
            unsigned short *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned short *dstp = srcp - 1 - r * (width + 1);
                const int src2 = (src + 128) >> 8;
                unsigned *m3 = (unsigned *) (m2 + src2 * mwidth);

                int mx;
                *srcp = 128;
                for (mx = 0; mx < mwidth; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        for (; y < height; y++) {
            unsigned short *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                const int y2 = r + height - y;
                register unsigned short *dstp = srcp - 1 - r * (width + 1);
                const int src2 = (src + 128) >> 8;
                unsigned *m3 = (unsigned *) (m2 + src2 * mwidth);

                int mx;
                *srcp = 128;
                for (mx = 0; mx < y2; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        t++;
    }

    t = tmp2;
    s = buffer;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            s[x] = t[x] >> 8;
        }
        s += stride;
        t += width + 1;
    }
}

/**
 * \brief Blur with [[1,2,1]. [2,4,2], [1,2,1]] kernel
 * This blur is the same as the one employed by vsfilter.
 */
static void be_blur(unsigned char *buf, int w, int h)
{
    unsigned int x, y;
    unsigned int old_sum, new_sum;

    for (y = 0; y < h; y++) {
        old_sum = 2 * buf[y * w];
        for (x = 0; x < w - 1; x++) {
            new_sum = buf[y * w + x] + buf[y * w + x + 1];
            buf[y * w + x] = (old_sum + new_sum) >> 2;
            old_sum = new_sum;
        }
    }

    for (x = 0; x < w; x++) {
        old_sum = 2 * buf[x];
        for (y = 0; y < h - 1; y++) {
            new_sum = buf[y * w + x] + buf[(y + 1) * w + x];
            buf[y * w + x] = (old_sum + new_sum) >> 2;
            old_sum = new_sum;
        }
    }
}

int glyph_to_bitmap(ASS_Library *library, ASS_SynthPriv *priv_blur,
                    FT_Glyph glyph, FT_Glyph outline_glyph,
                    Bitmap **bm_g, Bitmap **bm_o, Bitmap **bm_s,
                    int be, double blur_radius, FT_Vector shadow_offset,
                    int border_style)
{
    blur_radius *= 2;
    int bbord = be > 0 ? sqrt(2 * be) : 0;
    int gbord = blur_radius > 0.0 ? blur_radius + 1 : 0;
    int bord = FFMAX(bbord, gbord);
    if (bord == 0 && (shadow_offset.x || shadow_offset.y))
        bord = 1;

    assert(bm_g && bm_o && bm_s);

    *bm_g = *bm_o = *bm_s = 0;

    if (glyph)
        *bm_g = glyph_to_bitmap_internal(library, glyph, bord);
    if (!*bm_g)
        return 1;

    if (outline_glyph) {
        *bm_o = glyph_to_bitmap_internal(library, outline_glyph, bord);
        if (!*bm_o) {
            return 1;
        }
    }

    // Apply box blur (multiple passes, if requested)
    while (be--) {
        if (*bm_o)
            be_blur((*bm_o)->buffer, (*bm_o)->w, (*bm_o)->h);
        else
            be_blur((*bm_g)->buffer, (*bm_g)->w, (*bm_g)->h);
    }

    // Apply gaussian blur
    if (blur_radius > 0.0) {
        if (*bm_o)
            resize_tmp(priv_blur, (*bm_o)->w, (*bm_o)->h);
        else
            resize_tmp(priv_blur, (*bm_g)->w, (*bm_g)->h);
        generate_tables(priv_blur, blur_radius);
        if (*bm_o)
            ass_gauss_blur((*bm_o)->buffer, priv_blur->tmp,
                           (*bm_o)->w, (*bm_o)->h, (*bm_o)->w,
                           (int *) priv_blur->gt2, priv_blur->g_r,
                           priv_blur->g_w);
        else
            ass_gauss_blur((*bm_g)->buffer, priv_blur->tmp,
                           (*bm_g)->w, (*bm_g)->h, (*bm_g)->w,
                           (int *) priv_blur->gt2, priv_blur->g_r,
                           priv_blur->g_w);
    }

    // Create shadow and fix outline as needed
    if (*bm_o && border_style != 3) {
        *bm_s = copy_bitmap(*bm_o);
        fix_outline(*bm_g, *bm_o);
    } else if (*bm_o) {
        *bm_s = copy_bitmap(*bm_o);
    } else
        *bm_s = copy_bitmap(*bm_g);

    assert(bm_s);

    shift_bitmap((*bm_s)->buffer, (*bm_s)->w,(*bm_s)->h,
                 shadow_offset.x, shadow_offset.y);

    return 0;
}
