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
#include "mp_image.h"
#include "img_format.h"
#include "mp_image_pool.h"

#define VA_VERBOSE(...) mp_msg(MSGT_VO, MSGL_V, "[vaapi] "  __VA_ARGS__)
#define VA_ERROR(...) mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] "  __VA_ARGS__)

bool check_va_status(VAStatus status, const char *msg)
{
    if (status != VA_STATUS_SUCCESS) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] %s: %s\n", msg, vaErrorStr(status));
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

struct va_image_formats {
    VAImageFormat *entries;
    int num;
};

static void va_get_formats(struct mp_vaapi_ctx *ctx)
{
    int num = vaMaxNumImageFormats(ctx->display);
    VAImageFormat entries[num];
    VAStatus status = vaQueryImageFormats(ctx->display, entries, &num);
    if (!check_va_status(status, "vaQueryImageFormats()"))
        return;
    struct va_image_formats *formats = talloc_ptrtype(ctx, formats);
    formats->entries = talloc_array(formats, VAImageFormat, num);
    formats->num = num;
    VA_VERBOSE("%d image formats available:\n", num);
    for (int i = 0; i < num; i++) {
        formats->entries[i] = entries[i];
        VA_VERBOSE("  %s\n", VA_STR_FOURCC(entries[i].fourcc));
    }
    ctx->image_formats = formats;
}

struct mp_vaapi_ctx *va_initialize(VADisplay *display)
{
    int major_version, minor_version;
    int status = vaInitialize(display, &major_version, &minor_version);
    if (!check_va_status(status, "vaInitialize()"))
        return NULL;

    VA_VERBOSE("VA API version %d.%d\n", major_version, minor_version);

    struct mp_vaapi_ctx *res = talloc_ptrtype(NULL, res);
    *res = (struct mp_vaapi_ctx) {
        .display = display,
    };

    va_get_formats(res);
    if (!res->image_formats)
        goto error;
    return res;

error:
    if (res->display)
        vaTerminate(res->display);
    talloc_free(res);
    return NULL;
}

// Undo va_initialize, and close the VADisplay.
void va_destroy(struct mp_vaapi_ctx *ctx)
{
    if (ctx) {
        if (ctx->display)
            vaTerminate(ctx->display);
        talloc_free(ctx);
    }
}

VAImageFormat *va_image_format_from_imgfmt(const struct va_image_formats *formats,
                                           int imgfmt)
{
    const int fourcc = va_fourcc_from_imgfmt(imgfmt);
    if (!formats || !formats->num || !fourcc)
        return NULL;
    for (int i = 0; i < formats->num; i++) {
        if (formats->entries[i].fourcc == fourcc)
            return &formats->entries[i];
    }
    return NULL;
}

static void va_surface_destroy(struct va_surface *surface);

struct va_surface_pool {
    VADisplay display;
    int rt_format;
    int num_surfaces, lru_counter;
    struct va_surface **surfaces;
};

typedef struct va_surface_priv {
    VADisplay display;
    VAImage image;       // used for software decoding case
    bool is_derived;     // is image derived by vaDeriveImage()?
    bool is_used;        // referenced
    bool is_dead;        // used, but deallocate VA objects as soon as possible
    int  order;          // for LRU allocation
} va_surface_priv_t;

struct va_surface_pool *va_surface_pool_alloc(VADisplay display, int rt_format)
{
    struct va_surface_pool *pool = talloc_ptrtype(NULL, pool);
    *pool = (struct va_surface_pool) {
        .display = display,
        .rt_format = rt_format
    };
    return pool;
}


void va_surface_pool_release(struct va_surface_pool *pool)
{
    if (!pool)
        return;
    va_surface_pool_clear(pool);
    talloc_free(pool);
}

void va_surface_pool_releasep(struct va_surface_pool **pool) {
    if (!pool)
        return;
    va_surface_pool_release(*pool);
    *pool = NULL;
}

void va_surface_pool_clear(struct va_surface_pool *pool)
{
    for (int i=0; i<pool->num_surfaces; ++i) {
        struct va_surface *s = pool->surfaces[i];
        if (s->p->is_used)
            s->p->is_dead = true;
        else
            va_surface_destroy(s);
    }
    talloc_free(pool->surfaces);
    pool->num_surfaces = 0;
}

void va_surface_destroy(struct va_surface *surface)
{
    if (!surface)
        return;
    if (surface->id != VA_INVALID_ID) {
        va_surface_priv_t *p = surface->p;
        assert(!p->is_used);
        if (p->image.image_id != VA_INVALID_ID)
            vaDestroyImage(p->display, p->image.image_id);
        vaDestroySurfaces(p->display, &surface->id, 1);
    }
    talloc_free(surface);
}

