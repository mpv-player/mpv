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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>
#include <libavutil/bswap.h>
#include <libavcodec/avcodec.h>

#include "talloc.h"

#include "img_format.h"
#include "mp_image.h"
#include "sws_utils.h"
#include "memcpy_pic.h"
#include "fmt-conversion.h"

#include "video/filter/vf.h"

static pthread_mutex_t refcount_mutex = PTHREAD_MUTEX_INITIALIZER;
#define refcount_lock() pthread_mutex_lock(&refcount_mutex)
#define refcount_unlock() pthread_mutex_unlock(&refcount_mutex)

struct m_refcount {
    void *arg;
    // free() is called if refcount reaches 0.
    void (*free)(void *arg);
    // External refcounted object (such as libavcodec DR buffers). This assumes
    // that the actual data is managed by the external object, not by
    // m_refcount. The .ext_* calls use that external object's refcount
    // primitives.
    void (*ext_ref)(void *arg);
    void (*ext_unref)(void *arg);
    bool (*ext_is_unique)(void *arg);
    // Native refcount (there may be additional references if .ext_* are set)
    int refcount;
};

// Only for checking API usage
static void m_refcount_destructor(void *ptr)
{
    struct m_refcount *ref = ptr;
    assert(ref->refcount == 0);
}

// Starts out with refcount==1, caller can set .arg and .free and .ext_*
static struct m_refcount *m_refcount_new(void)
{
    struct m_refcount *ref = talloc_ptrtype(NULL, ref);
    *ref = (struct m_refcount) { .refcount = 1 };
    talloc_set_destructor(ref, m_refcount_destructor);
    return ref;
}

static void m_refcount_ref(struct m_refcount *ref)
{
    refcount_lock();
    ref->refcount++;
    refcount_unlock();

    if (ref->ext_ref)
        ref->ext_ref(ref->arg);
}

static void m_refcount_unref(struct m_refcount *ref)
{
    if (ref->ext_unref)
        ref->ext_unref(ref->arg);

    bool dead;
    refcount_lock();
    assert(ref->refcount > 0);
    ref->refcount--;
    dead = ref->refcount == 0;
    refcount_unlock();

    if (dead) {
        if (ref->free)
            ref->free(ref->arg);
        talloc_free(ref);
    }
}

static bool m_refcount_is_unique(struct m_refcount *ref)
{
    bool nonunique;
    refcount_lock();
    nonunique = ref->refcount > 1;
    refcount_unlock();

    if (nonunique)
        return false;
    if (ref->ext_is_unique)
        return ref->ext_is_unique(ref->arg); // referenced only by us
    return true;
}

static bool mp_image_alloc_planes(struct mp_image *mpi)
{
    assert(!mpi->planes[0]);

    if (!mp_image_params_valid(&mpi->params))
        return false;

    // Note: for non-mod-2 4:2:0 YUV frames, we have to allocate an additional
    //       top/right border. This is needed for correct handling of such
    //       images in filter and VO code (e.g. vo_vdpau or vo_opengl).

    size_t plane_size[MP_MAX_PLANES];
    for (int n = 0; n < MP_MAX_PLANES; n++) {
        int alloc_h = MP_ALIGN_UP(mpi->h, 32) >> mpi->fmt.ys[n];
        int line_bytes = (mpi->plane_w[n] * mpi->fmt.bpp[n] + 7) / 8;
        mpi->stride[n] = FFALIGN(line_bytes, SWS_MIN_BYTE_ALIGN);
        plane_size[n] = mpi->stride[n] * alloc_h;
    }
    if (mpi->fmt.flags & MP_IMGFLAG_PAL)
        plane_size[1] = MP_PALETTE_SIZE;

    size_t sum = 0;
    for (int n = 0; n < MP_MAX_PLANES; n++)
        sum += plane_size[n];

    uint8_t *data = av_malloc(FFMAX(sum, 1));
    if (!data)
        return false;

    for (int n = 0; n < MP_MAX_PLANES; n++) {
        mpi->planes[n] = plane_size[n] ? data : NULL;
        data += plane_size[n];
    }
    return true;
}

