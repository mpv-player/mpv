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

#include <limits.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>
#include <libavutil/display.h>
#include <libavutil/dovi_meta.h>
#include <libavutil/bswap.h>
#include <libavutil/hwcontext.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/rational.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mastering_display_metadata.h>
#include <libplacebo/utils/libav.h>

#include "mpv_talloc.h"

#include "common/av_common.h"
#include "common/common.h"
#include "fmt-conversion.h"
#include "hwdec.h"
#include "mp_image.h"
#include "osdep/threads.h"
#include "sws_utils.h"
#include "out/placebo/utils.h"

// Determine strides, plane sizes, and total required size for an image
// allocation. Returns total size on success, <0 on error. Unused planes
// have out_stride/out_plane_size to 0, and out_plane_offset set to -1 up
// until MP_MAX_PLANES-1.
static int mp_image_layout(int imgfmt, int w, int h, int stride_align,
                           int out_stride[MP_MAX_PLANES],
                           int out_plane_offset[MP_MAX_PLANES],
                           int out_plane_size[MP_MAX_PLANES])
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);

    w = MP_ALIGN_UP(w, desc.align_x);
    h = MP_ALIGN_UP(h, desc.align_y);

    struct mp_image_params params = {.imgfmt = imgfmt, .w = w, .h = h};

    if (!mp_image_params_valid(&params) || desc.flags & MP_IMGFLAG_HWACCEL)
        return -1;

    // Note: for non-mod-2 4:2:0 YUV frames, we have to allocate an additional
    //       top/right border. This is needed for correct handling of such
    //       images in filter and VO code (e.g. vo_vdpau or vo_gpu).

    for (int n = 0; n < MP_MAX_PLANES; n++) {
        int alloc_w = mp_chroma_div_up(w, desc.xs[n]);
        int alloc_h = MP_ALIGN_UP(h, 32) >> desc.ys[n];
        int line_bytes = (alloc_w * desc.bpp[n] + 7) / 8;
        out_stride[n] = MP_ALIGN_NPOT(line_bytes, stride_align);
        out_plane_size[n] = out_stride[n] * alloc_h;
    }
    if (desc.flags & MP_IMGFLAG_PAL)
        out_plane_size[1] = AVPALETTE_SIZE;

    int sum = 0;
    for (int n = 0; n < MP_MAX_PLANES; n++) {
        out_plane_offset[n] = out_plane_size[n] ? sum : -1;
        sum += out_plane_size[n];
    }

    return sum;
}

// Return the total size needed for an image allocation of the given
// configuration (imgfmt, w, h must be set). Returns -1 on error.
// Assumes the allocation is already aligned on stride_align (otherwise you
// need to add padding yourself).
int mp_image_get_alloc_size(int imgfmt, int w, int h, int stride_align)
{
    int stride[MP_MAX_PLANES];
    int plane_offset[MP_MAX_PLANES];
    int plane_size[MP_MAX_PLANES];
    return mp_image_layout(imgfmt, w, h, stride_align, stride, plane_offset,
                           plane_size);
}

// Fill the mpi->planes and mpi->stride fields of the given mpi with data
// from buffer according to the mpi's w/h/imgfmt fields. See mp_image_from_buffer
// aboud remarks how to allocate/use buffer/buffer_size.
// This does not free the data. You are expected to setup refcounting by
// setting mp_image.bufs before or after this function is called.
// Returns true on success, false on failure.
static bool mp_image_fill_alloc(struct mp_image *mpi, int stride_align,
                                void *buffer, int buffer_size)
{
    int stride[MP_MAX_PLANES];
    int plane_offset[MP_MAX_PLANES];
    int plane_size[MP_MAX_PLANES];
    int size = mp_image_layout(mpi->imgfmt, mpi->w, mpi->h, stride_align,
                               stride, plane_offset, plane_size);
    if (size < 0 || size > buffer_size)
        return false;

    int align = MP_ALIGN_UP((uintptr_t)buffer, stride_align) - (uintptr_t)buffer;
    if (buffer_size - size < align)
        return false;
    uint8_t *s = buffer;
    s += align;

    for (int n = 0; n < MP_MAX_PLANES; n++) {
        mpi->planes[n] = plane_offset[n] >= 0 ? s + plane_offset[n] : NULL;
        mpi->stride[n] = stride[n];
    }

    return true;
}

// Create a mp_image from the provided buffer. The mp_image is filled according
// to the imgfmt/w/h parameters, and respecting the stride_align parameter to
// align the plane start pointers and strides. Once the last reference to the
// returned image is destroyed, free(free_opaque, buffer) is called. (Be aware
// that this can happen from any thread.)
// The allocated size of buffer must be given by buffer_size. buffer_size should
// be at least the value returned by mp_image_get_alloc_size(). If buffer is not
// already aligned to stride_align, the function will attempt to align the
// pointer itself by incrementing the buffer pointer until their alignment is
// achieved (if buffer_size is not large enough to allow aligning the buffer
// safely, the function fails). To be safe, you may want to overallocate the
// buffer by stride_align bytes, and include the overallocation in buffer_size.
// Returns NULL on failure. On failure, the free() callback is not called.
struct mp_image *mp_image_from_buffer(int imgfmt, int w, int h, int stride_align,
                                      uint8_t *buffer, int buffer_size,
                                      void *free_opaque,
                                      void (*free)(void *opaque, uint8_t *data))
{
    struct mp_image *mpi = mp_image_new_dummy_ref(NULL);
    mp_image_setfmt(mpi, imgfmt);
    mp_image_set_size(mpi, w, h);

    if (!mp_image_fill_alloc(mpi, stride_align, buffer, buffer_size))
        goto fail;

    mpi->bufs[0] = av_buffer_create(buffer, buffer_size, free, free_opaque, 0);
    if (!mpi->bufs[0])
        goto fail;

    return mpi;

fail:
    talloc_free(mpi);
    return NULL;
}

static bool mp_image_alloc_planes(struct mp_image *mpi)
{
    mp_assert(!mpi->planes[0]);
    mp_assert(!mpi->bufs[0]);

    int align = MP_IMAGE_BYTE_ALIGN;

    int size = mp_image_get_alloc_size(mpi->imgfmt, mpi->w, mpi->h, align);
    if (size < 0)
        return false;

    // Note: mp_image_pool assumes this creates only 1 AVBufferRef.
    mpi->bufs[0] = av_buffer_alloc(size + align);
    if (!mpi->bufs[0])
        return false;

    if (!mp_image_fill_alloc(mpi, align, mpi->bufs[0]->data, mpi->bufs[0]->size)) {
        av_buffer_unref(&mpi->bufs[0]);
        return false;
    }

    return true;
}

