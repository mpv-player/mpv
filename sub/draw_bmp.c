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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "sub/draw_bmp.h"

#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "sub/sub.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/sws_utils.h"
#include "libmpcodecs/img_format.h"
#include "libvo/csputils.h"

const bool mp_draw_sub_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = true,
    [SUBBITMAP_RGBA] = true,
};

struct sub_cache {
    struct mp_image *i, *a;
};

struct part {
    int bitmap_pos_id;
    int num_imgs;
    struct sub_cache *imgs;
};

struct mp_draw_sub_cache
{
    struct part *parts[MAX_OSD_PARTS];
};

#define ACCURATE
#define CONDITIONAL
#define CONDITIONAL2

static void blend_const16_alpha(uint8_t *dst,
                                ssize_t dstRowStride,
                                uint16_t srcp,
                                const uint8_t *srca,
                                ssize_t srcaRowStride,
                                uint8_t srcamul, int rows,
                                int cols)
{
    int i, j;
#ifdef CONDITIONAL
    if (!srcamul)
        return;
#endif
    for (i = 0; i < rows; ++i) {
        uint16_t *dstr = (uint16_t *) (dst + dstRowStride * i);
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint32_t srcap = srcar[j];
                // 32bit to force the math ops to operate on 32 bit
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
#ifdef CONDITIONAL2
            if (srcap == 255 && srcamul == 255) {
                dstr[j] = srcp;
                continue;
            }
#endif
            uint16_t dstp = dstr[j];
            srcap *= srcamul; // now 0..65025
            uint16_t outp =
                (srcp * srcap + dstp * (65025 - srcap) + 32512) / 65025;
            dstr[j] = outp;
        }
    }
}

static void blend_src16_alpha(uint8_t *dst,
                              ssize_t dstRowStride,
                              const uint8_t *src,
                              ssize_t srcRowStride,
                              const uint8_t *srca,
                              ssize_t srcaRowStride,
                              int rows,
                              int cols)
{
    int i, j;
    for (i = 0; i < rows; ++i) {
        uint16_t *dstr = (uint16_t *) (dst + dstRowStride * i);
        const uint16_t *srcr = (const uint16_t *) (src + srcRowStride * i);
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint32_t srcap = srcar[j];
                // 32bit to force the math ops to operate on 32 bit
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
            uint16_t srcp = srcr[j];
#ifdef CONDITIONAL2
            if (srcap == 255) {
                dstr[j] = srcp;
                continue;
            }
#endif
            uint16_t dstp = dstr[j];
            uint16_t outp =
                (srcp * srcap + dstp * (255 - srcap) + 127) / 255;
            dstr[j] = outp;
        }
    }
}

static void blend_const8_alpha(uint8_t *dst,
                               ssize_t dstRowStride,
                               uint16_t srcp,
                               const uint8_t *srca,
                               ssize_t srcaRowStride,
                               uint8_t srcamul, int rows,
                               int cols)
{
    int i, j;
#ifdef CONDITIONAL
    if (!srcamul)
        return;
#endif
    for (i = 0; i < rows; ++i) {
        uint8_t *dstr = dst + dstRowStride * i;
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint32_t srcap = srcar[j];
                // 32bit to force the math ops to operate on 32 bit
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
#ifdef CONDITIONAL2
            if (srcap == 255 && srcamul == 255) {
                dstr[j] = srcp;
                continue;
            }
#endif
            uint8_t dstp = dstr[j];
#ifdef ACCURATE
            srcap *= srcamul; // now 0..65025
            uint8_t outp =
                (srcp * srcap + dstp * (65025 - srcap) + 32512) / 65025;
            dstr[j] = outp;
#else
            srcap = (srcap * srcamul + 255) >> 8;
            uint8_t outp =
                (srcp * srcap + dstp * (255 - srcap) + 255) >> 8;
            dstr[j] = outp;
#endif
        }
    }
}

