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

#include "config.h"

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mem.h>

#include "mpv_talloc.h"

#include "common/common.h"

#include "fmt-conversion.h"
#include "mp_image.h"
#include "mp_image_pool.h"

static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
#define pool_lock() pthread_mutex_lock(&pool_mutex)
#define pool_unlock() pthread_mutex_unlock(&pool_mutex)

// Thread-safety: the pool itself is not thread-safe, but pool-allocated images
// can be referenced and unreferenced from other threads. (As long as the image
// destructors are thread-safe.)

struct mp_image_pool {
    struct mp_image **images;
    int num_images;

    int fmt, w, h;

    mp_image_allocator allocator;
    void *allocator_ctx;

    bool use_lru;
    unsigned int lru_counter;
};

// Used to gracefully handle the case when the pool is freed while image
// references allocated from the image pool are still held by someone.
struct image_flags {
    // If both of these are false, the image must be freed.
    bool referenced;            // outside mp_image reference exists
    bool pool_alive;            // the mp_image_pool references this
    unsigned int order;         // for LRU allocation (basically a timestamp)
};

static void image_pool_destructor(void *ptr)
{
    struct mp_image_pool *pool = ptr;
    mp_image_pool_clear(pool);
}

// If tparent!=NULL, set it as talloc parent for the pool.
struct mp_image_pool *mp_image_pool_new(void *tparent)
{
    struct mp_image_pool *pool = talloc_ptrtype(tparent, pool);
    talloc_set_destructor(pool, image_pool_destructor);
    *pool = (struct mp_image_pool) {0};
    return pool;
}

void mp_image_pool_clear(struct mp_image_pool *pool)
{
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        struct image_flags *it = img->priv;
        bool referenced;
        pool_lock();
        assert(it->pool_alive);
        it->pool_alive = false;
        referenced = it->referenced;
        pool_unlock();
        if (!referenced)
            talloc_free(img);
    }
    pool->num_images = 0;
}

// This is the only function that is allowed to run in a different thread.
// (Consider passing an image to another thread, which frees it.)
static void unref_image(void *opaque, uint8_t *data)
{
    struct mp_image *img = opaque;
    struct image_flags *it = img->priv;
    bool alive;
    pool_lock();
    assert(it->referenced);
    it->referenced = false;
    alive = it->pool_alive;
    pool_unlock();
    if (!alive)
        talloc_free(img);
}

// Return a new image of given format/size. Unlike mp_image_pool_get(), this
// returns NULL if there is no free image of this format/size.
struct mp_image *mp_image_pool_get_no_alloc(struct mp_image_pool *pool, int fmt,
                                            int w, int h)
{
    struct mp_image *new = NULL;
    pool_lock();
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        struct image_flags *img_it = img->priv;
        assert(img_it->pool_alive);
        if (!img_it->referenced) {
            if (img->imgfmt == fmt && img->w == w && img->h == h) {
                if (pool->use_lru) {
                    struct image_flags *new_it = new ? new->priv : NULL;
                    if (!new_it || new_it->order > img_it->order)
                        new = img;
                } else {
                    new = img;
                    break;
                }
            }
        }
    }
    pool_unlock();
    if (!new)
        return NULL;

    // Reference the new image. Since mp_image_pool is not declared thread-safe,
    // and unreffing images from other threads does not allocate new images,
    // no synchronization is required here.
    for (int p = 0; p < MP_MAX_PLANES; p++)
        assert(!!new->bufs[p] == !p); // only 1 AVBufferRef

    struct mp_image *ref = mp_image_new_dummy_ref(new);

    // This assumes the buffer is at this point exclusively owned by us: we
    // can't track whether the buffer is unique otherwise.
    // (av_buffer_is_writable() checks the refcount of the new buffer only.)
    int flags = av_buffer_is_writable(new->bufs[0]) ? 0 : AV_BUFFER_FLAG_READONLY;
    ref->bufs[0] = av_buffer_create(new->bufs[0]->data, new->bufs[0]->size,
                                    unref_image, new, flags);
    if (!ref->bufs[0]) {
        talloc_free(ref);
        return NULL;
    }

    struct image_flags *it = new->priv;
    assert(!it->referenced && it->pool_alive);
    it->referenced = true;
    it->order = ++pool->lru_counter;
    return ref;
}

