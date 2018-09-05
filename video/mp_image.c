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
#include <pthread.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>
#include <libavutil/bswap.h>
#include <libavutil/hwcontext.h>
#include <libavutil/rational.h>
#include <libavcodec/avcodec.h>

#if LIBAVUTIL_VERSION_MICRO >= 100
#include <libavutil/mastering_display_metadata.h>
#endif

#include "mpv_talloc.h"

#include "config.h"
#include "common/av_common.h"
#include "common/common.h"
#include "hwdec.h"
#include "mp_image.h"
#include "sws_utils.h"
#include "fmt-conversion.h"

const struct m_opt_choice_alternatives mp_spherical_names[] = {
    {"auto",        MP_SPHERICAL_AUTO},
    {"none",        MP_SPHERICAL_NONE},
    {"unknown",     MP_SPHERICAL_UNKNOWN},
    {"equirect",    MP_SPHERICAL_EQUIRECTANGULAR},
    {0}
};

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
        out_stride[n] = MP_ALIGN_UP(line_bytes, stride_align);
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
// pointer itself by incrementing the buffer pointer until ther alignment is
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
    assert(!mpi->planes[0]);
    assert(!mpi->bufs[0]);

    int align = SWS_MIN_BYTE_ALIGN;

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
    struct mp_image_params params = mpi->params;
    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(out_fmt);
    params.imgfmt = fmt.id;
    mpi->fmt = fmt;
    mpi->imgfmt = fmt.id;
    mpi->num_planes = fmt.num_planes;
    mpi->params = params;
}