void mp_image_setfmt(struct mp_image *mpi, int out_fmt)
{
    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(out_fmt);
    mpi->params.imgfmt = fmt.id;
    mpi->fmt = fmt;
    mpi->imgfmt = fmt.id;
    mpi->num_planes = fmt.num_planes;
}

static void mp_image_destructor(void *ptr)
{
    mp_image_t *mpi = ptr;
    for (int p = 0; p < MP_MAX_PLANES; p++)
        av_buffer_unref(&mpi->bufs[p]);
    av_buffer_unref(&mpi->hwctx);
    av_buffer_unref(&mpi->icc_profile);
    av_buffer_unref(&mpi->a53_cc);
    av_buffer_unref(&mpi->dovi);
    av_buffer_unref(&mpi->film_grain);
    for (int n = 0; n < mpi->num_ff_side_data; n++)
        av_buffer_unref(&mpi->ff_side_data[n].buf);
    talloc_free(mpi->ff_side_data);
}

int mp_chroma_div_up(int size, int shift)
{
    return (size + (1 << shift) - 1) >> shift;
}

// Return the storage width in pixels of the given plane.
int mp_image_plane_w(struct mp_image *mpi, int plane)
{
    return mp_chroma_div_up(mpi->w, mpi->fmt.xs[plane]);
}

// Return the storage height in pixels of the given plane.
int mp_image_plane_h(struct mp_image *mpi, int plane)
{
    return mp_chroma_div_up(mpi->h, mpi->fmt.ys[plane]);
}

// Caller has to make sure this doesn't exceed the allocated plane data/strides.
void mp_image_set_size(struct mp_image *mpi, int w, int h)
{
    mp_assert(w >= 0 && h >= 0);
    mpi->w = mpi->params.w = w;
    mpi->h = mpi->params.h = h;
}

void mp_image_set_params(struct mp_image *image,
                         const struct mp_image_params *params)
{
    // possibly initialize other stuff
    mp_image_setfmt(image, params->imgfmt);
    mp_image_set_size(image, params->w, params->h);
    image->params = *params;
}

struct mp_image *mp_image_alloc(int imgfmt, int w, int h)
{
    struct mp_image *mpi = talloc_zero(NULL, struct mp_image);
    talloc_set_destructor(mpi, mp_image_destructor);

    mp_image_set_size(mpi, w, h);
    mp_image_setfmt(mpi, imgfmt);
    if (!mp_image_alloc_planes(mpi)) {
        talloc_free(mpi);
        return NULL;
    }
    return mpi;
}

int mp_image_approx_byte_size(struct mp_image *img)
{
    int total = sizeof(*img);

    for (int n = 0; n < MP_MAX_PLANES; n++) {
        struct AVBufferRef *buf = img->bufs[n];
        if (buf)
            total += buf->size;
    }

    return total;
}

struct mp_image *mp_image_new_copy(struct mp_image *img)
{
    struct mp_image *new = mp_image_alloc(img->imgfmt, img->w, img->h);
    if (!new)
        return NULL;
    mp_image_copy(new, img);
    mp_image_copy_attributes(new, img);
    return new;
}

// Make dst take over the image data of src, and free src.
// This is basically a safe version of *dst = *src; free(src);
// Only works with ref-counted images, and can't change image size/format.
void mp_image_steal_data(struct mp_image *dst, struct mp_image *src)
{
    mp_assert(dst->imgfmt == src->imgfmt && dst->w == src->w && dst->h == src->h);
    mp_assert(dst->bufs[0] && src->bufs[0]);

    mp_image_destructor(dst); // unref old
    talloc_free_children(dst);

    *dst = *src;

    *src = (struct mp_image){0};
    talloc_free(src);
}

// Unref most data buffer (and clear the data array), but leave other fields
// allocated. In particular, mp_image.hwctx is preserved.
void mp_image_unref_data(struct mp_image *img)
{
    for (int n = 0; n < MP_MAX_PLANES; n++) {
        img->planes[n] = NULL;
        img->stride[n] = 0;
        av_buffer_unref(&img->bufs[n]);
    }
}

static void ref_buffer(AVBufferRef **dst)
{
    if (*dst) {
        *dst = av_buffer_ref(*dst);
        MP_HANDLE_OOM(*dst);
    }
}

// Return a new reference to img. The returned reference is owned by the caller,
// while img is left untouched.
struct mp_image *mp_image_new_ref(struct mp_image *img)
{
    if (!img)
        return NULL;

    if (!img->bufs[0])
        return mp_image_new_copy(img);

    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = *img;

    for (int p = 0; p < MP_MAX_PLANES; p++)
        ref_buffer(&new->bufs[p]);

    ref_buffer(&new->hwctx);
    ref_buffer(&new->icc_profile);
    ref_buffer(&new->a53_cc);
    ref_buffer(&new->dovi);
    ref_buffer(&new->film_grain);

    new->ff_side_data = talloc_memdup(NULL, new->ff_side_data,
                        new->num_ff_side_data * sizeof(new->ff_side_data[0]));
    for (int n = 0; n < new->num_ff_side_data; n++)
        ref_buffer(&new->ff_side_data[n].buf);

    return new;
}

struct free_args {
    void *arg;
    void (*free)(void *arg);
};

static void call_free(void *opaque, uint8_t *data)
{
    struct free_args *args = opaque;
    args->free(args->arg);
    talloc_free(args);
}

// Create a new mp_image based on img, but don't set any buffers.
// Using this is only valid until the original img is unreferenced (including
// implicit unreferencing of the data by mp_image_make_writeable()), unless
// a new reference is set.
struct mp_image *mp_image_new_dummy_ref(struct mp_image *img)
{
    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = img ? *img : (struct mp_image){0};
    for (int p = 0; p < MP_MAX_PLANES; p++)
        new->bufs[p] = NULL;
    new->hwctx = NULL;
    new->icc_profile = NULL;
    new->a53_cc = NULL;
    new->dovi = NULL;
    new->film_grain = NULL;
    new->num_ff_side_data = 0;
    new->ff_side_data = NULL;
    return new;
}

// Return a reference counted reference to img. If the reference count reaches
// 0, call free(free_arg). The data passed by img must not be free'd before
// that. The new reference will be writeable.
// On allocation failure, unref the frame and return NULL.
// This is only used for hw decoding; this is important, because libav* expects
// all plane data to be accounted for by AVBufferRefs.
struct mp_image *mp_image_new_custom_ref(struct mp_image *img, void *free_arg,
                                         void (*free)(void *arg))
{
    struct mp_image *new = mp_image_new_dummy_ref(img);

    struct free_args *args = talloc_ptrtype(NULL, args);
    *args = (struct free_args){free_arg, free};
    new->bufs[0] = av_buffer_create(NULL, 0, call_free, args,
                                    AV_BUFFER_FLAG_READONLY);
    if (new->bufs[0])
        return new;
    talloc_free(new);
    return NULL;
}