void mp_image_pool_add(struct mp_image_pool *pool, struct mp_image *new)
{
    struct image_flags *it = talloc_ptrtype(new, it);
    *it = (struct image_flags) { .pool_alive = true };
    new->priv = it;
    MP_TARRAY_APPEND(pool, pool->images, pool->num_images, new);
}

// Return a new image of given format/size. The only difference to
// mp_image_alloc() is that there is a transparent mechanism to recycle image
// data allocations through this pool.
// If pool==NULL, mp_image_alloc() is called (for convenience).
// The image can be free'd with talloc_free().
// Returns NULL on OOM.
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, int fmt,
                                   int w, int h)
{
    if (!pool)
        return mp_image_alloc(fmt, w, h);
    struct mp_image *new = mp_image_pool_get_no_alloc(pool, fmt, w, h);
    if (!new) {
        if (fmt != pool->fmt || w != pool->w || h != pool->h)
            mp_image_pool_clear(pool);
        pool->fmt = fmt;
        pool->w = w;
        pool->h = h;
        if (pool->allocator) {
            new = pool->allocator(pool->allocator_ctx, fmt, w, h);
        } else {
            new = mp_image_alloc(fmt, w, h);
        }
        if (!new)
            return NULL;
        mp_image_pool_add(pool, new);
        new = mp_image_pool_get_no_alloc(pool, fmt, w, h);
    }
    return new;
}

// Like mp_image_new_copy(), but allocate the image out of the pool.
// If pool==NULL, a plain copy is made (for convenience).
// Returns NULL on OOM.
struct mp_image *mp_image_pool_new_copy(struct mp_image_pool *pool,
                                        struct mp_image *img)
{
    struct mp_image *new = mp_image_pool_get(pool, img->imgfmt, img->w, img->h);
    if (new) {
        mp_image_copy(new, img);
        mp_image_copy_attributes(new, img);
    }
    return new;
}

// Like mp_image_make_writeable(), but if a copy has to be made, allocate it
// out of the pool.
// If pool==NULL, mp_image_make_writeable() is called (for convenience).
// Returns false on failure (see mp_image_make_writeable()).
bool mp_image_pool_make_writeable(struct mp_image_pool *pool,
                                  struct mp_image *img)
{
    if (mp_image_is_writeable(img))
        return true;
    struct mp_image *new = mp_image_pool_new_copy(pool, img);
    if (!new)
        return false;
    mp_image_steal_data(img, new);
    assert(mp_image_is_writeable(img));
    return true;
}

// Call cb(cb_data, fmt, w, h) to allocate an image. Note that the resulting
// image must use only 1 AVBufferRef. The returned image must also be owned
// exclusively by the image pool, otherwise mp_image_is_writeable() will not
// work due to FFmpeg restrictions.
void mp_image_pool_set_allocator(struct mp_image_pool *pool,
                                 mp_image_allocator cb, void  *cb_data)
{
    pool->allocator = cb;
    pool->allocator_ctx = cb_data;
}

// Put into LRU mode. (Likely better for hwaccel surfaces, but worse for memory.)
void mp_image_pool_set_lru(struct mp_image_pool *pool)
{
    pool->use_lru = true;
}

// Return the sw image format mp_image_hw_download() would use. This can be
// different from src->params.hw_subfmt in obscure cases.
int mp_image_hw_download_get_sw_format(struct mp_image *src)
{
    if (!src->hwctx)
        return 0;

    // Try to find the first format which we can apparently use.
    int imgfmt = 0;
    enum AVPixelFormat *fmts;
    if (av_hwframe_transfer_get_formats(src->hwctx,
            AV_HWFRAME_TRANSFER_DIRECTION_FROM, &fmts, 0) < 0)
        return 0;
    for (int n = 0; fmts[n] != AV_PIX_FMT_NONE; n++) {
        imgfmt = pixfmt2imgfmt(fmts[n]);
        if (imgfmt)
            break;
    }
    av_free(fmts);

    return imgfmt;
}

// Copies the contents of the HW surface src to system memory and retuns it.
// If swpool is not NULL, it's used to allocate the target image.
// src must be a hw surface with a AVHWFramesContext attached.
// The returned image is cropped as needed.
// Returns NULL on failure.
struct mp_image *mp_image_hw_download(struct mp_image *src,
                                      struct mp_image_pool *swpool)
{
    int imgfmt = mp_image_hw_download_get_sw_format(src);
    if (!imgfmt)
        return NULL;

