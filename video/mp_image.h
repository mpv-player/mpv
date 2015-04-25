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
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_MP_IMAGE_H
#define MPLAYER_MP_IMAGE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "common/common.h"
#include "common/msg.h"
#include "csputils.h"
#include "video/img_format.h"

#define MP_PALETTE_SIZE (256 * 4)

#define MP_IMGFIELD_TOP_FIRST 0x02
#define MP_IMGFIELD_REPEAT_FIRST 0x04
#define MP_IMGFIELD_INTERLACED 0x20

// Describes image parameters that usually stay constant.
// New fields can be added in the future. Code changing the parameters should
// usually copy the whole struct, so that fields added later will be preserved.
struct mp_image_params {
    enum mp_imgfmt imgfmt;      // pixel format
    int w, h;                   // image dimensions
    int d_w, d_h;               // define display aspect ratio (never 0/0)
    enum mp_csp colorspace;
    enum mp_csp_levels colorlevels;
    enum mp_csp_prim primaries;
    enum mp_csp_trc gamma;
    enum mp_chroma_location chroma_location;
    // The image should be converted to these levels. Unlike colorlevels, it
    // does not describe the current state of the image. (Somewhat similar to
    // d_w/d_h vs. w/h.)
    enum mp_csp_levels outputlevels;
    // The image should be rotated clockwise (0-359 degrees).
    int rotate;
    enum mp_stereo3d_mode stereo_in;    // image is encoded with this mode
    enum mp_stereo3d_mode stereo_out;   // should be displayed with this mode
};

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
    int w, h;  // visible dimensions (redundant with params.w/h)

    struct mp_image_params params;

    // fields redundant to params.imgfmt, for convenience or compatibility
    struct mp_imgfmt_desc fmt;
    enum mp_imgfmt imgfmt;
    int num_planes;

    uint8_t *planes[MP_MAX_PLANES];
    int stride[MP_MAX_PLANES];

    int pict_type; // 0->unknown, 1->I, 2->P, 3->B
    int fields;

    /* only inside filter chain */
    double pts;
    /* memory management */
    struct m_refcount *refcount;
    /* for private use */
    void* priv;
} mp_image_t;

int mp_chroma_div_up(int size, int shift);

struct mp_image *mp_image_alloc(int fmt, int w, int h);
void mp_image_copy(struct mp_image *dmpi, struct mp_image *mpi);
void mp_image_copy_attributes(struct mp_image *dmpi, struct mp_image *mpi);
struct mp_image *mp_image_new_copy(struct mp_image *img);
struct mp_image *mp_image_new_ref(struct mp_image *img);
bool mp_image_is_writeable(struct mp_image *img);
bool mp_image_make_writeable(struct mp_image *img);
void mp_image_setrefp(struct mp_image **p_img, struct mp_image *new_value);
void mp_image_unrefp(struct mp_image **p_img);

void mp_image_clear(struct mp_image *mpi, int x0, int y0, int x1, int y1);
void mp_image_crop(struct mp_image *img, int x0, int y0, int x1, int y1);
void mp_image_crop_rc(struct mp_image *img, struct mp_rect rc);
void mp_image_vflip(struct mp_image *img);

void mp_image_set_size(struct mp_image *mpi, int w, int h);
int mp_image_plane_w(struct mp_image *mpi, int plane);
int mp_image_plane_h(struct mp_image *mpi, int plane);

void mp_image_setfmt(mp_image_t* mpi, int out_fmt);
void mp_image_steal_data(struct mp_image *dst, struct mp_image *src);

struct mp_image *mp_image_new_custom_ref(struct mp_image *img, void *arg,
                                         void (*free)(void *arg));

void mp_image_params_guess_csp(struct mp_image_params *params);

char *mp_image_params_to_str_buf(char *b, size_t bs,
                                 const struct mp_image_params *p);
#define mp_image_params_to_str(p) mp_image_params_to_str_buf((char[80]){0}, 80, p)

bool mp_image_params_valid(const struct mp_image_params *p);
bool mp_image_params_equal(const struct mp_image_params *p1,
                           const struct mp_image_params *p2);

void mp_image_set_params(struct mp_image *image,
                         const struct mp_image_params *params);

void mp_image_set_attributes(struct mp_image *image,
                             const struct mp_image_params *params);

struct AVFrame;
void mp_image_copy_fields_from_av_frame(struct mp_image *dst,
                                        struct AVFrame *src);
void mp_image_copy_fields_to_av_frame(struct AVFrame *dst,
                                      struct mp_image *src);
struct mp_image *mp_image_from_av_frame(struct AVFrame *av_frame);
struct AVFrame *mp_image_to_av_frame_and_unref(struct mp_image *img);

void memcpy_pic(void *dst, const void *src, int bytesPerLine, int height,
                int dstStride, int srcStride);
void memset_pic(void *dst, int fill, int bytesPerLine, int height, int stride);
void memset16_pic(void *dst, int fill, int unitsPerLine, int height, int stride);

#endif /* MPLAYER_MP_IMAGE_H */