bool mp_image_is_writeable(struct mp_image *img)
{
    if (!img->bufs[0])
        return true; // not ref-counted => always considered writeable
    for (int p = 0; p < MP_MAX_PLANES; p++) {
        if (!img->bufs[p])
            break;
        if (!av_buffer_is_writable(img->bufs[p]))
            return false;
    }
    return true;
}

// Make the image data referenced by img writeable. This allocates new data
// if the data wasn't already writeable, and img->planes[] and img->stride[]
// will be set to the copy.
// Returns success; if false is returned, the image could not be made writeable.
bool mp_image_make_writeable(struct mp_image *img)
{
    if (mp_image_is_writeable(img))
        return true;

    struct mp_image *new = mp_image_new_copy(img);
    if (!new)
        return false;
    mp_image_steal_data(img, new);
    mp_assert(mp_image_is_writeable(img));
    return true;
}

// Helper function: unrefs *p_img, and sets *p_img to a new ref of new_value.
// Only unrefs *p_img and sets it to NULL if out of memory.
void mp_image_setrefp(struct mp_image **p_img, struct mp_image *new_value)
{
    if (*p_img != new_value) {
        talloc_free(*p_img);
        *p_img = new_value ? mp_image_new_ref(new_value) : NULL;
    }
}

// Mere helper function (mp_image can be directly free'd with talloc_free)
void mp_image_unrefp(struct mp_image **p_img)
{
    talloc_free(*p_img);
    *p_img = NULL;
}

void memcpy_pic(void *dst, const void *src, int bytesPerLine, int height,
                int dstStride, int srcStride)
{
    if (bytesPerLine == dstStride && dstStride == srcStride && height) {
        if (srcStride < 0) {
            src = (uint8_t*)src + (height - 1) * srcStride;
            dst = (uint8_t*)dst + (height - 1) * dstStride;
            srcStride = -srcStride;
        }

        memcpy(dst, src, srcStride * (height - 1) + bytesPerLine);
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(dst, src, bytesPerLine);
            src = (uint8_t*)src + srcStride;
            dst = (uint8_t*)dst + dstStride;
        }
    }
}

void mp_image_copy(struct mp_image *dst, struct mp_image *src)
{
    mp_assert(dst->imgfmt == src->imgfmt);
    mp_assert(dst->w == src->w && dst->h == src->h);
    mp_assert(mp_image_is_writeable(dst));
    for (int n = 0; n < dst->num_planes; n++) {
        int line_bytes = (mp_image_plane_w(dst, n) * dst->fmt.bpp[n] + 7) / 8;
        int plane_h = mp_image_plane_h(dst, n);
        memcpy_pic(dst->planes[n], src->planes[n], line_bytes, plane_h,
                   dst->stride[n], src->stride[n]);
    }
    if (dst->fmt.flags & MP_IMGFLAG_PAL)
        memcpy(dst->planes[1], src->planes[1], AVPALETTE_SIZE);
}

static enum pl_color_system mp_image_params_get_forced_csp(struct mp_image_params *params)
{
    int imgfmt = params->hw_subfmt ? params->hw_subfmt : params->imgfmt;
    enum pl_color_system csp = mp_imgfmt_get_forced_csp(imgfmt);

    if (csp == PL_COLOR_SYSTEM_RGB && params->repr.sys == PL_COLOR_SYSTEM_XYZ)
        csp = PL_COLOR_SYSTEM_XYZ;

    return csp;
}

static void assign_bufref(AVBufferRef **dst, AVBufferRef *new)
{
    av_buffer_unref(dst);
    if (new) {
        *dst = av_buffer_ref(new);
        MP_HANDLE_OOM(*dst);
    }
}

void mp_image_copy_attributes(struct mp_image *dst, struct mp_image *src)
{
    mp_assert(dst != src);

    dst->pict_type = src->pict_type;
    dst->fields = src->fields;
    dst->pts = src->pts;
    dst->dts = src->dts;
    dst->pkt_duration = src->pkt_duration;
    dst->params.rotate = src->params.rotate;
    dst->params.stereo3d = src->params.stereo3d;
    dst->params.p_w = src->params.p_w;
    dst->params.p_h = src->params.p_h;
    dst->params.color = src->params.color;
    dst->params.repr = src->params.repr;
    dst->params.light = src->params.light;
    dst->params.chroma_location = src->params.chroma_location;
    dst->params.crop = src->params.crop;
    dst->nominal_fps = src->nominal_fps;
    dst->params.primaries_orig = src->params.primaries_orig;
    dst->params.transfer_orig = src->params.transfer_orig;
    dst->params.sys_orig = src->params.sys_orig;

    // ensure colorspace consistency
    enum pl_color_system dst_forced_csp = mp_image_params_get_forced_csp(&dst->params);
    if (mp_image_params_get_forced_csp(&src->params) != dst_forced_csp) {
        dst->params.repr.sys = dst_forced_csp != PL_COLOR_SYSTEM_UNKNOWN ?
                                    dst_forced_csp :
                                    mp_csp_guess_colorspace(src->w, src->h);
    }

    if ((dst->fmt.flags & MP_IMGFLAG_PAL) && (src->fmt.flags & MP_IMGFLAG_PAL)) {
        if (dst->planes[1] && src->planes[1]) {
            if (mp_image_make_writeable(dst))
                memcpy(dst->planes[1], src->planes[1], AVPALETTE_SIZE);
        }
    }
    assign_bufref(&dst->icc_profile, src->icc_profile);
    assign_bufref(&dst->dovi, src->dovi);
    assign_bufref(&dst->film_grain, src->film_grain);
    assign_bufref(&dst->a53_cc, src->a53_cc);

    for (int n = 0; n < dst->num_ff_side_data; n++)
        av_buffer_unref(&dst->ff_side_data[n].buf);

    MP_RESIZE_ARRAY(NULL, dst->ff_side_data, src->num_ff_side_data);
    dst->num_ff_side_data = src->num_ff_side_data;

    for (int n = 0; n < dst->num_ff_side_data; n++) {
        dst->ff_side_data[n].type = src->ff_side_data[n].type;
        dst->ff_side_data[n].buf = av_buffer_ref(src->ff_side_data[n].buf);
        MP_HANDLE_OOM(dst->ff_side_data[n].buf);
    }
}