    assert(src->hwctx);
    AVHWFramesContext *fctx = (void *)src->hwctx->data;

    struct mp_image *dst =
        mp_image_pool_get(swpool, imgfmt, fctx->width, fctx->height);
    if (!dst)
        return NULL;

    // Target image must be writable, so unref it.
    AVFrame *dstav = mp_image_to_av_frame_and_unref(dst);
    if (!dstav)
        return NULL;

    AVFrame *srcav = mp_image_to_av_frame(src);
    if (!srcav) {
        av_frame_unref(dstav);
        return NULL;
    }

    int res = av_hwframe_transfer_data(dstav, srcav, 0);
    av_frame_free(&srcav);
    dst = mp_image_from_av_frame(dstav);
    av_frame_free(&dstav);
    if (res >= 0 && dst) {
        mp_image_set_size(dst, src->w, src->h);
        mp_image_copy_attributes(dst, src);
    } else {
        mp_image_unrefp(&dst);
    }
    return dst;
}

bool mp_image_hw_upload(struct mp_image *hw_img, struct mp_image *src)
{
    if (hw_img->w != src->w || hw_img->h != src->h)
        return false;

    if (!hw_img->hwctx || src->hwctx)
        return false;

    bool ok = false;
    AVFrame *dstav = NULL;
    AVFrame *srcav = NULL;

    // This means the destination image will not be "writable", which would be
    // a pain if Libav enforced this - fortunately it doesn't care. We can
    // transfer data to it even if there are multiple refs.
    dstav = mp_image_to_av_frame(hw_img);
    if (!dstav)
        goto done;

    srcav = mp_image_to_av_frame(src);
    if (!srcav)
        goto done;

    ok = av_hwframe_transfer_data(dstav, srcav, 0) >= 0;

done:
    av_frame_unref(srcav);
    av_frame_unref(dstav);

    if (ok)
        mp_image_copy_attributes(hw_img, src);
    return ok;
}

bool mp_update_av_hw_frames_pool(struct AVBufferRef **hw_frames_ctx,
                                 struct AVBufferRef *hw_device_ctx,
                                 int imgfmt, int sw_imgfmt, int w, int h)
{
    enum AVPixelFormat format = imgfmt2pixfmt(imgfmt);
    enum AVPixelFormat sw_format = imgfmt2pixfmt(sw_imgfmt);

    if (format == AV_PIX_FMT_NONE || sw_format == AV_PIX_FMT_NONE ||
        !hw_device_ctx || w < 1 || h < 1)
    {
        av_buffer_unref(hw_frames_ctx);
        return false;
    }

    if (*hw_frames_ctx) {
        AVHWFramesContext *hw_frames = (void *)(*hw_frames_ctx)->data;

        if (hw_frames->device_ref->data != hw_device_ctx->data ||
            hw_frames->format != format || hw_frames->sw_format != sw_format ||
            hw_frames->width != w || hw_frames->height != h)
            av_buffer_unref(hw_frames_ctx);
    }

    if (!*hw_frames_ctx) {
        *hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
        if (!*hw_frames_ctx)
            return false;

        AVHWFramesContext *hw_frames = (void *)(*hw_frames_ctx)->data;
        hw_frames->format = format;
        hw_frames->sw_format = sw_format;
        hw_frames->width = w;
        hw_frames->height = h;
        if (av_hwframe_ctx_init(*hw_frames_ctx) < 0) {
            av_buffer_unref(hw_frames_ctx);
            return false;
        }
    }

    return true;
}

struct mp_image *mp_av_pool_image_hw_upload(struct AVBufferRef *hw_frames_ctx,
                                            struct mp_image *src)
{
    AVFrame *av_frame = av_frame_alloc();
    if (!av_frame)
        return NULL;
    if (av_hwframe_get_buffer(hw_frames_ctx, av_frame, 0) < 0) {
        av_frame_free(&av_frame);
        return NULL;
    }
    struct mp_image *dst = mp_image_from_av_frame(av_frame);
    av_frame_free(&av_frame);
    if (!dst)
        return NULL;

    if (dst->w < src->w || dst->h < src->h) {
        talloc_free(dst);
        return NULL;
    }

    mp_image_set_size(dst, src->w, src->h);

    if (!mp_image_hw_upload(dst, src)) {
        talloc_free(dst);
        return NULL;
    }

    mp_image_copy_attributes(dst, src);
    return dst;
}