static void blend_src8_alpha(uint8_t *dst,
                             ssize_t dstRowStride,
                             const uint8_t *src,
                             ssize_t srcRowStride,
                             const uint8_t *srca,
                             ssize_t srcaRowStride,
                             int rows,
                             int cols)
{
    int i, j;
    for (i = 0; i < rows; ++i) {
        uint8_t *dstr = dst + dstRowStride * i;
        const uint8_t *srcr = src + srcRowStride * i;
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint16_t srcap = srcar[j];
                // 16bit to force the math ops to operate on 16 bit
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
            uint8_t srcp = srcr[j];
#ifdef CONDITIONAL2
            if (srcap == 255) {
                dstr[j] = srcp;
                continue;
            }
#endif
            uint8_t dstp = dstr[j];
#ifdef ACCURATE
            uint8_t outp =
                (srcp * srcap + dstp * (255 - srcap) + 127) / 255;
#else
            uint8_t outp =
                (srcp * srcap + dstp * (255 - srcap) + 255) >> 8;
#endif
            dstr[j] = outp;
        }
    }
}

static void blend_src_alpha(uint8_t *dst, ssize_t dstRowStride,
                            const uint8_t *src, ssize_t srcRowStride,
                            const uint8_t *srca, ssize_t srcaRowStride,
                            int rows, int cols, int bytes)
{
    if (bytes == 2) {
        blend_src16_alpha(dst, dstRowStride, src,
                          srcRowStride, srca,
                          srcaRowStride, rows, cols);
    } else if (bytes == 1) {
        blend_src8_alpha(dst, dstRowStride, src,
                         srcRowStride, srca,
                         srcaRowStride, rows, cols);
    }
}

static void blend_const_alpha(uint8_t *dst, ssize_t dstRowStride,
                              uint16_t srcp,
                              const uint8_t *srca, ssize_t srcaRowStride,
                              uint8_t srcamul,
                              int rows, int cols, int bytes)
{
    if (bytes == 2) {
        blend_const16_alpha(dst, dstRowStride, srcp,
                            srca, srcaRowStride,
                            srcamul, rows,
                            cols);
    } else if (bytes == 1) {
        blend_const8_alpha(dst, dstRowStride, srcp,
                           srca, srcaRowStride, srcamul,
                           rows,
                           cols);
    }
}

static inline int min(int x, int y)
{
    if (x < y)
        return x;
    else
        return y;
}

static void unpremultiply_and_split_bgra(mp_image_t *img, mp_image_t *alpha)
{
    int x, y;
    for (y = 0; y < img->h; ++y) {
        unsigned char *irow = &img->planes[0][img->stride[0] * y];
        unsigned char *arow = &alpha->planes[0][alpha->stride[0] * y];
        for (x = 0; x < img->w; ++x) {
            unsigned char aval = irow[4 * x + 3];
            // multiplied = separate * alpha / 255
            // separate = rint(multiplied * 255 / alpha)
            //          = floor(multiplied * 255 / alpha + 0.5)
            //          = floor((multiplied * 255 + 0.5 * alpha) / alpha)
            //          = floor((multiplied * 255 + floor(0.5 * alpha)) / alpha)
            int div = (int) aval;
            int add = div / 2;
            if (aval) {
                irow[4 * x + 0] = min(255, (irow[4 * x + 0] * 255 + add) / div);
                irow[4 * x + 1] = min(255, (irow[4 * x + 1] * 255 + add) / div);
                irow[4 * x + 2] = min(255, (irow[4 * x + 2] * 255 + add) / div);
            }
            arow[x] = aval;
        }
    }
}