// Crop the given image to (x0, y0)-(x1, y1) (bottom/right border exclusive)
// x0/y0 must be naturally aligned.
void mp_image_crop(struct mp_image *img, int x0, int y0, int x1, int y1)
{
    mp_assert(x0 >= 0 && y0 >= 0);
    mp_assert(x0 <= x1 && y0 <= y1);
    mp_assert(x1 <= img->w && y1 <= img->h);
    mp_assert(!(x0 & (img->fmt.align_x - 1)));
    mp_assert(!(y0 & (img->fmt.align_y - 1)));

    for (int p = 0; p < img->num_planes; ++p) {
        img->planes[p] += (y0 >> img->fmt.ys[p]) * img->stride[p] +
                          (x0 >> img->fmt.xs[p]) * img->fmt.bpp[p] / 8;
    }
    mp_image_set_size(img, x1 - x0, y1 - y0);
}

void mp_image_crop_rc(struct mp_image *img, struct mp_rect rc)
{
    mp_image_crop(img, rc.x0, rc.y0, rc.x1, rc.y1);
}

// Repeatedly write count patterns of src[0..src_size] to p.
static void memset_pattern(void *p, size_t count, uint8_t *src, size_t src_size)
{
    mp_assert(src_size >= 1);

    if (src_size == 1) {
        memset(p, src[0], count);
    } else if (src_size == 2) { // >8 bit YUV => common, be slightly less naive
        uint16_t val;
        memcpy(&val, src, 2);
        uint16_t *p16 = p;
        while (count--)
            *p16++ = val;
    } else {
        while (count--) {
            memcpy(p, src, src_size);
            p = (char *)p + src_size;
        }
    }
}

static bool endian_swap_bytes(void *d, size_t bytes, size_t word_size)
{
    if (word_size != 2 && word_size != 4)
        return false;

    size_t num_words = bytes / word_size;
    uint8_t *ud = d;

    switch (word_size) {
    case 2:
        for (size_t x = 0; x < num_words; x++)
            AV_WL16(ud + x * 2, AV_RB16(ud + x * 2));
        break;
    case 4:
        for (size_t x = 0; x < num_words; x++)
            AV_WL32(ud + x * 2, AV_RB32(ud + x * 2));
        break;
    default:
        MP_ASSERT_UNREACHABLE();
    }

    return true;
}

// Bottom/right border is allowed not to be aligned, but it might implicitly
// overwrite pixel data until the alignment (align_x/align_y) is reached.
// Alpha is cleared to 0 (fully transparent).
void mp_image_clear(struct mp_image *img, int x0, int y0, int x1, int y1)
{
    mp_assert(x0 >= 0 && y0 >= 0);
    mp_assert(x0 <= x1 && y0 <= y1);
    mp_assert(x1 <= img->w && y1 <= img->h);
    mp_assert(!(x0 & (img->fmt.align_x - 1)));
    mp_assert(!(y0 & (img->fmt.align_y - 1)));

    struct mp_image area = *img;
    struct mp_imgfmt_desc *fmt = &area.fmt;
    mp_image_crop(&area, x0, y0, x1, y1);

    // "Black" color for each plane.
    uint8_t plane_clear[MP_MAX_PLANES][8] = {0};
    int plane_size[MP_MAX_PLANES] = {0};
    int misery = 1; // pixel group width

    // YUV integer chroma needs special consideration, and technically luma is
    // usually not 0 either.
    if ((fmt->flags & (MP_IMGFLAG_HAS_COMPS | MP_IMGFLAG_PACKED_SS_YUV)) &&
        (fmt->flags & MP_IMGFLAG_TYPE_MASK) == MP_IMGFLAG_TYPE_UINT &&
        (fmt->flags & MP_IMGFLAG_COLOR_MASK) == MP_IMGFLAG_COLOR_YUV)
    {
        uint64_t plane_clear_i[MP_MAX_PLANES] = {0};

        // Need to handle "multiple" pixels with packed YUV.
        uint8_t luma_offsets[4] = {0};
        if (fmt->flags & MP_IMGFLAG_PACKED_SS_YUV) {
            misery = fmt->align_x;
            if (misery <= MP_ARRAY_SIZE(luma_offsets)) // ignore if out of bounds
                mp_imgfmt_get_packed_yuv_locations(fmt->id, luma_offsets);
        }

        for (int c = 0; c < 4; c++) {
            struct mp_imgfmt_comp_desc *cd = &fmt->comps[c];
            int plane_bits = fmt->bpp[cd->plane] * misery;
            if (plane_bits <= 64 && plane_bits % 8u == 0 && cd->size) {
                plane_size[cd->plane] = plane_bits / 8u;
                int depth = cd->size + MPMIN(cd->pad, 0);
                double m, o;
                mp_get_csp_uint_mul(area.params.repr.sys,
                                    area.params.repr.levels,
                                    depth, c + 1, &m, &o);
                uint64_t val = MPCLAMP(lrint((0 - o) / m), 0, 1ull << depth);
                plane_clear_i[cd->plane] |= val << cd->offset;
                for (int x = 1; x < (c ? 0 : misery); x++)
                    plane_clear_i[cd->plane] |= val << luma_offsets[x];
            }
        }

        for (int p = 0; p < MP_MAX_PLANES; p++) {
            if (!plane_clear_i[p])
                plane_size[p] = 0;
            memcpy(&plane_clear[p][0], &plane_clear_i[p], 8); // endian dependent

            if (fmt->endian_shift) {
                endian_swap_bytes(&plane_clear[p][0], plane_size[p],
                                  1 << fmt->endian_shift);
            }
        }
    }

    for (int p = 0; p < area.num_planes; p++) {
        int p_h = mp_image_plane_h(&area, p);
        int p_w = mp_image_plane_w(&area, p);
        for (int y = 0; y < p_h; y++) {
            void *ptr = area.planes[p] + (ptrdiff_t)area.stride[p] * y;
            if (plane_size[p]) {
                memset_pattern(ptr, p_w / misery, plane_clear[p], plane_size[p]);
            } else {
                memset(ptr, 0, mp_image_plane_bytes(&area, p, 0, area.w));
            }
        }
    }
}

void mp_image_clear_rc(struct mp_image *mpi, struct mp_rect rc)
{
    mp_image_clear(mpi, rc.x0, rc.y0, rc.x1, rc.y1);
}

// Clear the are of the image _not_ covered by rc.
void mp_image_clear_rc_inv(struct mp_image *mpi, struct mp_rect rc)
{
    struct mp_rect clr[4];
    int cnt = mp_rect_subtract(&(struct mp_rect){0, 0, mpi->w, mpi->h}, &rc, clr);
    for (int n = 0; n < cnt; n++)
        mp_image_clear_rc(mpi, clr[n]);
}

void mp_image_vflip(struct mp_image *img)
{
    for (int p = 0; p < img->num_planes; p++) {
        int plane_h = mp_image_plane_h(img, p);
        img->planes[p] = img->planes[p] + img->stride[p] * (plane_h - 1);
        img->stride[p] = -img->stride[p];
    }
}