void va_surface_release(struct va_surface *surface)
{
    if (!surface)
        return;
    surface->p->is_used = false;
    if (surface->p->is_dead)
        va_surface_destroy(surface);
}

void va_surface_releasep(struct va_surface **surface)
{
    if (!surface)
        return;
    va_surface_release(*surface);
    *surface = NULL;
}

static struct va_surface *va_surface_alloc(struct va_surface_pool *pool,
                                           int w, int h)
{
    VASurfaceID id = VA_INVALID_ID;
    VAStatus status;
    status = vaCreateSurfaces(pool->display, w, h, pool->rt_format, 1, &id);
    if (!check_va_status(status, "vaCreateSurfaces()"))
        return NULL;

    struct va_surface *surface = talloc_ptrtype(NULL, surface);
    if (!surface)
        return NULL;

    MP_TARRAY_APPEND(NULL, pool->surfaces, pool->num_surfaces, surface);
    surface->id = id;
    surface->w = w;
    surface->h = h;
    surface->rt_format = pool->rt_format;
    surface->p = talloc_zero(surface, va_surface_priv_t);
    surface->p->display = pool->display;
    surface->p->image.image_id = surface->p->image.buf = VA_INVALID_ID;
    return surface;
}

struct mp_image *va_surface_pool_get_wrapped(struct va_surface_pool *pool,
                                             const struct va_image_formats *formats,
                                             int imgfmt, int w, int h)
{
    return va_surface_wrap(va_surface_pool_get_by_imgfmt(pool, formats, imgfmt,
                                                         w, h));
}

int va_surface_pool_rt_format(const struct va_surface_pool *pool)
{
    return pool->rt_format;
}

bool va_surface_pool_reserve(struct va_surface_pool *pool, int count,
                             int w, int h)
{
    for (int i=0; i<pool->num_surfaces && count > 0; ++i) {
        const struct va_surface *s = pool->surfaces[i];
        if (s->w == w && s->h == h && !s->p->is_used)
            --count;
    }
    while (count > 0) {
        if (!va_surface_alloc(pool, w, h))
            break;
        --count;
    }
    return !count;
}

struct va_surface *va_surface_pool_get(struct va_surface_pool *pool,
                                       int w, int h)
{
    struct va_surface *best = NULL;
    for (int i=0; i<pool->num_surfaces; ++i) {
        struct va_surface *s = pool->surfaces[i];
        if (!s->p->is_used && s->w == w && s->h == h) {
            if (!best || best->p->order > s->p->order)
                best = s;
        }
    }
    if (!best)
        best = va_surface_alloc(pool, w, h);
    if (best) {
        best->p->is_used = true;
        best->p->order = ++pool->lru_counter;
    }
    return best;
}

static void va_surface_image_destroy(struct va_surface *surface)
{
    if (!surface || surface->p->image.image_id == VA_INVALID_ID)
        return;
    va_surface_priv_t *p = surface->p;
    vaDestroyImage(p->display, p->image.image_id);
    p->image.image_id = VA_INVALID_ID;
    p->is_derived = false;
}