static bool sub_bitmap_to_mp_images(struct mp_image **sbi, int *color_yuv,
                                    int *color_a, struct mp_image **sba,
                                    struct sub_bitmap *sb,
                                    int format, struct mp_csp_details *csp,
                                    float rgb2yuv[3][4], int imgfmt, int bits)
{
    *sbi = NULL;
    *sba = NULL;
    if (format == SUBBITMAP_RGBA && sb->w >= 8) {
        // >= 8 because of libswscale madness
        // swscale the bitmap from w*h to dw*dh, changing BGRA8 into YUV444P16
        // and make a scaled copy of A8
        mp_image_t *sbisrc = new_mp_image(sb->w, sb->h);
        mp_image_setfmt(sbisrc, IMGFMT_BGRA);
        sbisrc->planes[0] = sb->bitmap;
        sbisrc->stride[0] = sb->stride;
        mp_image_t *sbisrc2 = alloc_mpi(sb->dw, sb->dh, IMGFMT_BGRA);
        mp_image_swscale(sbisrc2, sbisrc, csp, SWS_BILINEAR);

        // sbisrc2 now is the original image in premultiplied alpha, but
        // properly scaled...
        // now, un-premultiply so we can work in YUV color space, also extract
        // alpha
        *sba = alloc_mpi(sb->dw, sb->dh, IMGFMT_Y8);
        unpremultiply_and_split_bgra(sbisrc2, *sba);

        // convert to the output format
        *sbi = alloc_mpi(sb->dw, sb->dh, imgfmt);
        mp_image_swscale(*sbi, sbisrc2, csp, SWS_BILINEAR);

        free_mp_image(sbisrc);
        free_mp_image(sbisrc2);

        color_yuv[0] = 255;
        color_yuv[1] = 128;
        color_yuv[2] = 128;
        *color_a = 255;
        return true;
    } else if (format == SUBBITMAP_LIBASS &&
            sb->w == sb->dw && sb->h == sb->dh) {
        // swscale alpha only
        *sba = new_mp_image(sb->w, sb->h);
        mp_image_setfmt(*sba, IMGFMT_Y8);
        (*sba)->planes[0] = sb->bitmap;
        (*sba)->stride[0] = sb->stride;
        int r = (sb->libass.color >> 24) & 0xFF;
        int g = (sb->libass.color >> 16) & 0xFF;
        int b = (sb->libass.color >> 8) & 0xFF;
        int a = sb->libass.color & 0xFF;
        color_yuv[0] =
            rint(MP_MAP_RGB2YUV_COLOR(rgb2yuv, r, g, b, 255, 0)
                    * (1 << (bits - 8)));
        color_yuv[1] =
            rint(MP_MAP_RGB2YUV_COLOR(rgb2yuv, r, g, b, 255, 1)
                    * (1 << (bits - 8)));
        color_yuv[2] =
            rint(MP_MAP_RGB2YUV_COLOR(rgb2yuv, r, g, b, 255, 2)
                    * (1 << (bits - 8)));
        *color_a = 255 - a;
        // NOTE: these overflows can actually happen (when subtitles use color
        // 0,0,0 while output levels only allows 16,16,16 upwards...)
        if (color_yuv[0] < 0)
            color_yuv[0] = 0;
        if (color_yuv[1] < 0)
            color_yuv[1] = 0;
        if (color_yuv[2] < 0)
            color_yuv[2] = 0;
        if (*color_a < 0)
            *color_a = 0;
        if (color_yuv[0] > ((1 << bits) - 1))
            color_yuv[0] = ((1 << bits) - 1);
        if (color_yuv[1] > ((1 << bits) - 1))
            color_yuv[1] = ((1 << bits) - 1);
        if (color_yuv[2] > ((1 << bits) - 1))
            color_yuv[2] = ((1 << bits) - 1);
        if (*color_a > 255)
            *color_a = 255;
        return true;
    } else
        return false;
}

static void mp_image_crop(struct mp_image *img, int x, int y, int w, int h)
{
    int p;
    for (p = 0; p < img->num_planes; ++p) {
        int bits = MP_IMAGE_BITS_PER_PIXEL_ON_PLANE(img, p);
        img->planes[p] +=
            (y >> (p ? img->chroma_y_shift : 0)) * img->stride[p] +
            ((x >> (p ? img->chroma_x_shift : 0)) * bits) / 8;
    }
    img->w = w;
    img->h = h;
}

static bool clip_to_bounds(int *x, int *y, int *w, int *h,
                           int bx, int by, int bw, int bh)
{
    if (*x < bx) {
        *w += *x - bx;
        *x = bx;
    }
    if (*y < 0) {
        *h += *y - by;
        *y = by;
    }
    if (*x + *w > bx + bw)
        *w = bx + bw - *x;
    if (*y + *h > by + bh)
        *h = by + bh - *y;

    if (*w <= 0 || *h <= 0)
        return false;  // nothing left

    return true;
}