bool mp_image_crop_valid(const struct mp_image_params *p)
{
    return p->crop.x1 > p->crop.x0 && p->crop.y1 > p->crop.y0 &&
           p->crop.x0 >= 0 && p->crop.y0 >= 0 &&
           p->crop.x1 <= p->w && p->crop.y1 <= p->h;
}

// Display size derived from image size and pixel aspect ratio.
void mp_image_params_get_dsize(const struct mp_image_params *p,
                               int *d_w, int *d_h)
{
    if (mp_image_crop_valid(p))
    {
        *d_w = mp_rect_w(p->crop);
        *d_h = mp_rect_h(p->crop);
    } else {
        *d_w = p->w;
        *d_h = p->h;
    }

    if (p->p_w > p->p_h && p->p_h >= 1)
        *d_w = MPCLAMP(*d_w * (int64_t)p->p_w / p->p_h, 1, INT_MAX);
    if (p->p_h > p->p_w && p->p_w >= 1)
        *d_h = MPCLAMP(*d_h * (int64_t)p->p_h / p->p_w, 1, INT_MAX);
}

void mp_image_params_set_dsize(struct mp_image_params *p, int d_w, int d_h)
{
    AVRational ds = av_div_q((AVRational){d_w, d_h}, (AVRational){p->w, p->h});
    p->p_w = ds.num;
    p->p_h = ds.den;
}

char *mp_image_params_to_str_buf(char *b, size_t bs,
                                 const struct mp_image_params *p)
{
    if (p && p->imgfmt) {
        snprintf(b, bs, "%dx%d", p->w, p->h);
        if (p->p_w != p->p_h || !p->p_w)
            mp_snprintf_cat(b, bs, " [%d:%d]", p->p_w, p->p_h);
        mp_snprintf_cat(b, bs, " %s", mp_imgfmt_to_name(p->imgfmt));
        if (p->hw_subfmt)
            mp_snprintf_cat(b, bs, "[%s]", mp_imgfmt_to_name(p->hw_subfmt));
        mp_snprintf_cat(b, bs, " %s/%s/%s/%s/%s",
                        m_opt_choice_str(pl_csp_names, p->repr.sys),
                        m_opt_choice_str(pl_csp_prim_names, p->color.primaries),
                        m_opt_choice_str(pl_csp_trc_names, p->color.transfer),
                        m_opt_choice_str(pl_csp_levels_names, p->repr.levels),
                        m_opt_choice_str(mp_csp_light_names, p->light));
        mp_snprintf_cat(b, bs, " CL=%s",
                        m_opt_choice_str(pl_chroma_names, p->chroma_location));
        if (mp_image_crop_valid(p)) {
            mp_snprintf_cat(b, bs, " crop=%dx%d+%d+%d", mp_rect_w(p->crop),
                            mp_rect_h(p->crop), p->crop.x0, p->crop.y0);
        }
        if (p->rotate)
            mp_snprintf_cat(b, bs, " rot=%d", p->rotate);
        if (p->stereo3d > 0) {
            mp_snprintf_cat(b, bs, " stereo=%s",
                            MP_STEREO3D_NAME_DEF(p->stereo3d, "?"));
        }
        if (p->repr.alpha) {
            mp_snprintf_cat(b, bs, " A=%s",
                            m_opt_choice_str(pl_alpha_names, p->repr.alpha));
        }
    } else {
        snprintf(b, bs, "???");
    }
    return b;
}

// Return whether the image parameters are valid.
// Some non-essential fields are allowed to be unset (like colorspace flags).
bool mp_image_params_valid(const struct mp_image_params *p)
{
    // av_image_check_size has similar checks and triggers around 16000*16000
    // It's mostly needed to deal with the fact that offsets are sometimes
    // ints. We also should (for now) do the same as FFmpeg, to be sure large
    // images don't crash with libswscale or when wrapping with AVFrame and
    // passing the result to filters.
    if (p->w <= 0 || p->h <= 0 || (p->w + 128LL) * (p->h + 128LL) >= INT_MAX / 8)
        return false;

    if (p->p_w < 0 || p->p_h < 0)
        return false;

    if (p->rotate < 0 || p->rotate >= 360)
        return false;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->imgfmt);
    if (!desc.id)
        return false;

    if (p->hw_subfmt && !(desc.flags & MP_IMGFLAG_HWACCEL))
        return false;

    return true;
}

bool mp_image_params_equal(const struct mp_image_params *p1,
                           const struct mp_image_params *p2)
{
    return p1->imgfmt == p2->imgfmt &&
           p1->hw_subfmt == p2->hw_subfmt &&
           p1->w == p2->w && p1->h == p2->h &&
           p1->p_w == p2->p_w && p1->p_h == p2->p_h &&
           p1->force_window == p2->force_window &&
           pl_color_space_equal(&p1->color, &p2->color) &&
           pl_color_repr_equal(&p1->repr, &p2->repr) &&
           p1->light == p2->light &&
           p1->chroma_location == p2->chroma_location &&
           p1->rotate == p2->rotate &&
           p1->stereo3d == p2->stereo3d &&
           mp_rect_equals(&p1->crop, &p2->crop);
}

bool mp_image_params_static_equal(const struct mp_image_params *p1,
                                  const struct mp_image_params *p2)
{
    // Compare only static video parameters, excluding dynamic metadata.
    struct mp_image_params a = *p1;
    struct mp_image_params b = *p2;
    a.repr.dovi = b.repr.dovi = NULL;
    a.color.hdr = b.color.hdr = (struct pl_hdr_metadata){0};
    return mp_image_params_equal(&a, &b);
}

void mp_image_params_update_dynamic(struct mp_image_params *dst,
                                    const struct mp_image_params *src,
                                    bool has_peak_detect_values)
{
    dst->repr.dovi = src->repr.dovi;
    // Don't overwrite peak-detected HDR metadata if available.
    float max_pq_y = dst->color.hdr.max_pq_y;
    float avg_pq_y = dst->color.hdr.avg_pq_y;
    dst->color.hdr = src->color.hdr;
    if (has_peak_detect_values) {
        dst->color.hdr.max_pq_y = max_pq_y;
        dst->color.hdr.avg_pq_y = avg_pq_y;
    }
}

// Restore color system, transfer, and primaries to their original values
// before dovi mapping.
void mp_image_params_restore_dovi_mapping(struct mp_image_params *params)
{
    if (params->repr.sys != PL_COLOR_SYSTEM_DOLBYVISION)
        return;
    params->color.primaries = params->primaries_orig;
    params->color.transfer = params->transfer_orig;
    params->repr.sys = params->sys_orig;
    if (!pl_color_transfer_is_hdr(params->transfer_orig))
        params->color.hdr = (struct pl_hdr_metadata){0};
    if (params->transfer_orig != PL_COLOR_TRC_PQ)
        params->color.hdr.max_pq_y = params->color.hdr.avg_pq_y = 0;
}