static void mp_image_destructor(void *ptr)
{
    mp_image_t *mpi = ptr;
    for (int p = 0; p < MP_MAX_PLANES; p++)
        av_buffer_unref(&mpi->bufs[p]);
    av_buffer_unref(&mpi->hwctx);
    av_buffer_unref(&mpi->icc_profile);
    av_buffer_unref(&mpi->a53_cc);
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
    assert(w >= 0 && h >= 0);
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
    assert(dst->imgfmt == src->imgfmt && dst->w == src->w && dst->h == src->h);
    assert(dst->bufs[0] && src->bufs[0]);

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

static void ref_buffer(bool *ok, AVBufferRef **dst)
{
    if (*dst) {
        *dst = av_buffer_ref(*dst);
        if (!*dst)
            *ok = false;
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

    bool ok = true;
    for (int p = 0; p < MP_MAX_PLANES; p++)
        ref_buffer(&ok, &new->bufs[p]);

    ref_buffer(&ok, &new->hwctx);
    ref_buffer(&ok, &new->icc_profile);
    ref_buffer(&ok, &new->a53_cc);

    new->ff_side_data = talloc_memdup(NULL, new->ff_side_data,
                        new->num_ff_side_data * sizeof(new->ff_side_data[0]));
    for (int n = 0; n < new->num_ff_side_data; n++)
        ref_buffer(&ok, &new->ff_side_data[n].buf);

    if (ok)
        return new;

    // Do this after _all_ bufs were changed; we don't want it to free bufs
    // from the original image if this fails.
    talloc_free(new);
    return NULL;
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
    assert(mp_image_is_writeable(img));
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

typedef void *(*memcpy_fn)(void *d, const void *s, size_t size);

static void memcpy_pic_cb(void *dst, const void *src, int bytesPerLine, int height,
                          int dstStride, int srcStride, memcpy_fn cpy)
{
    if (bytesPerLine == dstStride && dstStride == srcStride && height) {
        if (srcStride < 0) {
            src = (uint8_t*)src + (height - 1) * srcStride;
            dst = (uint8_t*)dst + (height - 1) * dstStride;
            srcStride = -srcStride;
        }

        cpy(dst, src, srcStride * (height - 1) + bytesPerLine);
    } else {
        for (int i = 0; i < height; i++) {
            cpy(dst, src, bytesPerLine);
            src = (uint8_t*)src + srcStride;
            dst = (uint8_t*)dst + dstStride;
        }
    }
}

static void mp_image_copy_cb(struct mp_image *dst, struct mp_image *src,
                             memcpy_fn cpy)
{
    assert(dst->imgfmt == src->imgfmt);
    assert(dst->w == src->w && dst->h == src->h);
    assert(mp_image_is_writeable(dst));
    for (int n = 0; n < dst->num_planes; n++) {
        int line_bytes = (mp_image_plane_w(dst, n) * dst->fmt.bpp[n] + 7) / 8;
        int plane_h = mp_image_plane_h(dst, n);
        memcpy_pic_cb(dst->planes[n], src->planes[n], line_bytes, plane_h,
                      dst->stride[n], src->stride[n], cpy);
    }
    if (dst->fmt.flags & MP_IMGFLAG_PAL)
        memcpy(dst->planes[1], src->planes[1], AVPALETTE_SIZE);
}

void mp_image_copy(struct mp_image *dst, struct mp_image *src)
{
    mp_image_copy_cb(dst, src, memcpy);
}

static enum mp_csp mp_image_params_get_forced_csp(struct mp_image_params *params)
{
    int imgfmt = params->hw_subfmt ? params->hw_subfmt : params->imgfmt;
    return mp_imgfmt_get_forced_csp(imgfmt);
}

void mp_image_copy_attributes(struct mp_image *dst, struct mp_image *src)
{
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
    dst->params.chroma_location = src->params.chroma_location;
    dst->params.spherical = src->params.spherical;
    dst->nominal_fps = src->nominal_fps;
    // ensure colorspace consistency
    if (mp_image_params_get_forced_csp(&dst->params) !=
        mp_image_params_get_forced_csp(&src->params))
        dst->params.color = (struct mp_colorspace){0};
    if ((dst->fmt.flags & MP_IMGFLAG_PAL) && (src->fmt.flags & MP_IMGFLAG_PAL)) {
        if (dst->planes[1] && src->planes[1]) {
            if (mp_image_make_writeable(dst))
                memcpy(dst->planes[1], src->planes[1], AVPALETTE_SIZE);
        }
    }
    av_buffer_unref(&dst->icc_profile);
    dst->icc_profile = src->icc_profile;
    if (dst->icc_profile) {
        dst->icc_profile = av_buffer_ref(dst->icc_profile);
        if (!dst->icc_profile)
            abort();
    }
}

// Crop the given image to (x0, y0)-(x1, y1) (bottom/right border exclusive)
// x0/y0 must be naturally aligned.
void mp_image_crop(struct mp_image *img, int x0, int y0, int x1, int y1)
{
    assert(x0 >= 0 && y0 >= 0);
    assert(x0 <= x1 && y0 <= y1);
    assert(x1 <= img->w && y1 <= img->h);
    assert(!(x0 & (img->fmt.align_x - 1)));
    assert(!(y0 & (img->fmt.align_y - 1)));

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

// Bottom/right border is allowed not to be aligned, but it might implicitly
// overwrite pixel data until the alignment (align_x/align_y) is reached.
void mp_image_clear(struct mp_image *img, int x0, int y0, int x1, int y1)
{
    assert(x0 >= 0 && y0 >= 0);
    assert(x0 <= x1 && y0 <= y1);
    assert(x1 <= img->w && y1 <= img->h);
    assert(!(x0 & (img->fmt.align_x - 1)));
    assert(!(y0 & (img->fmt.align_y - 1)));

    struct mp_image area = *img;
    mp_image_crop(&area, x0, y0, x1, y1);

    uint32_t plane_clear[MP_MAX_PLANES] = {0};

    if (area.imgfmt == IMGFMT_UYVY) {
        plane_clear[0] = av_le2ne16(0x0080);
    } else if (area.fmt.flags & MP_IMGFLAG_YUV_NV) {
        plane_clear[1] = 0x8080;
    } else if (area.fmt.flags & MP_IMGFLAG_YUV_P) {
        uint16_t chroma_clear = (1 << area.fmt.plane_bits) / 2;
        if (!(area.fmt.flags & MP_IMGFLAG_NE))
            chroma_clear = av_bswap16(chroma_clear);
        if (area.num_planes > 2)
            plane_clear[1] = plane_clear[2] = chroma_clear;
    }

    for (int p = 0; p < area.num_planes; p++) {
        int bpp = area.fmt.bpp[p];
        int bytes = (mp_image_plane_w(&area, p) * bpp + 7) / 8;
        if (bpp <= 8) {
            memset_pic(area.planes[p], plane_clear[p], bytes,
                       mp_image_plane_h(&area, p), area.stride[p]);
        } else {
            memset16_pic(area.planes[p], plane_clear[p], (bytes + 1) / 2,
                         mp_image_plane_h(&area, p), area.stride[p]);
        }
    }
}

void mp_image_vflip(struct mp_image *img)
{
    for (int p = 0; p < img->num_planes; p++) {
        int plane_h = mp_image_plane_h(img, p);
        img->planes[p] = img->planes[p] + img->stride[p] * (plane_h - 1);
        img->stride[p] = -img->stride[p];
    }
}

// Display size derived from image size and pixel aspect ratio.
void mp_image_params_get_dsize(const struct mp_image_params *p,
                               int *d_w, int *d_h)
{
    *d_w = p->w;
    *d_h = p->h;
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
        if (p->hw_flags)
            mp_snprintf_cat(b, bs, "[0x%x]", p->hw_flags);
        mp_snprintf_cat(b, bs, " %s/%s/%s/%s/%s",
                        m_opt_choice_str(mp_csp_names, p->color.space),
                        m_opt_choice_str(mp_csp_prim_names, p->color.primaries),
                        m_opt_choice_str(mp_csp_trc_names, p->color.gamma),
                        m_opt_choice_str(mp_csp_levels_names, p->color.levels),
                        m_opt_choice_str(mp_csp_light_names, p->color.light));
        if (p->color.sig_peak)
            mp_snprintf_cat(b, bs, " SP=%f", p->color.sig_peak);
        mp_snprintf_cat(b, bs, " CL=%s",
                        m_opt_choice_str(mp_chroma_names, p->chroma_location));
        if (p->rotate)
            mp_snprintf_cat(b, bs, " rot=%d", p->rotate);
        if (p->stereo3d > 0) {
            mp_snprintf_cat(b, bs, " stereo=%s",
                            MP_STEREO3D_NAME_DEF(p->stereo3d, "?"));
        }
        if (p->spherical.type != MP_SPHERICAL_NONE) {
            const float *a = p->spherical.ref_angles;
            mp_snprintf_cat(b, bs, " (%s %f/%f/%f)",
                            m_opt_choice_str(mp_spherical_names, p->spherical.type),
                            a[0], a[1], a[2]);
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

static bool mp_spherical_equal(const struct mp_spherical_params *p1,
                               const struct mp_spherical_params *p2)
{
    for (int n = 0; n < 3; n++) {
        if (p1->ref_angles[n] != p2->ref_angles[n])
            return false;
    }
    return p1->type == p2->type;
}

bool mp_image_params_equal(const struct mp_image_params *p1,
                           const struct mp_image_params *p2)
{
    return p1->imgfmt == p2->imgfmt &&
           p1->hw_subfmt == p2->hw_subfmt &&
           p1->hw_flags == p2->hw_flags &&
           p1->w == p2->w && p1->h == p2->h &&
           p1->p_w == p2->p_w && p1->p_h == p2->p_h &&
           mp_colorspace_equal(p1->color, p2->color) &&
           p1->chroma_location == p2->chroma_location &&
           p1->rotate == p2->rotate &&
           p1->stereo3d == p2->stereo3d &&
           mp_spherical_equal(&p1->spherical, &p2->spherical);
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
    if (nparams.imgfmt != params->imgfmt)
        nparams.color = (struct mp_colorspace){0};
    mp_image_set_params(image, &nparams);
}

// If details like params->colorspace/colorlevels are missing, guess them from
// the other settings. Also, even if they are set, make them consistent with
// the colorspace as implied by the pixel format.
void mp_image_params_guess_csp(struct mp_image_params *params)
{
    enum mp_csp forced_csp = mp_image_params_get_forced_csp(params);
    if (forced_csp == MP_CSP_AUTO) { // YUV/other
        if (params->color.space != MP_CSP_BT_601 &&
            params->color.space != MP_CSP_BT_709 &&
            params->color.space != MP_CSP_BT_2020_NC &&
            params->color.space != MP_CSP_BT_2020_C &&
            params->color.space != MP_CSP_SMPTE_240M &&
            params->color.space != MP_CSP_YCGCO)
        {
            // Makes no sense, so guess instead
            // YCGCO should be separate, but libavcodec disagrees
            params->color.space = MP_CSP_AUTO;
        }
        if (params->color.space == MP_CSP_AUTO)
            params->color.space = mp_csp_guess_colorspace(params->w, params->h);
        if (params->color.levels == MP_CSP_LEVELS_AUTO) {
            if (params->color.gamma == MP_CSP_TRC_V_LOG) {
                params->color.levels = MP_CSP_LEVELS_PC;
            } else {
                params->color.levels = MP_CSP_LEVELS_TV;
            }
        }
        if (params->color.primaries == MP_CSP_PRIM_AUTO) {
            // Guess based on the colormatrix as a first priority
            if (params->color.space == MP_CSP_BT_2020_NC ||
                params->color.space == MP_CSP_BT_2020_C) {
                params->color.primaries = MP_CSP_PRIM_BT_2020;
            } else if (params->color.space == MP_CSP_BT_709) {
                params->color.primaries = MP_CSP_PRIM_BT_709;
            } else {
                // Ambiguous colormatrix for BT.601, guess based on res
                params->color.primaries = mp_csp_guess_primaries(params->w, params->h);
            }
        }
        if (params->color.gamma == MP_CSP_TRC_AUTO)
            params->color.gamma = MP_CSP_TRC_BT_1886;
    } else if (forced_csp == MP_CSP_RGB) {
        params->color.space = MP_CSP_RGB;
        params->color.levels = MP_CSP_LEVELS_PC;

        // The majority of RGB content is either sRGB or (rarely) some other
        // color space which we don't even handle, like AdobeRGB or
        // ProPhotoRGB. The only reasonable thing we can do is assume it's
        // sRGB and hope for the best, which should usually just work out fine.
        // Note: sRGB primaries = BT.709 primaries
        if (params->color.primaries == MP_CSP_PRIM_AUTO)
            params->color.primaries = MP_CSP_PRIM_BT_709;
        if (params->color.gamma == MP_CSP_TRC_AUTO)
            params->color.gamma = MP_CSP_TRC_SRGB;
    } else if (forced_csp == MP_CSP_XYZ) {
        params->color.space = MP_CSP_XYZ;
        params->color.levels = MP_CSP_LEVELS_PC;

        // The default XYZ matrix converts it to BT.709 color space
        // since that's the most likely scenario. Proper VOs should ignore
        // this field as well as the matrix and treat XYZ input as absolute,
        // but for VOs which use the matrix (and hence, consult this field)
        // this is the correct parameter. This doubles as a reasonable output
        // gamut for VOs which *do* use the specialized XYZ matrix but don't
        // know any better output gamut other than whatever the source is
        // tagged with.
        if (params->color.primaries == MP_CSP_PRIM_AUTO)
            params->color.primaries = MP_CSP_PRIM_BT_709;
        if (params->color.gamma == MP_CSP_TRC_AUTO)
            params->color.gamma = MP_CSP_TRC_LINEAR;
    } else {
        // We have no clue.
        params->color.space = MP_CSP_AUTO;
        params->color.levels = MP_CSP_LEVELS_AUTO;
        params->color.primaries = MP_CSP_PRIM_AUTO;
        params->color.gamma = MP_CSP_TRC_AUTO;
    }

    if (!params->color.sig_peak) {
        if (params->color.gamma == MP_CSP_TRC_HLG) {
            params->color.sig_peak = 1000 / MP_REF_WHITE; // reference display
        } else {
            // If the signal peak is unknown, we're forced to pick the TRC's
            // nominal range as the signal peak to prevent clipping
            params->color.sig_peak = mp_trc_nom_peak(params->color.gamma);
        }
    }

    if (!mp_trc_is_hdr(params->color.gamma)) {
        // Some clips have leftover HDR metadata after conversion to SDR, so to
        // avoid blowing up the tone mapping code, strip/sanitize it
        params->color.sig_peak = 1.0;
    }

    if (params->chroma_location == MP_CHROMA_AUTO) {
        if (params->color.levels == MP_CSP_LEVELS_TV)
            params->chroma_location = MP_CHROMA_LEFT;
        if (params->color.levels == MP_CSP_LEVELS_PC)
            params->chroma_location = MP_CHROMA_CENTER;
    }

    if (params->color.light == MP_CSP_LIGHT_AUTO) {
        // HLG is always scene-referred (using its own OOTF), everything else
        // we assume is display-refered by default.
        if (params->color.gamma == MP_CSP_TRC_HLG) {
            params->color.light = MP_CSP_LIGHT_SCENE_HLG;
        } else {
            params->color.light = MP_CSP_LIGHT_DISPLAY;
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

    dst->fields = 0;
    if (src->interlaced_frame)
        dst->fields |= MP_IMGFIELD_INTERLACED;
    if (src->top_field_first)
        dst->fields |= MP_IMGFIELD_TOP_FIRST;
    if (src->repeat_pict == 1)
        dst->fields |= MP_IMGFIELD_REPEAT_FIRST;

    dst->params.color = (struct mp_colorspace){
        .space = avcol_spc_to_mp_csp(src->colorspace),
        .levels = avcol_range_to_mp_csp_levels(src->color_range),
        .primaries = avcol_pri_to_mp_csp_prim(src->color_primaries),
        .gamma = avcol_trc_to_mp_csp_trc(src->color_trc),
    };

    dst->params.chroma_location = avchroma_location_to_mp(src->chroma_location);

    if (src->opaque_ref) {
        struct mp_image_params *p = (void *)src->opaque_ref->data;
        dst->params.rotate = p->rotate;
        dst->params.stereo3d = p->stereo3d;
        dst->params.spherical = p->spherical;
        // Might be incorrect if colorspace changes.
        dst->params.color.light = p->color.light;
    }

#if LIBAVUTIL_VERSION_MICRO >= 100
    sd = av_frame_get_side_data(src, AV_FRAME_DATA_ICC_PROFILE);
    if (sd)
        dst->icc_profile = sd->buf;

    // Get the content light metadata if available
    sd = av_frame_get_side_data(src, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
    if (sd) {
        AVContentLightMetadata *clm = (AVContentLightMetadata *)sd->data;
        dst->params.color.sig_peak = clm->MaxCLL / MP_REF_WHITE;
    }

    // Otherwise, try getting the mastering metadata if available
    sd = av_frame_get_side_data(src, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    if (!dst->params.color.sig_peak && sd) {
        AVMasteringDisplayMetadata *mdm = (AVMasteringDisplayMetadata *)sd->data;
        if (mdm->has_luminance)
            dst->params.color.sig_peak = av_q2d(mdm->max_luminance) / MP_REF_WHITE;
    }

    sd = av_frame_get_side_data(src, AV_FRAME_DATA_A53_CC);
    if (sd)
        dst->a53_cc = sd->buf;

    for (int n = 0; n < src->nb_side_data; n++) {
        sd = src->side_data[n];
        struct mp_ff_side_data mpsd = {
            .type = sd->type,
            .buf = sd->buf,
        };
        MP_TARRAY_APPEND(NULL, dst->ff_side_data, dst->num_ff_side_data, mpsd);
    }
#endif

    if (dst->hwctx) {
        AVHWFramesContext *fctx = (void *)dst->hwctx->data;
        dst->params.hw_subfmt = pixfmt2imgfmt(fctx->sw_format);
        const struct hwcontext_fns *fns =
            hwdec_get_hwcontext_fns(fctx->device_ctx->type);
        if (fns && fns->complete_image_params)
            fns->complete_image_params(dst);
    }

    struct mp_image *res = mp_image_new_ref(dst);

    // Allocated, but non-refcounted data.
    talloc_free(dst->ff_side_data);

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

    dst->sample_aspect_ratio.num = src->params.p_w;
    dst->sample_aspect_ratio.den = src->params.p_h;

    for (int i = 0; i < 4; i++) {
        dst->data[i] = src->planes[i];
        dst->linesize[i] = src->stride[i];
    }
    dst->extended_data = dst->data;

    dst->pict_type = src->pict_type;
    if (src->fields & MP_IMGFIELD_INTERLACED)
        dst->interlaced_frame = 1;
    if (src->fields & MP_IMGFIELD_TOP_FIRST)
        dst->top_field_first = 1;
    if (src->fields & MP_IMGFIELD_REPEAT_FIRST)
        dst->repeat_pict = 1;

    dst->colorspace = mp_csp_to_avcol_spc(src->params.color.space);
    dst->color_range = mp_csp_levels_to_avcol_range(src->params.color.levels);
    dst->color_primaries =
        mp_csp_prim_to_avcol_pri(src->params.color.primaries);
    dst->color_trc = mp_csp_trc_to_avcol_trc(src->params.color.gamma);

    dst->chroma_location = mp_chroma_location_to_av(src->params.chroma_location);

    dst->opaque_ref = av_buffer_alloc(sizeof(struct mp_image_params));
    if (!dst->opaque_ref)
        abort();
    *(struct mp_image_params *)dst->opaque_ref->data = src->params;

#if LIBAVUTIL_VERSION_MICRO >= 100
    if (src->icc_profile) {
        AVFrameSideData *sd =
            av_frame_new_side_data_from_buf(dst, AV_FRAME_DATA_ICC_PROFILE,
                                            new_ref->icc_profile);
        if (!sd)
            abort();
        new_ref->icc_profile = NULL;
    }

    if (src->params.color.sig_peak) {
        AVContentLightMetadata *clm =
            av_content_light_metadata_create_side_data(dst);
        if (!clm)
            abort();
        clm->MaxCLL = src->params.color.sig_peak * MP_REF_WHITE;
    }

    // Add back side data, but only for types which are not specially handled
    // above. Keep in mind that the types above will be out of sync anyway.
    for (int n = 0; n < new_ref->num_ff_side_data; n++) {
        struct mp_ff_side_data *mpsd = &new_ref->ff_side_data[n];
        if (!av_frame_get_side_data(dst, mpsd->type)) {
            AVFrameSideData *sd = av_frame_new_side_data_from_buf(dst, mpsd->type,
                                                                  mpsd->buf);
            if (!sd)
                abort();
            mpsd->buf = NULL;
        }
    }
#endif

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

void memcpy_pic(void *dst, const void *src, int bytesPerLine, int height,
                int dstStride, int srcStride)
{
    memcpy_pic_cb(dst, src, bytesPerLine, height, dstStride, srcStride, memcpy);
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
