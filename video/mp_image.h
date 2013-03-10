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

#ifndef MPLAYER_MP_IMAGE_H
#define MPLAYER_MP_IMAGE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "core/mp_msg.h"
#include "csputils.h"
#include "video/img_format.h"

#define MP_PALETTE_SIZE (256 * 4)

#define MP_IMGFIELD_ORDERED 0x01
#define MP_IMGFIELD_TOP_FIRST 0x02
#define MP_IMGFIELD_REPEAT_FIRST 0x04
#define MP_IMGFIELD_TOP 0x08
#define MP_IMGFIELD_BOTTOM 0x10
#define MP_IMGFIELD_INTERLACED 0x20

/* Memory management:
 * - mp_image is a light-weight reference to the actual image data (pixels).
 *   The actual image data is reference counted and can outlive mp_image
 *   allocations. mp_image references can be created with mp_image_new_ref()
 *   and free'd with talloc_free() (the helpers mp_image_setrefp() and
 *   mp_image_unrefp() can also be used). The actual image data is free'd when
 *   the last mp_image reference to it is free'd.
 * - Each mp_image has a clear owner. The owner can do anything with it, such
 *   as changing mp_image fields. Instead of making ownership ambiguous by
 *   sharing a mp_image reference, new references should be created.
 * - Write access to the actual image data is allowed only after calling
 *   mp_image_make_writeable(), or if mp_image_is_writeable() returns true.
 *   Conceptually, images can be changed by their owner only, and copy-on-write
 *   is used to ensure that other references do not see any changes to the
 *   image data. mp_image_make_writeable() will do that copy if required.
 */
typedef struct mp_image {
    unsigned int flags;
    struct mp_imgfmt_desc fmt;

    // fields redundant to fmt, for convenience or compatibility
    unsigned int imgfmt;
    int num_planes;
    int chroma_x_shift; // horizontal
    int chroma_y_shift; // vertical

    int w,h;  // visible dimensions
    int display_w,display_h; // if set (!= 0), anamorphic size
    uint8_t *planes[MP_MAX_PLANES];
    int stride[MP_MAX_PLANES];

    char * qscale;
    int qstride;
    int pict_type; // 0->unknown, 1->I, 2->P, 3->B
    int fields;
    int qscale_type; // 0->mpeg1/4/h263, 1->mpeg2

    /* redundant */
    int chroma_width;
    int chroma_height;
    int plane_w[MP_MAX_PLANES];
    int plane_h[MP_MAX_PLANES];

    enum mp_csp colorspace;
    enum mp_csp_levels levels;
    /* only inside filter chain */
    double pts;
    /* memory management */
    struct m_refcount *refcount;
    /* for private use */
    void* priv;
} mp_image_t;

struct mp_image *mp_image_alloc(unsigned int fmt, int w, int h);
void mp_image_copy(struct mp_image *dmpi, struct mp_image *mpi);
void mp_image_copy_attributes(struct mp_image *dmpi, struct mp_image *mpi);
struct mp_image *mp_image_new_copy(struct mp_image *img);
struct mp_image *mp_image_new_ref(struct mp_image *img);
bool mp_image_is_writeable(struct mp_image *img);
void mp_image_make_writeable(struct mp_image *img);
void mp_image_setrefp(struct mp_image **p_img, struct mp_image *new_value);
void mp_image_unrefp(struct mp_image **p_img);

void mp_image_clear(struct mp_image *mpi, int x0, int y0, int x1, int y1);
void mp_image_crop(struct mp_image *img, int x0, int y0, int x1, int y1);
void mp_image_crop_rc(struct mp_image *img, struct mp_rect rc);
void mp_image_vflip(struct mp_image *img);

void mp_image_set_size(struct mp_image *mpi, int w, int h);
void mp_image_set_display_size(struct mp_image *mpi, int dw, int dh);

void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt);
void mp_image_steal_data(struct mp_image *dst, struct mp_image *src);

struct mp_image *mp_image_new_custom_ref(struct mp_image *img, void *arg,
                                         void (*free)(void *arg));

struct mp_image *mp_image_new_external_ref(struct mp_image *img, void *arg,
                                           void (*ref)(void *arg),
                                           void (*unref)(void *arg),
                                           bool (*is_unique)(void *arg),
                                           void (*free)(void *arg));

enum mp_csp mp_image_csp(struct mp_image *img);
enum mp_csp_levels mp_image_levels(struct mp_image *img);

struct mp_csp_details;
void mp_image_set_colorspace_details(struct mp_image *image,
                                     struct mp_csp_details *csp);

struct AVFrame;
void mp_image_copy_fields_from_av_frame(struct mp_image *dst,
                                        struct AVFrame *src);
void mp_image_copy_fields_to_av_frame(struct AVFrame *dst,
                                      struct mp_image *src);
struct mp_image *mp_image_from_av_frame(struct AVFrame *av_frame);
struct AVFrame *mp_image_to_av_frame_and_unref(struct mp_image *img);

// align must be a power of two (align >= 1), v >= 0
#define MP_ALIGN_UP(v, align) FFALIGN(v, align)
#define MP_ALIGN_DOWN(v, align) ((v) & ~((align) - 1))

#endif /* MPLAYER_MP_IMAGE_H */