static void get_swscale_requirements(int *sx, int *sy,
                                       const struct mp_image *img)
{
    int p;

    if (img->chroma_x_shift == 31)
        *sx = 1;
    else
        *sx = (1 << img->chroma_x_shift);

    if (img->chroma_y_shift == 31)
        *sy = 1;
    else
        *sy = (1 << img->chroma_y_shift);

    for (p = 0; p < img->num_planes; ++p) {
        int bits = MP_IMAGE_BITS_PER_PIXEL_ON_PLANE(img, p);
        // the * 2 fixes problems with writing past the destination width
        while (((*sx >> img->chroma_x_shift) * bits) % (SWS_MIN_BYTE_ALIGN * 8 * 2))
            *sx *= 2;
    }
}

static void align_bbox(int *x1, int *y1, int *x2, int *y2, int xstep, int ystep)
{
    *x1 -= (*x1 % xstep);
    *y1 -= (*y1 % ystep);

    *x2 += xstep - 1;
    *y2 += ystep - 1;
    *x2 -= (*x2 % xstep);
    *y2 -= (*y2 % ystep);
}

static bool align_bbox_to_swscale_requirements(int *x1, int *y1,
                                               int *x2, int *y2,
                                               struct mp_image *img)
{
    int xstep, ystep;
    get_swscale_requirements(&xstep, &ystep, img);
    align_bbox(x1, y1, x2, y2, xstep, ystep);

    if (*x1 < 0)
        *x1 = 0;
    if (*y1 < 0)
        *y1 = 0;
    if (*x2 > img->w)
        *x2 = img->w;
    if (*y2 > img->h)
        *y2 = img->h;

    return (*x2 > *x1) && (*y2 > *y1);
}

// cache: if not NULL, the function will set *cache to a talloc-allocated cache
//        containing scaled versions of sbs contents - free the cache with
//        talloc_free()
void mp_draw_sub_bitmaps(struct mp_draw_sub_cache **cache, struct mp_image *dst,
                         struct sub_bitmaps *sbs, struct mp_csp_details *csp)
{
    int i;
    int x1, y1, x2, y2;
    int color_yuv[3];
    int color_a;
    float yuv2rgb[3][4];
    float rgb2yuv[3][4];

    if (!mp_sws_supported_format(dst->imgfmt))
        return;

    if (cache && !*cache)
        *cache = talloc_zero(NULL, struct mp_draw_sub_cache);

    struct part *part = NULL;

    bool use_cache = sbs->format == SUBBITMAP_RGBA;
    if (cache && use_cache) {
        part = (*cache)->parts[sbs->render_index];
        if (part && part->bitmap_pos_id != sbs->bitmap_pos_id) {
            talloc_free(part);
            part = NULL;
        }
        if (!part) {
            part = talloc_zero(*cache, struct part);
            part->bitmap_pos_id = sbs->bitmap_pos_id;
            part->num_imgs = sbs->num_parts;
            part->imgs = talloc_zero_array(part, struct sub_cache,
                                           part->num_imgs);
        }
        assert(part->num_imgs == sbs->num_parts);
        (*cache)->parts[sbs->render_index] = part;
    }

#ifdef ACCURATE
    int format = IMGFMT_444P16;
    int bits = 16;
    // however, we can try matching 8bit, 9bit, 10bit yuv formats!
    if (dst->flags & MP_IMGFLAG_YUV) {
        if (mp_get_chroma_shift(dst->imgfmt, NULL, NULL, &bits)) {
            switch (bits) {
                case 8:
                    format = IMGFMT_444P;
                    break;
                case 9:
                    format = IMGFMT_444P9;
                    break;
                case 10:
                    format = IMGFMT_444P10;
                    break;
                default:
                    // revert back
                    bits = 16;
                    break;
            }
        }
    }
#else
    int format = IMGFMT_444P;
    int bits = 8;
#endif
    int bytes = (bits + 7) / 8;

    struct mp_csp_params cspar = {
        .colorspace = *csp,
        .brightness = 0, .contrast = 1,
        .hue = 0, .saturation = 1,
        .rgamma = 1, .ggamma = 1, .bgamma = 1,
        .texture_bits = 8, .input_bits = 8
    };