void mp_image_setfmt(struct mp_image *mpi, int out_fmt)
{
    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(out_fmt);
    mpi->params.imgfmt = fmt.id;
    mpi->fmt = fmt;
    mpi->flags = fmt.flags;
    mpi->imgfmt = fmt.id;
    mpi->chroma_x_shift = fmt.chroma_xs;
    mpi->chroma_y_shift = fmt.chroma_ys;
    mpi->num_planes = fmt.num_planes;
    mp_image_set_size(mpi, mpi->w, mpi->h);
}

static void mp_image_destructor(void *ptr)
{
    mp_image_t *mpi = ptr;
    m_refcount_unref(mpi->refcount);
}

static int mp_chroma_div_up(int size, int shift)
{
    return (size + (1 << shift) - 1) >> shift;
}

// Caller has to make sure this doesn't exceed the allocated plane data/strides.
void mp_image_set_size(struct mp_image *mpi, int w, int h)
{
    assert(w >= 0 && h >= 0);
    mpi->w = mpi->params.w = mpi->params.d_w = w;
    mpi->h = mpi->params.h = mpi->params.d_h = h;
    for (int n = 0; n < mpi->num_planes; n++) {
        mpi->plane_w[n] = mp_chroma_div_up(mpi->w, mpi->fmt.xs[n]);
        mpi->plane_h[n] = mp_chroma_div_up(mpi->h, mpi->fmt.ys[n]);
    }
    mpi->chroma_width = mpi->plane_w[1];
    mpi->chroma_height = mpi->plane_h[1];
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
    mpi->refcount = m_refcount_new();

    mp_image_set_size(mpi, w, h);
    mp_image_setfmt(mpi, imgfmt);
    if (!mp_image_alloc_planes(mpi)) {
        talloc_free(mpi);
        return NULL;
    }
    mpi->refcount->free = av_free;
    mpi->refcount->arg = mpi->planes[0];
    return mpi;
}

struct mp_image *mp_image_new_copy(struct mp_image *img)
{
    struct mp_image *new = mp_image_alloc(img->imgfmt, img->w, img->h);
    if (!new)
        return NULL;
    mp_image_copy(new, img);
    mp_image_copy_attributes(new, img);

    // Normally these are covered by the reference to the original image data
    // (like the AVFrame in vd_lavc.c), but we can't manage it on our own.
    new->qscale = NULL;
    new->qstride = 0;

    return new;
}

// Make dst take over the image data of src, and free src.
// This is basically a safe version of *dst = *src; free(src);
// Only works with ref-counted images, and can't change image size/format.
void mp_image_steal_data(struct mp_image *dst, struct mp_image *src)
{
    assert(dst->imgfmt == src->imgfmt && dst->w == src->w && dst->h == src->h);
    assert(dst->refcount && src->refcount);

    for (int p = 0; p < MP_MAX_PLANES; p++) {
        dst->planes[p] = src->planes[p];
        dst->stride[p] = src->stride[p];
    }
    mp_image_copy_attributes(dst, src);

    m_refcount_unref(dst->refcount);
    dst->refcount = src->refcount;
    talloc_set_destructor(src, NULL);
    talloc_free(src);
}

// Return a new reference to img. The returned reference is owned by the caller,
// while img is left untouched.
struct mp_image *mp_image_new_ref(struct mp_image *img)
{
    if (!img->refcount)
        return mp_image_new_copy(img);

    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = *img;

    m_refcount_ref(new->refcount);
    return new;
}

// Return a reference counted reference to img. If the reference count reaches
// 0, call free(free_arg). The data passed by img must not be free'd before
// that. The new reference will be writeable.
// On allocation failure, unref the frame and return NULL.
struct mp_image *mp_image_new_custom_ref(struct mp_image *img, void *free_arg,
                                         void (*free)(void *arg))
{
    return mp_image_new_external_ref(img, free_arg, NULL, NULL, NULL, free);
}