// Set most image parameters, but not image format or size.
// Display size is used to set the PAR.
void mp_image_set_attributes(struct mp_image *image,
                             const struct mp_image_params *params)
{
    struct mp_image_params nparams = *params;
    nparams.imgfmt = image->imgfmt;
    nparams.w = image->w;
    nparams.h = image->h;
    if (nparams.imgfmt != params->imgfmt) {
        nparams.repr = (struct pl_color_repr){0};
        nparams.color = (struct pl_color_space){0};
    }
    mp_image_set_params(image, &nparams);
}

static enum pl_color_levels infer_levels(enum mp_imgfmt imgfmt)
{
    switch (imgfmt2pixfmt(imgfmt)) {
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVJ411P:
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUVJ444P:
    case AV_PIX_FMT_YUVJ440P:
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_YA8:
    case AV_PIX_FMT_GRAY9LE:
    case AV_PIX_FMT_GRAY9BE:
    case AV_PIX_FMT_GRAY10LE:
    case AV_PIX_FMT_GRAY10BE:
    case AV_PIX_FMT_GRAY12LE:
    case AV_PIX_FMT_GRAY12BE:
    case AV_PIX_FMT_GRAY14LE:
    case AV_PIX_FMT_GRAY14BE:
    case AV_PIX_FMT_GRAY16LE:
    case AV_PIX_FMT_GRAY16BE:
    case AV_PIX_FMT_YA16BE:
    case AV_PIX_FMT_YA16LE:
        return PL_COLOR_LEVELS_FULL;
    default:
        return PL_COLOR_LEVELS_LIMITED;
    }
}

// If details like params->colorspace/colorlevels are missing, guess them from
// the other settings. Also, even if they are set, make them consistent with
// the colorspace as implied by the pixel format.
void mp_image_params_guess_csp(struct mp_image_params *params)
{
    enum pl_color_system forced_csp = mp_image_params_get_forced_csp(params);
    if (forced_csp == PL_COLOR_SYSTEM_UNKNOWN) { // YUV/other
        if (params->repr.sys != PL_COLOR_SYSTEM_BT_601 &&
            params->repr.sys != PL_COLOR_SYSTEM_BT_709 &&
            params->repr.sys != PL_COLOR_SYSTEM_BT_2020_NC &&
            params->repr.sys != PL_COLOR_SYSTEM_BT_2020_C &&
            params->repr.sys != PL_COLOR_SYSTEM_BT_2100_PQ &&
            params->repr.sys != PL_COLOR_SYSTEM_BT_2100_HLG &&
            params->repr.sys != PL_COLOR_SYSTEM_DOLBYVISION &&
            params->repr.sys != PL_COLOR_SYSTEM_SMPTE_240M &&
            params->repr.sys != PL_COLOR_SYSTEM_YCGCO)
        {
            // Makes no sense, so guess instead
            // YCGCO should be separate, but libavcodec disagrees
            params->repr.sys = PL_COLOR_SYSTEM_UNKNOWN;
        }
        if (params->repr.sys == PL_COLOR_SYSTEM_UNKNOWN)
            params->repr.sys = mp_csp_guess_colorspace(params->w, params->h);
        if (params->repr.levels == PL_COLOR_LEVELS_UNKNOWN) {
            if (params->color.transfer == PL_COLOR_TRC_V_LOG) {
                params->repr.levels = PL_COLOR_LEVELS_FULL;
            } else {
                params->repr.levels = infer_levels(params->imgfmt);
            }
        }
        if (params->color.primaries == PL_COLOR_PRIM_UNKNOWN) {
            // Guess based on the colormatrix as a first priority
            if (params->repr.sys == PL_COLOR_SYSTEM_BT_2020_NC ||
                params->repr.sys == PL_COLOR_SYSTEM_BT_2020_C) {
                params->color.primaries = PL_COLOR_PRIM_BT_2020;
            } else if (params->repr.sys == PL_COLOR_SYSTEM_BT_709) {
                params->color.primaries = PL_COLOR_PRIM_BT_709;
            } else {
                // Ambiguous colormatrix for BT.601, guess based on res
                params->color.primaries = mp_csp_guess_primaries(params->w, params->h);
            }
        }
        if (params->color.transfer == PL_COLOR_TRC_UNKNOWN)
            params->color.transfer = PL_COLOR_TRC_BT_1886;
    } else if (forced_csp == PL_COLOR_SYSTEM_RGB) {
        params->repr.sys = PL_COLOR_SYSTEM_RGB;
        params->repr.levels = PL_COLOR_LEVELS_FULL;

        // The majority of RGB content is either sRGB or (rarely) some other
        // color space which we don't even handle, like AdobeRGB or
        // ProPhotoRGB. The only reasonable thing we can do is assume it's
        // sRGB and hope for the best, which should usually just work out fine.
        // Note: sRGB primaries = BT.709 primaries
        if (params->color.primaries == PL_COLOR_PRIM_UNKNOWN)
            params->color.primaries = PL_COLOR_PRIM_BT_709;
        if (params->color.transfer == PL_COLOR_TRC_UNKNOWN)
            params->color.transfer = PL_COLOR_TRC_SRGB;
    } else if (forced_csp == PL_COLOR_SYSTEM_XYZ) {
        params->repr.sys = PL_COLOR_SYSTEM_XYZ;
        params->repr.levels = PL_COLOR_LEVELS_FULL;
        // Force gamma to ST428 as this is the only correct for DCDM X'Y'Z'
        params->color.transfer = PL_COLOR_TRC_ST428;
        // Don't care about primaries, they shouldn't be used, or if anything
        // MP_CSP_PRIM_ST428 should be defined.
    } else {
        // We have no clue.
        params->repr.sys = PL_COLOR_SYSTEM_UNKNOWN;
        params->repr.levels = PL_COLOR_LEVELS_UNKNOWN;
        params->color.primaries = PL_COLOR_PRIM_UNKNOWN;
        params->color.transfer = PL_COLOR_TRC_UNKNOWN;
    }

    if (!params->color.hdr.max_luma) {
        if (params->color.transfer == PL_COLOR_TRC_HLG) {
            params->color.hdr.max_luma = 1000; // reference display
        } else {
            // If the signal peak is unknown, we're forced to pick the TRC's
            // nominal range as the signal peak to prevent clipping
            params->color.hdr.max_luma = pl_color_transfer_nominal_peak(params->color.transfer) * MP_REF_WHITE;
        }
    }

    if (!pl_color_space_is_hdr(&params->color)) {
        // Some clips have leftover HDR metadata after conversion to SDR, so to
        // avoid blowing up the tone mapping code, strip/sanitize it
        params->color.hdr = pl_hdr_metadata_empty;
    }

    if (params->chroma_location == PL_CHROMA_UNKNOWN) {
        if (params->repr.levels == PL_COLOR_LEVELS_LIMITED)
            params->chroma_location = PL_CHROMA_LEFT;
        if (params->repr.levels == PL_COLOR_LEVELS_FULL)
            params->chroma_location = PL_CHROMA_CENTER;
    }

    if (params->light == MP_CSP_LIGHT_AUTO) {
        // HLG is always scene-referred (using its own OOTF), everything else
        // we assume is display-referred by default.
        if (params->color.transfer == PL_COLOR_TRC_HLG) {
            params->light = MP_CSP_LIGHT_SCENE_HLG;
        } else {
            params->light = MP_CSP_LIGHT_DISPLAY;
        }
    }
}