    // prepare YUV/RGB conversion values
    mp_get_yuv2rgb_coeffs(&cspar, yuv2rgb);
    mp_invert_yuv2rgb(rgb2yuv, yuv2rgb);

    //mp_msg(MSGT_VO, MSGL_ERR, "%f %f %f %f // %f %f %f %f // %f %f %f %f\n",
    //        rgb2yuv[0][0],
    //        rgb2yuv[0][1],
    //        rgb2yuv[0][2],
    //        rgb2yuv[0][3],
    //        rgb2yuv[1][0],
    //        rgb2yuv[1][1],
    //        rgb2yuv[1][2],
    //        rgb2yuv[1][3],
    //        rgb2yuv[2][0],
    //        rgb2yuv[2][1],
    //        rgb2yuv[2][2],
    //        rgb2yuv[2][3]);

    // calculate bounding range
    if (!sub_bitmaps_bb(sbs, &x1, &y1, &x2, &y2))
        return;

    if (!align_bbox_to_swscale_requirements(&x1, &y1, &x2, &y2, dst))
        return;  // nothing to do

    // convert to a temp image
    mp_image_t *temp;
    mp_image_t dst_region = *dst;
    if (dst->imgfmt == format) {
        mp_image_crop(&dst_region, x1, y1, x2 - x1, y2 - y1);
        temp = &dst_region;
    } else {
        mp_image_crop(&dst_region, x1, y1, x2 - x1, y2 - y1);
        temp = alloc_mpi(x2 - x1, y2 - y1, format);
        mp_image_swscale(temp, &dst_region, csp, SWS_POINT); // chroma up
    }

    for (i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];
        mp_image_t *sbi = NULL;
        mp_image_t *sba = NULL;

        // cut off areas outside the image
        int dst_x = sb->x - x1; // coordinates are relative to the bbox
        int dst_y = sb->y - y1; // coordinates are relative to the bbox
        int dst_w = sb->dw;
        int dst_h = sb->dh;
        if (!clip_to_bounds(&dst_x, &dst_y, &dst_w, &dst_h,
                            0, 0, temp->w, temp->h))
            continue;

        if (part) {
            sbi = part->imgs[i].i;
            sba = part->imgs[i].a;
        }

        if (!(sbi && sba)) {
            if (!sub_bitmap_to_mp_images(&sbi, color_yuv, &color_a, &sba, sb,
                                         sbs->format, csp, rgb2yuv, format,
                                         bits))
            {
                mp_msg(MSGT_VO, MSGL_ERR,
                       "render_sub_bitmap: invalid sub bitmap type\n");
                continue;
            }
        }

        // call blend_alpha 3 times
        int p;
        int src_x = (dst_x + x1) - sb->x;
        int src_y = (dst_y + y1) - sb->y;
        unsigned char *alpha_p =
            sba->planes[0] + src_y * sba->stride[0] + src_x;
        for (p = 0; p < 3; ++p) {
            unsigned char *dst_p =
                temp->planes[p] + dst_y * temp->stride[p] + dst_x * bytes;
            if (sbi) {
                unsigned char *src_p =
                    sbi->planes[p] + src_y * sbi->stride[p] + src_x * bytes;
                blend_src_alpha(
                    dst_p, temp->stride[p],
                    src_p, sbi->stride[p],
                    alpha_p, sba->stride[0],
                    dst_h, dst_w, bytes
                    );
            } else {
                blend_const_alpha(
                    dst_p, temp->stride[p],
                    color_yuv[p],
                    alpha_p, sba->stride[0], color_a,
                    dst_h, dst_w, bytes
                    );
            }
        }

        if (part) {
            part->imgs[i].i = talloc_steal(part, sbi);
            part->imgs[i].a = talloc_steal(part, sba);
        } else {
            free_mp_image(sbi);
            free_mp_image(sba);
        }
    }

    if (temp != &dst_region) {
        // convert back
        mp_image_swscale(&dst_region, temp, csp, SWS_AREA); // chroma down

        // clean up
        free_mp_image(temp);
    }
}

// vim: ts=4 sw=4 et tw=80