// Return a reference counted reference to img. ref/unref/is_unique are used to
// connect to an external refcounting API. It is assumed that the new object
// has an initial reference to that external API. If free is given, that is
// called after the last unref. All function pointers are optional.
// On allocation failure, unref the frame and return NULL.
struct mp_image *mp_image_new_external_ref(struct mp_image *img, void *arg,
                                           void (*ref)(void *arg),
                                           void (*unref)(void *arg),
                                           bool (*is_unique)(void *arg),
                                           void (*free)(void *arg))
{
    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = *img;

    new->refcount = m_refcount_new();
    new->refcount->ext_ref = ref;
    new->refcount->ext_unref = unref;
    new->refcount->ext_is_unique = is_unique;
    new->refcount->free = free;
    new->refcount->arg = arg;
    return new;
}

bool mp_image_is_writeable(struct mp_image *img)
{
    if (!img->refcount)
        return true; // not ref-counted => always considered writeable
    return m_refcount_is_unique(img->refcount);
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

void mp_image_copy(struct mp_image *dst, struct mp_image *src)
{
    assert(dst->imgfmt == src->imgfmt);
    assert(dst->w == src->w && dst->h == src->h);
    assert(mp_image_is_writeable(dst));
    for (int n = 0; n < dst->num_planes; n++) {
        int line_bytes = (dst->plane_w[n] * dst->fmt.bpp[n] + 7) / 8;
        memcpy_pic(dst->planes[n], src->planes[n], line_bytes, dst->plane_h[n],
                   dst->stride[n], src->stride[n]);
    }
    if (dst->fmt.flags & MP_IMGFLAG_PAL)
        memcpy(dst->planes[1], src->planes[1], MP_PALETTE_SIZE);
}

void mp_image_copy_attributes(struct mp_image *dst, struct mp_image *src)
{
    dst->pict_type = src->pict_type;
    dst->fields = src->fields;
    dst->qscale_type = src->qscale_type;
    dst->pts = src->pts;
    if (dst->w == src->w && dst->h == src->h) {
        dst->params.d_w = src->params.d_w;
        dst->params.d_h = src->params.d_h;
    }
    if ((dst->flags & MP_IMGFLAG_YUV) == (src->flags & MP_IMGFLAG_YUV)) {
        dst->params.colorspace = src->params.colorspace;
        dst->params.colorlevels = src->params.colorlevels;
        dst->params.chroma_location = src->params.chroma_location;
    }
    if ((dst->fmt.flags & MP_IMGFLAG_PAL) && (src->fmt.flags & MP_IMGFLAG_PAL)) {
        if (dst->planes[1] && src->planes[1])
            memcpy(dst->planes[1], src->planes[1], MP_PALETTE_SIZE);
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

    if (area.imgfmt == IMGFMT_YUYV) {
        plane_clear[0] = av_le2ne16(0x8000);
    } else if (area.imgfmt == IMGFMT_UYVY) {
        plane_clear[0] = av_le2ne16(0x0080);
    } else if (area.imgfmt == IMGFMT_NV12 || area.imgfmt == IMGFMT_NV21) {
        plane_clear[1] = 0x8080;
    } else if (area.flags & MP_IMGFLAG_YUV_P) {
        uint16_t chroma_clear = (1 << area.fmt.plane_bits) / 2;
        if (!(area.flags & MP_IMGFLAG_NE))
            chroma_clear = av_bswap16(chroma_clear);
        if (area.num_planes > 2)
            plane_clear[1] = plane_clear[2] = chroma_clear;
    }

    for (int p = 0; p < area.num_planes; p++) {
        int bpp = area.fmt.bpp[p];
        int bytes = (area.plane_w[p] * bpp + 7) / 8;
        if (bpp <= 8) {
            memset_pic(area.planes[p], plane_clear[p], bytes,
                       area.plane_h[p], area.stride[p]);
        } else {
            memset16_pic(area.planes[p], plane_clear[p], (bytes + 1) / 2,
                         area.plane_h[p], area.stride[p]);
        }
    }
}

void mp_image_vflip(struct mp_image *img)
{
    for (int p = 0; p < img->num_planes; p++) {
        img->planes[p] = img->planes[p] + img->stride[p] * (img->plane_h[p] - 1);
        img->stride[p] = -img->stride[p];
    }
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
    // Unlike FFmpeg, consider 0x0 valid (might be needed for OSD/screenshots).
    if (p->w < 0 || p->h < 0 || (p->w + 128LL) * (p->h + 128LL) >= INT_MAX / 8)
        return false;

    if (p->d_w < 0 || p->d_h < 0)
        return false;

    if (p->rotate < 0 || p->rotate >= 360)
        return false;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->imgfmt);
    if (!desc.id)
        return false;

    return true;
}

bool mp_image_params_equals(const struct mp_image_params *p1,
                            const struct mp_image_params *p2)
{
    return p1->imgfmt == p2->imgfmt &&
           p1->w == p2->w && p1->h == p2->h &&
           p1->d_w == p2->d_w && p1->d_h == p2->d_h &&
           p1->colorspace == p2->colorspace &&
           p1->colorlevels == p2->colorlevels &&
           p1->outputlevels == p2->outputlevels &&
           p1->chroma_location == p2->chroma_location &&
           p1->rotate == p2->rotate;
}

void mp_image_params_from_image(struct mp_image_params *params,
                                const struct mp_image *image)
{
    *params = image->params;
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
        mp_image_params_guess_csp(&nparams);
    if (nparams.w != params->w || nparams.h != params->h) {
        if (nparams.d_w && nparams.d_h) {
            vf_rescale_dsize(&nparams.d_w, &nparams.d_h,
                             params->w, params->h, nparams.w, nparams.h);
        }
    }
    mp_image_set_params(image, &nparams);
}

// If details like params->colorspace/colorlevels are missing, guess them from
// the other settings. Also, even if they are set, make them consistent with
// the colorspace as implied by the pixel format.
void mp_image_params_guess_csp(struct mp_image_params *params)
{
    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(params->imgfmt);
    if (!fmt.id)
        return;
    if (fmt.flags & MP_IMGFLAG_YUV) {
        if (params->colorspace != MP_CSP_BT_601 &&
            params->colorspace != MP_CSP_BT_709 &&
            params->colorspace != MP_CSP_SMPTE_240M &&
            params->colorspace != MP_CSP_YCGCO)
        {
            // Makes no sense, so guess instead
            // YCGCO should be separate, but libavcodec disagrees
            params->colorspace = MP_CSP_AUTO;
        }
        if (params->colorspace == MP_CSP_AUTO)
            params->colorspace = mp_csp_guess_colorspace(params->w, params->h);
        if (params->colorlevels == MP_CSP_LEVELS_AUTO)
            params->colorlevels = MP_CSP_LEVELS_TV;
    } else if (fmt.flags & MP_IMGFLAG_RGB) {
        params->colorspace = MP_CSP_RGB;
        params->colorlevels = MP_CSP_LEVELS_PC;
    } else if (fmt.flags & MP_IMGFLAG_XYZ) {
        params->colorspace = MP_CSP_XYZ;
        params->colorlevels = MP_CSP_LEVELS_PC;
    } else {
        // We have no clue.
        params->colorspace = MP_CSP_AUTO;
        params->colorlevels = MP_CSP_LEVELS_AUTO;
    }
}

// Copy properties and data of the AVFrame into the mp_image, without taking
// care of memory management issues.
void mp_image_copy_fields_from_av_frame(struct mp_image *dst,
                                        struct AVFrame *src)
{
    mp_image_setfmt(dst, pixfmt2imgfmt(src->format));
    mp_image_set_size(dst, src->width, src->height);

    for (int i = 0; i < 4; i++) {
        dst->planes[i] = src->data[i];
        dst->stride[i] = src->linesize[i];
    }

    dst->pict_type = src->pict_type;

    dst->fields = MP_IMGFIELD_ORDERED;
    if (src->interlaced_frame)
        dst->fields |= MP_IMGFIELD_INTERLACED;
    if (src->top_field_first)
        dst->fields |= MP_IMGFIELD_TOP_FIRST;
    if (src->repeat_pict == 1)
        dst->fields |= MP_IMGFIELD_REPEAT_FIRST;

#if HAVE_AVUTIL_QP_API
    dst->qscale = av_frame_get_qp_table(src, &dst->qstride, &dst->qscale_type);
#endif
}

// Not strictly related, but was added in a similar timeframe.
#define HAVE_AVFRAME_COLORSPACE HAVE_AVCODEC_CHROMA_POS_API

// Copy properties and data of the mp_image into the AVFrame, without taking
// care of memory management issues.
void mp_image_copy_fields_to_av_frame(struct AVFrame *dst,
                                      struct mp_image *src)
{
    dst->format = imgfmt2pixfmt(src->imgfmt);
    dst->width = src->w;
    dst->height = src->h;

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

#if HAVE_AVFRAME_COLORSPACE
    dst->colorspace = mp_csp_to_avcol_spc(src->params.colorspace);
    dst->color_range = mp_csp_levels_to_avcol_range(src->params.colorlevels);
#endif
}

static void frame_free(void *p)
{
    AVFrame *frame = p;
    av_frame_free(&frame);
}

static bool frame_is_unique(void *p)
{
    AVFrame *frame = p;
    return av_frame_is_writable(frame);
}

// Create a new mp_image reference to av_frame.
struct mp_image *mp_image_from_av_frame(struct AVFrame *av_frame)
{
    AVFrame *new_ref = av_frame_clone(av_frame);
    if (!new_ref)
        return NULL;
    struct mp_image t = {0};
    mp_image_copy_fields_from_av_frame(&t, new_ref);
    return mp_image_new_external_ref(&t, new_ref, NULL, NULL, frame_is_unique,
                                     frame_free);
}

static void free_img(void *opaque, uint8_t *data)
{
    struct mp_image *img = opaque;
    talloc_free(img);
}

// Convert the mp_image reference to a AVFrame reference.
// Warning: img is unreferenced (i.e. free'd). This is asymmetric to
//          mp_image_from_av_frame(). It's done this way to allow marking the
//          resulting AVFrame as writeable if img is the only reference (in
//          other words, it's an optimization).
// On failure, img is only unreffed.
struct AVFrame *mp_image_to_av_frame_and_unref(struct mp_image *img)
{
    struct mp_image *new_ref = mp_image_new_ref(img); // ensure it's refcounted
    talloc_free(img);
    if (!new_ref)
        return NULL;
    AVFrame *frame = av_frame_alloc();
    mp_image_copy_fields_to_av_frame(frame, new_ref);
    // Caveat: if img has shared references, and all other references disappear
    //         at a later point, the AVFrame will still be read-only.
    int flags = 0;
    if (!mp_image_is_writeable(new_ref))
        flags |= AV_BUFFER_FLAG_READONLY;
    for (int n = 0; n < new_ref->num_planes; n++) {
        // Make it so that the actual image data is freed only if _all_ buffers
        // are unreferenced.
        struct mp_image *dummy_ref = mp_image_new_ref(new_ref);
        if (!dummy_ref)
            abort(); // out of memory (for the ref, not real image data)
        void *ptr = new_ref->planes[n];
        size_t size = new_ref->stride[n] * new_ref->h;
        frame->buf[n] = av_buffer_create(ptr, size, free_img, dummy_ref, flags);
    }
    talloc_free(new_ref);
    return frame;
}
