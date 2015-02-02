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

#include <assert.h>

#include "vaapi.h"
#include "common/common.h"
#include "common/msg.h"
#include "osdep/threads.h"
#include "mp_image.h"
#include "img_format.h"
#include "mp_image_pool.h"

bool check_va_status(struct mp_log *log, VAStatus status, const char *msg)
{
    if (status != VA_STATUS_SUCCESS) {
        mp_err(log, "%s: %s\n", msg, vaErrorStr(status));
        return false;
    }
    return true;
}

int va_get_colorspace_flag(enum mp_csp csp)
{
#if USE_VAAPI_COLORSPACE
    switch (csp) {
    case MP_CSP_BT_601:         return VA_SRC_BT601;
    case MP_CSP_BT_709:         return VA_SRC_BT709;
    case MP_CSP_SMPTE_240M:     return VA_SRC_SMPTE_240;
    }
#endif
    return 0;
}

struct fmtentry {
    uint32_t va;
    enum mp_imgfmt mp;
};

static const struct fmtentry va_to_imgfmt[] = {
    {VA_FOURCC_YV12, IMGFMT_420P},
    {VA_FOURCC_I420, IMGFMT_420P},
    {VA_FOURCC_IYUV, IMGFMT_420P},
    {VA_FOURCC_NV12, IMGFMT_NV12},
    {VA_FOURCC_UYVY, IMGFMT_UYVY},
    {VA_FOURCC_YUY2, IMGFMT_YUYV},
    // Note: not sure about endian issues (the mp formats are byte-addressed)
    {VA_FOURCC_RGBA, IMGFMT_RGBA},
    {VA_FOURCC_RGBX, IMGFMT_RGBA},
    {VA_FOURCC_BGRA, IMGFMT_BGRA},
    {VA_FOURCC_BGRX, IMGFMT_BGRA},
    {0             , IMGFMT_NONE}
};

enum mp_imgfmt va_fourcc_to_imgfmt(uint32_t fourcc)
{
    for (const struct fmtentry *entry = va_to_imgfmt; entry->va; ++entry) {
        if (entry->va == fourcc)
            return entry->mp;
    }
    return IMGFMT_NONE;
}

uint32_t va_fourcc_from_imgfmt(int imgfmt)
{
    for (const struct fmtentry *entry = va_to_imgfmt; entry->va; ++entry) {
        if (entry->mp == imgfmt)
            return entry->va;
    }
    return 0;
}

static struct mp_image *ctx_download_image(struct mp_hwdec_ctx *ctx,
                                           struct mp_image *mpi,
                                           struct mp_image_pool *swpool)
{
    return va_surface_download(mpi, swpool);
}

struct va_image_formats {
    VAImageFormat *entries;
    int num;
};

static void va_get_formats(struct mp_vaapi_ctx *ctx)
{
    int num = vaMaxNumImageFormats(ctx->display);
    VAImageFormat entries[num];
    VAStatus status = vaQueryImageFormats(ctx->display, entries, &num);
    if (!CHECK_VA_STATUS(ctx, "vaQueryImageFormats()"))
        return;
    struct va_image_formats *formats = talloc_ptrtype(ctx, formats);
    formats->entries = talloc_array(formats, VAImageFormat, num);
    formats->num = num;
    MP_VERBOSE(ctx, "%d image formats available:\n", num);
    for (int i = 0; i < num; i++) {
        formats->entries[i] = entries[i];
        MP_VERBOSE(ctx, "  %s\n", VA_STR_FOURCC(entries[i].fourcc));
    }
    ctx->image_formats = formats;
}

struct mp_vaapi_ctx *va_initialize(VADisplay *display, struct mp_log *plog)
{
    struct mp_vaapi_ctx *res = NULL;
    struct mp_log *log = mp_log_new(NULL, plog, "/vaapi");
    int major_version, minor_version;
    int status = vaInitialize(display, &major_version, &minor_version);
    if (!check_va_status(log, status, "vaInitialize()"))
        goto error;

    mp_verbose(log, "VA API version %d.%d\n", major_version, minor_version);