static VAImage *va_surface_image_alloc(struct va_surface *surface,
                                       VAImageFormat *format)
{
    if (!format || !surface)
        return NULL;
    va_surface_priv_t *p = surface->p;
    if (p->image.image_id != VA_INVALID_ID &&
        p->image.format.fourcc == format->fourcc)
        return &p->image;
    va_surface_image_destroy(surface);

    VAStatus status = vaDeriveImage(p->display, surface->id, &p->image);
    if (status == VA_STATUS_SUCCESS) {
        /* vaDeriveImage() is supported, check format */
        if (p->image.format.fourcc == format->fourcc &&
                p->image.width == surface->w && p->image.height == surface->h) {
            p->is_derived = true;
            VA_VERBOSE("Using vaDeriveImage()\n");
        } else {
            vaDestroyImage(p->display, p->image.image_id);
            status = VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }
    if (status != VA_STATUS_SUCCESS) {
        p->image.image_id = VA_INVALID_ID;
        status = vaCreateImage(p->display, format, surface->w, surface->h,
                               &p->image);
        if (!check_va_status(status, "vaCreateImage()")) {
            p->image.image_id = VA_INVALID_ID;
            return NULL;
        }
    }
    return &surface->p->image;
}



struct va_surface *va_surface_pool_get_by_imgfmt(struct va_surface_pool *pool,
                                                 const struct va_image_formats *formats,
                                                 int imgfmt, int w, int h)
{
    if (imgfmt == IMGFMT_VAAPI)
        return va_surface_pool_get(pool, w, h);
    VAImageFormat *format = va_image_format_from_imgfmt(formats, imgfmt);
    if (!format)
        return NULL;
    // WTF: no mapping from VAImageFormat -> VA_RT_FORMAT_
    struct va_surface *surface = va_surface_pool_get(pool, w, h);
    if (!surface)
        return NULL;
    if (va_surface_image_alloc(surface, format))
        return surface;
    va_surface_release(surface);
    return NULL;
}

static void free_va_surface(void *arg)
{
    va_surface_release((struct va_surface*)arg);
}

struct mp_image *va_surface_wrap(struct va_surface *surface)
{
    if (!surface)
        return NULL;

    struct mp_image img = {0};
    mp_image_setfmt(&img, IMGFMT_VAAPI);
    mp_image_set_size(&img, surface->w, surface->h);
    img.planes[0] = (uint8_t*)surface;
    img.planes[3] = (uint8_t*)(uintptr_t)surface->id;
    return mp_image_new_custom_ref(&img, surface, free_va_surface);
}

VASurfaceID va_surface_id_in_mp_image(const struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_VAAPI ?
        (VASurfaceID)(uintptr_t)mpi->planes[3] : VA_INVALID_ID;
}

struct va_surface *va_surface_in_mp_image(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_VAAPI ?
        (struct va_surface*)mpi->planes[0] : NULL;
}

VASurfaceID va_surface_id(const struct va_surface *surface)
{
    return surface ? surface->id : VA_INVALID_ID;
}

bool va_image_map(VADisplay display, VAImage *image, struct mp_image *mpi)
{
    int imgfmt = va_fourcc_to_imgfmt(image->format.fourcc);
    if (imgfmt == IMGFMT_NONE)
        return false;
    void *data = NULL;
    const VAStatus status = vaMapBuffer(display, image->buf, &data);
    if (!check_va_status(status, "vaMapBuffer()"))
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

bool va_image_unmap(VADisplay display, VAImage *image)
{
    const VAStatus status = vaUnmapBuffer(display, image->buf);
    return check_va_status(status, "vaUnmapBuffer()");
}

bool va_surface_upload(struct va_surface *surface, struct mp_image *mpi)
{
    va_surface_priv_t *p = surface->p;
    if (p->image.image_id == VA_INVALID_ID)
        return false;

    if (va_fourcc_to_imgfmt(p->image.format.fourcc) != mpi->imgfmt)
        return false;

    struct mp_image img;
    if (!va_image_map(p->display, &p->image, &img))
        return false;
    mp_image_copy(&img, mpi);
    va_image_unmap(p->display, &p->image);

    if (!p->is_derived) {
        VAStatus status = vaPutImage2(p->display, surface->id,
                                      p->image.image_id,
                                      0, 0, mpi->w, mpi->h,
                                      0, 0, mpi->w, mpi->h);
        if (!check_va_status(status, "vaPutImage()"))
            return false;
    }

    return true;
}

static struct mp_image *try_download(struct va_surface *surface,
                                     VAImageFormat *format,
                                     struct mp_image_pool *pool)
{
    VAStatus status;

    enum mp_imgfmt imgfmt = va_fourcc_to_imgfmt(format->fourcc);
    if (imgfmt == IMGFMT_NONE)
        return NULL;

    if (!va_surface_image_alloc(surface, format))
        return NULL;

    VAImage *image = &surface->p->image;

    if (!surface->p->is_derived) {
        status = vaGetImage(surface->p->display, surface->id, 0, 0,
                            surface->w, surface->h, image->image_id);
        if (status != VA_STATUS_SUCCESS)
            return NULL;
    }

    struct mp_image *dst = NULL;
    struct mp_image tmp;
    if (va_image_map(surface->p->display, image, &tmp)) {
        assert(tmp.imgfmt == imgfmt);
        dst = pool ? mp_image_pool_get(pool, imgfmt, tmp.w, tmp.h)
                    : mp_image_alloc(imgfmt, tmp.w, tmp.h);
        mp_image_copy(dst, &tmp);
        va_image_unmap(surface->p->display, image);
    }
    return dst;
}

// pool is optional (used for allocating returned images).
// Note: unlike va_surface_upload(), this will attempt to (re)create the
//       VAImage stored with the va_surface.
struct mp_image *va_surface_download(struct va_surface *surface,
                                     const struct va_image_formats *formats,
                                     struct mp_image_pool *pool)
{
    VAStatus status = vaSyncSurface(surface->p->display, surface->id);
    if (!check_va_status(status, "vaSyncSurface()"))
        return NULL;

    VAImage *image = &surface->p->image;
    if (image->image_id != VA_INVALID_ID) {
        struct mp_image *mpi = try_download(surface, &image->format, pool);
        if (mpi)
            return mpi;
    }

    // We have no clue which format will work, so try them all.
    for (int i = 0; i < formats->num; i++) {
        VAImageFormat *format = &formats->entries[i];
        struct mp_image *mpi = try_download(surface, format, pool);
        if (mpi)
            return mpi;
    }

    VA_ERROR("failed to get surface data.\n");
    return NULL;
}