// Create a new mp_image reference to av_frame.
struct mp_image *mp_image_from_av_frame(struct AVFrame *src)
{
    struct mp_image *dst = &(struct mp_image){0};
    AVFrameSideData *sd;

    for (int p = 0; p < MP_MAX_PLANES; p++)
        dst->bufs[p] = src->buf[p];

    dst->hwctx = src->hw_frames_ctx;

    mp_image_setfmt(dst, pixfmt2imgfmt(src->format));
    mp_image_set_size(dst, src->width, src->height);

    dst->params.p_w = src->sample_aspect_ratio.num;
    dst->params.p_h = src->sample_aspect_ratio.den;

    for (int i = 0; i < 4; i++) {
        dst->planes[i] = src->data[i];
        dst->stride[i] = src->linesize[i];
    }

    dst->pict_type = src->pict_type;

    dst->params.crop.x0 = src->crop_left;
    dst->params.crop.y0 = src->crop_top;
    dst->params.crop.x1 = src->width - src->crop_right;
    dst->params.crop.y1 = src->height - src->crop_bottom;

    dst->fields = 0;
    if (src->flags & AV_FRAME_FLAG_INTERLACED)
        dst->fields |= MP_IMGFIELD_INTERLACED;
    if (src->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST)
        dst->fields |= MP_IMGFIELD_TOP_FIRST;
    if (src->repeat_pict == 1)
        dst->fields |= MP_IMGFIELD_REPEAT_FIRST;

    dst->params.repr = (struct pl_color_repr){
        .sys = pl_system_from_av(src->colorspace),
        .levels = pl_levels_from_av(src->color_range),
    };

    dst->params.color = (struct pl_color_space){
        .primaries = pl_primaries_from_av(src->color_primaries),
        .transfer = pl_transfer_from_av(src->color_trc),
    };

    dst->params.chroma_location = pl_chroma_from_av(src->chroma_location);

    if (src->opaque_ref) {
        struct mp_image_params *p = (void *)src->opaque_ref->data;
        dst->params.stereo3d = p->stereo3d;
        // Might be incorrect if colorspace changes.
        dst->params.light = p->light;
        dst->params.repr.alpha = p->repr.alpha;
    }

    sd = av_frame_get_side_data(src, AV_FRAME_DATA_DISPLAYMATRIX);
    if (sd) {
        double r = av_display_rotation_get((int32_t *)(sd->data));
        if (!isnan(r))
            dst->params.rotate = (((int)(-r) % 360) + 360) % 360;
    }

    sd = av_frame_get_side_data(src, AV_FRAME_DATA_ICC_PROFILE);
    if (sd)
        dst->icc_profile = sd->buf;