    res = talloc_ptrtype(NULL, res);
    *res = (struct mp_vaapi_ctx) {
        .log = talloc_steal(res, log),
        .display = display,
        .hwctx = {
            .type = HWDEC_VAAPI,
            .priv = res,
            .vaapi_ctx = res,
            .download_image = ctx_download_image,
        },
    };
    mpthread_mutex_init_recursive(&res->lock);

    va_get_formats(res);
    if (!res->image_formats)
        goto error;
    return res;

error:
    if (res && res->display)
        vaTerminate(res->display);
    talloc_free(log);
    talloc_free(res);
    return NULL;
}

// Undo va_initialize, and close the VADisplay.
void va_destroy(struct mp_vaapi_ctx *ctx)
{
    if (ctx) {
        if (ctx->display)
            vaTerminate(ctx->display);
        pthread_mutex_destroy(&ctx->lock);
        talloc_free(ctx);
    }
}

VAImageFormat *va_image_format_from_imgfmt(struct mp_vaapi_ctx *ctx,  int imgfmt)
{
    struct va_image_formats *formats = ctx->image_formats;
    const int fourcc = va_fourcc_from_imgfmt(imgfmt);
    if (!formats || !formats->num || !fourcc)
        return NULL;
    for (int i = 0; i < formats->num; i++) {
        if (formats->entries[i].fourcc == fourcc)
            return &formats->entries[i];
    }
    return NULL;
}

struct va_surface {
    struct mp_vaapi_ctx *ctx;
    VADisplay display;

    VASurfaceID id;
    int rt_format;

    VAImage image;       // used for software decoding case
    bool is_derived;     // is image derived by vaDeriveImage()?
};

VASurfaceID va_surface_id(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_VAAPI ?
        (VASurfaceID)(uintptr_t)mpi->planes[3] : VA_INVALID_ID;
}

static struct va_surface *va_surface_in_mp_image(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_VAAPI ?
        (struct va_surface*)mpi->planes[0] : NULL;
}

int va_surface_rt_format(struct mp_image *mpi)
{
    struct va_surface *surface = va_surface_in_mp_image(mpi);
    return surface ? surface->rt_format : 0;
}

static void release_va_surface(void *arg)
{
    struct va_surface *surface = arg;

    va_lock(surface->ctx);
    if (surface->id != VA_INVALID_ID) {
        if (surface->image.image_id != VA_INVALID_ID)
            vaDestroyImage(surface->display, surface->image.image_id);
        vaDestroySurfaces(surface->display, &surface->id, 1);
    }
    va_unlock(surface->ctx);
    talloc_free(surface);
}

static struct mp_image *alloc_surface(struct mp_vaapi_ctx *ctx, int rt_format,
                                      int w, int h)
{
    VASurfaceID id = VA_INVALID_ID;
    VAStatus status;
    va_lock(ctx);
    status = vaCreateSurfaces(ctx->display, w, h, rt_format, 1, &id);
    va_unlock(ctx);
    if (!CHECK_VA_STATUS(ctx, "vaCreateSurfaces()"))
        return NULL;

    struct va_surface *surface = talloc_ptrtype(NULL, surface);
    if (!surface)
        return NULL;

    *surface = (struct va_surface){
        .ctx = ctx,
        .id = id,
        .rt_format = rt_format,
        .display = ctx->display,
        .image = { .image_id = VA_INVALID_ID, .buf = VA_INVALID_ID },
    };

    struct mp_image img = {0};
    mp_image_setfmt(&img, IMGFMT_VAAPI);
    mp_image_set_size(&img, w, h);
    img.planes[0] = (uint8_t*)surface;
    img.planes[3] = (uint8_t*)(uintptr_t)surface->id;
    return mp_image_new_custom_ref(&img, surface, release_va_surface);
}

static void va_surface_image_destroy(struct va_surface *surface)
{
    if (!surface || surface->image.image_id == VA_INVALID_ID)
        return;
    vaDestroyImage(surface->display, surface->image.image_id);
    surface->image.image_id = VA_INVALID_ID;
    surface->is_derived = false;
}