    AVFrameSideData *mdm = av_frame_get_side_data(src, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    AVFrameSideData *clm = av_frame_get_side_data(src, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
    AVFrameSideData *dhp = av_frame_get_side_data(src, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
    pl_map_hdr_metadata(&dst->params.color.hdr, &(struct pl_av_hdr_metadata) {
        .mdm = (void *)(mdm ? mdm->data : NULL),
        .clm = (void *)(clm ? clm->data : NULL),
        .dhp = (void *)(dhp ? dhp->data : NULL),
    });

    sd = av_frame_get_side_data(src, AV_FRAME_DATA_A53_CC);
    if (sd)
        dst->a53_cc = sd->buf;

    dst->params.primaries_orig = dst->params.color.primaries;
    dst->params.transfer_orig = dst->params.color.transfer;
    dst->params.sys_orig = dst->params.repr.sys;
    AVBufferRef *dovi = NULL;
    sd = av_frame_get_side_data(src, AV_FRAME_DATA_DOVI_METADATA);
    if (sd) {
#ifdef PL_HAVE_LAV_DOLBY_VISION
        const AVDOVIMetadata *metadata = (const AVDOVIMetadata *)sd->buf->data;
        const AVDOVIRpuDataHeader *header = av_dovi_get_header(metadata);
        if (header->disable_residual_flag) {
            dst->dovi = dovi = av_buffer_alloc(sizeof(struct pl_dovi_metadata));
            MP_HANDLE_OOM(dovi);
#if PL_API_VER >= 343
            pl_map_avdovi_metadata(&dst->params.color, &dst->params.repr,
                                   (void *)dst->dovi->data, metadata);
#else
            struct pl_frame frame;
            frame.repr = dst->params.repr;
            frame.color = dst->params.color;
            pl_frame_map_avdovi_metadata(&frame, (void *)dst->dovi->data, metadata);
            dst->params.repr = frame.repr;
            dst->params.color = frame.color;
#endif
        }
#endif
    }

    sd = av_frame_get_side_data(src, AV_FRAME_DATA_DOVI_RPU_BUFFER);
    if (sd) {
        pl_hdr_metadata_from_dovi_rpu(&dst->params.color.hdr, sd->buf->data,
                                      sd->buf->size);
    }

    sd = av_frame_get_side_data(src, AV_FRAME_DATA_FILM_GRAIN_PARAMS);
    if (sd)
        dst->film_grain = sd->buf;

    for (int n = 0; n < src->nb_side_data; n++) {
        sd = src->side_data[n];
        struct mp_ff_side_data mpsd = {
            .type = sd->type,
            .buf = sd->buf,
        };
        MP_TARRAY_APPEND(NULL, dst->ff_side_data, dst->num_ff_side_data, mpsd);
    }

    if (dst->hwctx) {
        AVHWFramesContext *fctx = (void *)dst->hwctx->data;
        dst->params.hw_subfmt = pixfmt2imgfmt(fctx->sw_format);
    }

    struct mp_image *res = mp_image_new_ref(dst);

    // Allocated, but non-refcounted data.
    talloc_free(dst->ff_side_data);
    av_buffer_unref(&dovi);

    return res;
}


// Convert the mp_image reference to a AVFrame reference.
struct AVFrame *mp_image_to_av_frame(struct mp_image *src)
{
    struct mp_image *new_ref = mp_image_new_ref(src);
    AVFrame *dst = av_frame_alloc();
    if (!dst || !new_ref) {
        talloc_free(new_ref);
        av_frame_free(&dst);
        return NULL;
    }

    for (int p = 0; p < MP_MAX_PLANES; p++) {
        dst->buf[p] = new_ref->bufs[p];
        new_ref->bufs[p] = NULL;
    }

    dst->hw_frames_ctx = new_ref->hwctx;
    new_ref->hwctx = NULL;

    dst->format = imgfmt2pixfmt(src->imgfmt);
    dst->width = src->w;
    dst->height = src->h;

    dst->crop_left = src->params.crop.x0;
    dst->crop_top = src->params.crop.y0;
    dst->crop_right = dst->width - src->params.crop.x1;
    dst->crop_bottom = dst->height - src->params.crop.y1;

    dst->sample_aspect_ratio.num = src->params.p_w;
    dst->sample_aspect_ratio.den = src->params.p_h;

    for (int i = 0; i < 4; i++) {
        dst->data[i] = src->planes[i];
        dst->linesize[i] = src->stride[i];
    }
    dst->extended_data = dst->data;

    dst->pict_type = src->pict_type;
    if (src->fields & MP_IMGFIELD_INTERLACED)
        dst->flags |= AV_FRAME_FLAG_INTERLACED;
    if (src->fields & MP_IMGFIELD_TOP_FIRST)
        dst->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
    if (src->fields & MP_IMGFIELD_REPEAT_FIRST)
        dst->repeat_pict = 1;

    // Image params without dovi mapped; should be passed as side data instead
    struct mp_image_params params = src->params;
    mp_image_params_restore_dovi_mapping(&params);
    pl_avframe_set_repr(dst, params.repr);

    dst->chroma_location = pl_chroma_to_av(params.chroma_location);

    dst->opaque_ref = av_buffer_alloc(sizeof(struct mp_image_params));
    MP_HANDLE_OOM(dst->opaque_ref);
    *(struct mp_image_params *)dst->opaque_ref->data = params;

    if (src->icc_profile) {
        AVFrameSideData *sd =
            av_frame_new_side_data_from_buf(dst, AV_FRAME_DATA_ICC_PROFILE,
                                            new_ref->icc_profile);
        MP_HANDLE_OOM(sd);
        new_ref->icc_profile = NULL;
    }

    pl_avframe_set_color(dst, params.color);

    {
        AVFrameSideData *sd = av_frame_new_side_data(dst,
                                                     AV_FRAME_DATA_DISPLAYMATRIX,
                                                     sizeof(int32_t) * 9);
        MP_HANDLE_OOM(sd);
        av_display_rotation_set((int32_t *)sd->data, params.rotate);
    }

    // Add back side data, but only for types which are not specially handled
    // above. Keep in mind that the types above will be out of sync anyway.
    for (int n = 0; n < new_ref->num_ff_side_data; n++) {
        struct mp_ff_side_data *mpsd = &new_ref->ff_side_data[n];
        if (!av_frame_get_side_data(dst, mpsd->type)) {
            AVFrameSideData *sd = av_frame_new_side_data_from_buf(dst, mpsd->type,
                                                                  mpsd->buf);
            MP_HANDLE_OOM(sd);
            mpsd->buf = NULL;
        }
    }

    talloc_free(new_ref);

    if (dst->format == AV_PIX_FMT_NONE)
        av_frame_free(&dst);
    return dst;
}

// Same as mp_image_to_av_frame(), but unref img. (It does so even on failure.)
struct AVFrame *mp_image_to_av_frame_and_unref(struct mp_image *img)
{
    AVFrame *frame = mp_image_to_av_frame(img);
    talloc_free(img);
    return frame;
}

void memset_pic(void *dst, int fill, int bytesPerLine, int height, int stride)
{
    if (bytesPerLine == stride && height) {
        memset(dst, fill, stride * (height - 1) + bytesPerLine);
    } else {
        for (int i = 0; i < height; i++) {
            memset(dst, fill, bytesPerLine);
            dst = (uint8_t *)dst + stride;
        }
    }
}

void memset16_pic(void *dst, int fill, int unitsPerLine, int height, int stride)
{
    if (fill == 0) {
        memset_pic(dst, 0, unitsPerLine * 2, height, stride);
    } else {
        for (int i = 0; i < height; i++) {
            uint16_t *line = dst;
            uint16_t *end = line + unitsPerLine;
            while (line < end)
                *line++ = fill;
            dst = (uint8_t *)dst + stride;
        }
    }
}

// Pixel at the given luma position on the given plane. x/y always refer to
// non-subsampled coordinates (even if plane is chroma).
// The coordinates must be aligned to mp_imgfmt_desc.align_x/y (these are byte
// and chroma boundaries).
// You cannot access e.g. individual luma pixels on the luma plane with yuv420p.
void *mp_image_pixel_ptr(struct mp_image *img, int plane, int x, int y)
{
    mp_assert(MP_IS_ALIGNED(x, img->fmt.align_x));
    mp_assert(MP_IS_ALIGNED(y, img->fmt.align_y));
    return mp_image_pixel_ptr_ny(img, plane, x, y);
}

// Like mp_image_pixel_ptr(), but do not require alignment on Y coordinates if
// the plane does not require it. Use with care.
// Useful for addressing luma rows.
void *mp_image_pixel_ptr_ny(struct mp_image *img, int plane, int x, int y)
{
    mp_assert(MP_IS_ALIGNED(x, img->fmt.align_x));
    mp_assert(MP_IS_ALIGNED(y, 1 << img->fmt.ys[plane]));
    return img->planes[plane] +
           img->stride[plane] * (ptrdiff_t)(y >> img->fmt.ys[plane]) +
           (x >> img->fmt.xs[plane]) * (size_t)img->fmt.bpp[plane] / 8;
}

// Return size of pixels [x0, x0+w-1] in bytes. The coordinates refer to non-
// subsampled pixels (basically plane 0), and the size is rounded to chroma
// and byte alignment boundaries for the entire image, even if plane!=0.
// x0!=0 is useful for rounding (e.g. 8 bpp, x0=7, w=7 => 0..15 => 2 bytes).
size_t mp_image_plane_bytes(struct mp_image *img, int plane, int x0, int w)
{
    int x1 = MP_ALIGN_UP(x0 + w, img->fmt.align_x);
    x0 = MP_ALIGN_DOWN(x0, img->fmt.align_x);
    size_t bpp = img->fmt.bpp[plane];
    int xs = img->fmt.xs[plane];
    return (x1 >> xs) * bpp / 8 - (x0 >> xs) * bpp / 8;
}