static int va_surface_image_alloc(struct mp_image *img, VAImageFormat *format)
{
    struct va_surface *p = va_surface_in_mp_image(img);
    if (!format || !p)
        return -1;
    VADisplay *display = p->display;

    if (p->image.image_id != VA_INVALID_ID &&
        p->image.format.fourcc == format->fourcc)
        return 0;

    int r = 0;
    va_lock(p->ctx);

    va_surface_image_destroy(p);

    VAStatus status = vaDeriveImage(display, p->id, &p->image);
    if (status == VA_STATUS_SUCCESS) {
        /* vaDeriveImage() is supported, check format */
        if (p->image.format.fourcc == format->fourcc &&
            p->image.width == img->w && p->image.height == img->h)
        {
            p->is_derived = true;
            MP_VERBOSE(p->ctx, "Using vaDeriveImage()\n");
        } else {
            vaDestroyImage(p->display, p->image.image_id);
            status = VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }
    if (status != VA_STATUS_SUCCESS) {
        p->image.image_id = VA_INVALID_ID;
        status = vaCreateImage(p->display, format, img->w, img->h, &p->image);
        if (!CHECK_VA_STATUS(p->ctx, "vaCreateImage()")) {
            p->image.image_id = VA_INVALID_ID;
            r = -1;
        }
    }

    va_unlock(p->ctx);
    return r;
}

// img must be a VAAPI surface; make sure its internal VAImage is allocated
// to a format corresponding to imgfmt (or return an error).
int va_surface_alloc_imgfmt(struct mp_image *img, int imgfmt)
{
    struct va_surface *p = va_surface_in_mp_image(img);
    if (!p)
        return -1;
    // Multiple FourCCs can refer to the same imgfmt, so check by doing the
    // surjective conversion first.
    if (p->image.image_id != VA_INVALID_ID &&
        va_fourcc_to_imgfmt(p->image.format.fourcc) == imgfmt)
        return 0;
    VAImageFormat *format = va_image_format_from_imgfmt(p->ctx, imgfmt);
    if (!format)
        return -1;
    if (va_surface_image_alloc(img, format) < 0)
        return -1;
    return 0;
}

bool va_image_map(struct mp_vaapi_ctx *ctx, VAImage *image, struct mp_image *mpi)
{
    int imgfmt = va_fourcc_to_imgfmt(image->format.fourcc);
    if (imgfmt == IMGFMT_NONE)
        return false;
    void *data = NULL;
    va_lock(ctx);
    const VAStatus status = vaMapBuffer(ctx->display, image->buf, &data);
    va_unlock(ctx);
    if (!CHECK_VA_STATUS(ctx, "vaMapBuffer()"))
        return false;

    *mpi = (struct mp_image) {0};
    mp_image_setfmt(mpi, imgfmt);
    mp_image_set_size(mpi, image->width, image->height);

    for (int p = 0; p < image->num_planes; p++) {
        mpi->stride[p] = image->pitches[p];
        mpi->planes[p] = (uint8_t *)data + image->offsets[p];
    }

    if (image->format.fourcc == VA_FOURCC_YV12) {
        MPSWAP(unsigned int, mpi->stride[1], mpi->stride[2]);
        MPSWAP(uint8_t *, mpi->planes[1], mpi->planes[2]);
    }

    return true;
}

bool va_image_unmap(struct mp_vaapi_ctx *ctx, VAImage *image)
{
    va_lock(ctx);
    const VAStatus status = vaUnmapBuffer(ctx->display, image->buf);
    va_unlock(ctx);
    return CHECK_VA_STATUS(ctx, "vaUnmapBuffer()");
}

// va_dst: copy destination, must be IMGFMT_VAAPI
// sw_src: copy source, must be a software p
int va_surface_upload(struct mp_image *va_dst, struct mp_image *sw_src)
{
    struct va_surface *p = va_surface_in_mp_image(va_dst);
    if (!p)
        return -1;

    if (va_surface_alloc_imgfmt(va_dst, sw_src->imgfmt) < 0)
        return -1;

    struct mp_image img;
    if (!va_image_map(p->ctx, &p->image, &img))
        return -1;
    mp_image_copy(&img, sw_src);
    va_image_unmap(p->ctx, &p->image);

    if (!p->is_derived) {
        va_lock(p->ctx);
        VAStatus status = vaPutImage2(p->display, p->id,
                                      p->image.image_id,
                                      0, 0, sw_src->w, sw_src->h,
                                      0, 0, sw_src->w, sw_src->h);
        va_unlock(p->ctx);
        if (!CHECK_VA_STATUS(p->ctx, "vaPutImage()"))
            return -1;
    }

    return 0;
}

static struct mp_image *try_download(struct mp_image *src,
                                     struct mp_image_pool *pool)
{
    VAStatus status;
    struct va_surface *p = va_surface_in_mp_image(src);
    if (!p)
        return NULL;

    VAImage *image = &p->image;

    if (image->image_id == VA_INVALID_ID ||
        !va_fourcc_to_imgfmt(image->format.fourcc))
        return NULL;

    if (!p->is_derived) {
        va_lock(p->ctx);
        status = vaGetImage(p->display, p->id, 0, 0,
                            src->w, src->h, image->image_id);
        va_unlock(p->ctx);
        if (status != VA_STATUS_SUCCESS)
            return NULL;
    }

    struct mp_image *dst = NULL;
    struct mp_image tmp;
    if (va_image_map(p->ctx, image, &tmp)) {
        dst = mp_image_pool_get(pool, tmp.imgfmt, tmp.w, tmp.h);
        if (dst)
            mp_image_copy(dst, &tmp);
        va_image_unmap(p->ctx, image);
    }
    mp_image_copy_attributes(dst, src);
    return dst;
}

// Return a software copy of the IMGFMT_VAAPI src image.
// pool is optional (used for allocating returned images).
struct mp_image *va_surface_download(struct mp_image *src,
                                     struct mp_image_pool *pool)
{
    struct va_surface *p = va_surface_in_mp_image(src);
    if (!p)
        return NULL;
    struct mp_vaapi_ctx *ctx = p->ctx;
    va_lock(ctx);
    VAStatus status = vaSyncSurface(p->display, p->id);
    va_unlock(ctx);
    if (!CHECK_VA_STATUS(ctx, "vaSyncSurface()"))
        return NULL;

    struct mp_image *mpi = try_download(src, pool);
    if (mpi)
        return mpi;

    // We have no clue which format will work, so try them all.
    for (int i = 0; i < ctx->image_formats->num; i++) {
        VAImageFormat *format = &ctx->image_formats->entries[i];
        if (va_surface_image_alloc(src, format) < 0)
            continue;
        mpi = try_download(src, pool);
        if (mpi)
            return mpi;
    }

    MP_ERR(ctx, "failed to get surface data.\n");
    return NULL;
}

struct pool_alloc_ctx {
    struct mp_vaapi_ctx *vaapi;
    int rt_format;
};

static struct mp_image *alloc_pool(void *pctx, int fmt, int w, int h)
{
    struct pool_alloc_ctx *alloc_ctx = pctx;
    if (fmt != IMGFMT_VAAPI)
        return NULL;

    return alloc_surface(alloc_ctx->vaapi, alloc_ctx->rt_format, w, h);
}

// The allocator of the given image pool to allocate VAAPI surfaces, using
// the given rt_format.
void va_pool_set_allocator(struct mp_image_pool *pool, struct mp_vaapi_ctx *ctx,
                           int rt_format)
{
    struct pool_alloc_ctx *alloc_ctx = talloc_ptrtype(pool, alloc_ctx);
    *alloc_ctx = (struct pool_alloc_ctx){
        .vaapi = ctx,
        .rt_format = rt_format,
    };
    mp_image_pool_set_allocator(pool, alloc_pool, alloc_ctx);
    mp_image_pool_set_lru(pool);
}

bool va_guess_if_emulated(struct mp_vaapi_ctx *ctx)
{
    va_lock(ctx);
    const char *s = vaQueryVendorString(ctx->display);
    va_unlock(ctx);
    return s && strstr(s, "VDPAU backend");
}
