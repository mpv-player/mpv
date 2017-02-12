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

#include "config.h"

#include "vaapi.h"
#include "common/common.h"
#include "common/msg.h"
#include "osdep/threads.h"
#include "mp_image.h"
#include "img_format.h"
#include "mp_image_pool.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

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
    switch (csp) {
    case MP_CSP_BT_601:         return VA_SRC_BT601;
    case MP_CSP_BT_709:         return VA_SRC_BT709;
    case MP_CSP_SMPTE_240M:     return VA_SRC_SMPTE_240;
    }
    return 0;
}

struct fmtentry {
    uint32_t va;
    enum mp_imgfmt mp;
};

static const struct fmtentry va_to_imgfmt[] = {
    {VA_FOURCC_NV12, IMGFMT_NV12},
    {VA_FOURCC_YV12, IMGFMT_420P},
    {VA_FOURCC_IYUV, IMGFMT_420P},
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
    struct va_image_formats *formats = talloc_ptrtype(ctx, formats);
    formats->num = vaMaxNumImageFormats(ctx->display);
    formats->entries = talloc_array(formats, VAImageFormat, formats->num);
    VAStatus status = vaQueryImageFormats(ctx->display, formats->entries,
                                          &formats->num);
    if (!CHECK_VA_STATUS(ctx, "vaQueryImageFormats()"))
        return;
    MP_VERBOSE(ctx, "%d image formats available:\n", formats->num);
    for (int i = 0; i < formats->num; i++)
        MP_VERBOSE(ctx, "  %s\n", mp_tag_str(formats->entries[i].fourcc));
    ctx->image_formats = formats;
}

// VA message callbacks are global and do not have a context parameter, so it's
// impossible to know from which VADisplay they originate. Try to route them
// to existing mpv/libmpv instances within this process.
static pthread_mutex_t va_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct mp_vaapi_ctx **va_mpv_clients;
static int num_va_mpv_clients;

static void va_message_callback(const char *msg, int mp_level)
{
    pthread_mutex_lock(&va_log_mutex);

    if (num_va_mpv_clients) {
        struct mp_log *dst = va_mpv_clients[num_va_mpv_clients - 1]->log;
        mp_msg(dst, mp_level, "libva: %s", msg);
    } else {
        // We can't get or call the original libva handler (vaSet... return
        // them, but it might be from some other lib etc.). So just do what
        // libva happened to do at the time of this writing.
        if (mp_level <= MSGL_ERR) {
            fprintf(stderr, "libva error: %s", msg);
        } else {
            fprintf(stderr, "libva info: %s", msg);
        }
    }

    pthread_mutex_unlock(&va_log_mutex);
}

static void va_error_callback(const char *msg)
{
    va_message_callback(msg, MSGL_ERR);
}

static void va_info_callback(const char *msg)
{
    va_message_callback(msg, MSGL_V);
}

static void open_lavu_vaapi_device(struct mp_vaapi_ctx *ctx)
{
    ctx->av_device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!ctx->av_device_ref)
        return;

    AVHWDeviceContext *hwctx = (void *)ctx->av_device_ref->data;
    AVVAAPIDeviceContext *vactx = hwctx->hwctx;

    vactx->display = ctx->display;

    if (av_hwdevice_ctx_init(ctx->av_device_ref) < 0)
        av_buffer_unref(&ctx->av_device_ref);

    ctx->hwctx.av_device_ref = ctx->av_device_ref;
}

struct mp_vaapi_ctx *va_initialize(VADisplay *display, struct mp_log *plog,
                                   bool probing)
{
    struct mp_vaapi_ctx *res = talloc_ptrtype(NULL, res);
    *res = (struct mp_vaapi_ctx) {
        .log = mp_log_new(res, plog, "/vaapi"),
        .display = display,
        .hwctx = {
            .type = HWDEC_VAAPI,
            .ctx = res,
            .download_image = ctx_download_image,
        },
    };

    pthread_mutex_lock(&va_log_mutex);
    MP_TARRAY_APPEND(NULL, va_mpv_clients, num_va_mpv_clients, res);
    pthread_mutex_unlock(&va_log_mutex);

    // Check some random symbol added after message callbacks.
    // VA_MICRO_VERSION wasn't bumped at the time.
#ifdef VA_FOURCC_I010
    vaSetErrorCallback(va_error_callback);
    vaSetInfoCallback(va_info_callback);
#endif

    int major_version, minor_version;
    int status = vaInitialize(display, &major_version, &minor_version);
    if (status != VA_STATUS_SUCCESS && probing)
        goto error;
    if (!check_va_status(res->log, status, "vaInitialize()"))
        goto error;

    MP_VERBOSE(res, "VA API version %d.%d\n", major_version, minor_version);

    va_get_formats(res);
    if (!res->image_formats)
        goto error;

    // For now, some code will still work even if libavutil fails on old crap
    // libva drivers (such as the vdpau wraper). So don't error out on failure.
    open_lavu_vaapi_device(res);

    return res;

error:
    res->display = NULL; // do not vaTerminate this
    va_destroy(res);
    return NULL;
}

// Undo va_initialize, and close the VADisplay.
void va_destroy(struct mp_vaapi_ctx *ctx)
{
    if (ctx) {
        if (ctx->display)
            vaTerminate(ctx->display);

        if (ctx->destroy_native_ctx)
            ctx->destroy_native_ctx(ctx->native_ctx);

        pthread_mutex_lock(&va_log_mutex);
        for (int n = 0; n < num_va_mpv_clients; n++) {
            if (va_mpv_clients[n] == ctx) {
                MP_TARRAY_REMOVE_AT(va_mpv_clients, num_va_mpv_clients, n);
                break;
            }
        }
        if (num_va_mpv_clients == 0)
            TA_FREEP(&va_mpv_clients); // avoid triggering leak detectors
        pthread_mutex_unlock(&va_log_mutex);

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

    // The actually allocated surface size (needed for cropping).
    // mp_images can have a smaller size than this, which means they are
    // cropped down to a smaller size by removing right/bottom pixels.
    int w, h;

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

// Return the real size of the underlying surface. (HW decoding might allocate
// padded surfaces for example.)
void va_surface_get_uncropped_size(struct mp_image *mpi, int *out_w, int *out_h)
{
    if (mpi->hwctx) {
        AVHWFramesContext *fctx = (void *)mpi->hwctx->data;
        *out_w = fctx->width;
        *out_h = fctx->height;
    } else {
        struct va_surface *s = va_surface_in_mp_image(mpi);
        *out_w = s ? s->w : 0;
        *out_h = s ? s->h : 0;
    }
}

static void release_va_surface(void *arg)
{
    struct va_surface *surface = arg;

    if (surface->id != VA_INVALID_ID) {
        if (surface->image.image_id != VA_INVALID_ID)
            vaDestroyImage(surface->display, surface->image.image_id);
        vaDestroySurfaces(surface->display, &surface->id, 1);
    }

    talloc_free(surface);
}

static struct mp_image *alloc_surface(struct mp_vaapi_ctx *ctx, int rt_format,
                                      int w, int h)
{
    VASurfaceID id = VA_INVALID_ID;
    VAStatus status;
    status = vaCreateSurfaces(ctx->display, rt_format, w, h, &id, 1, NULL, 0);
    if (!CHECK_VA_STATUS(ctx, "vaCreateSurfaces()"))
        return NULL;

    struct va_surface *surface = talloc_ptrtype(NULL, surface);
    if (!surface)
        return NULL;

    *surface = (struct va_surface){
        .ctx = ctx,
        .id = id,
        .rt_format = rt_format,
        .w = w,
        .h = h,
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

static int va_surface_image_alloc(struct va_surface *p, VAImageFormat *format)
{
    VADisplay *display = p->display;

    if (p->image.image_id != VA_INVALID_ID &&
        p->image.format.fourcc == format->fourcc)
        return 0;

    int r = 0;

    va_surface_image_destroy(p);

    VAStatus status = vaDeriveImage(display, p->id, &p->image);
    if (status == VA_STATUS_SUCCESS) {
        /* vaDeriveImage() is supported, check format */
        if (p->image.format.fourcc == format->fourcc &&
            p->image.width == p->w && p->image.height == p->h)
        {
            p->is_derived = true;
            MP_TRACE(p->ctx, "Using vaDeriveImage()\n");
        } else {
            vaDestroyImage(p->display, p->image.image_id);
            status = VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }
    if (status != VA_STATUS_SUCCESS) {
        p->image.image_id = VA_INVALID_ID;
        status = vaCreateImage(p->display, format, p->w, p->h, &p->image);
        if (!CHECK_VA_STATUS(p->ctx, "vaCreateImage()")) {
            p->image.image_id = VA_INVALID_ID;
            r = -1;
        }
    }

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
    if (va_surface_image_alloc(p, format) < 0)
        return -1;
    return 0;
}

bool va_image_map(struct mp_vaapi_ctx *ctx, VAImage *image, struct mp_image *mpi)
{
    int imgfmt = va_fourcc_to_imgfmt(image->format.fourcc);
    if (imgfmt == IMGFMT_NONE)
        return false;
    void *data = NULL;
    const VAStatus status = vaMapBuffer(ctx->display, image->buf, &data);
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
        MPSWAP(int, mpi->stride[1], mpi->stride[2]);
        MPSWAP(uint8_t *, mpi->planes[1], mpi->planes[2]);
    }

    return true;
}

bool va_image_unmap(struct mp_vaapi_ctx *ctx, VAImage *image)
{
    const VAStatus status = vaUnmapBuffer(ctx->display, image->buf);
    return CHECK_VA_STATUS(ctx, "vaUnmapBuffer()");
}

// va_dst: copy destination, must be IMGFMT_VAAPI
// sw_src: copy source, must be a software pixel format
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
    assert(sw_src->w <= img.w && sw_src->h <= img.h);
    mp_image_set_size(&img, sw_src->w, sw_src->h); // copy only visible part
    mp_image_copy(&img, sw_src);
    va_image_unmap(p->ctx, &p->image);

    if (!p->is_derived) {
        VAStatus status = vaPutImage(p->display, p->id,
                                     p->image.image_id,
                                     0, 0, sw_src->w, sw_src->h,
                                     0, 0, sw_src->w, sw_src->h);
        if (!CHECK_VA_STATUS(p->ctx, "vaPutImage()"))
            return -1;
    }

    if (p->is_derived)
        va_surface_image_destroy(p);
    return 0;
}

static struct mp_image *try_download(struct va_surface *p, struct mp_image *src,
                                     struct mp_image_pool *pool)
{
    VAStatus status;
    VAImage *image = &p->image;

    if (image->image_id == VA_INVALID_ID ||
        !va_fourcc_to_imgfmt(image->format.fourcc))
        return NULL;

    if (!p->is_derived) {
        status = vaGetImage(p->display, p->id, 0, 0,
                            p->w, p->h, image->image_id);
        if (status != VA_STATUS_SUCCESS)
            return NULL;
    }

    struct mp_image *dst = NULL;
    struct mp_image tmp;
    if (va_image_map(p->ctx, image, &tmp)) {
        assert(src->w <= tmp.w && src->h <= tmp.h);
        mp_image_set_size(&tmp, src->w, src->h); // copy only visible part
        dst = mp_image_pool_get(pool, tmp.imgfmt, tmp.w, tmp.h);
        if (dst) {
            mp_check_gpu_memcpy(p->ctx->log, &p->ctx->gpu_memcpy_message);

            mp_image_copy_gpu(dst, &tmp);
            mp_image_copy_attributes(dst, src);
        }
        va_image_unmap(p->ctx, image);
    }
    if (p->is_derived)
        va_surface_image_destroy(p);
    return dst;
}

// Return a software copy of the IMGFMT_VAAPI src image.
// pool is optional (used for allocating returned images).
struct mp_image *va_surface_download(struct mp_image *src,
                                     struct mp_image_pool *pool)
{
    if (!src || src->imgfmt != IMGFMT_VAAPI)
        return NULL;
    struct va_surface *p = va_surface_in_mp_image(src);
    if (!p) {
        // We might still be able to get to the cheese if this is a surface
        // produced by libavutil's vaapi glue code.
        return mp_image_hw_download(src, pool);
    }
    struct mp_image *mpi = NULL;
    struct mp_vaapi_ctx *ctx = p->ctx;
    VAStatus status = vaSyncSurface(p->display, p->id);
    if (!CHECK_VA_STATUS(ctx, "vaSyncSurface()"))
        goto done;

    mpi = try_download(p, src, pool);
    if (mpi)
        goto done;

    // We have no clue which format will work, so try them all.
    // Make sure to start with the most preferred format (nv12), to avoid
    // slower code paths.
    for (int n = 0; va_to_imgfmt[n].mp; n++) {
        VAImageFormat *format =
            va_image_format_from_imgfmt(ctx, va_to_imgfmt[n].mp);
        if (format) {
            if (va_surface_image_alloc(p, format) < 0)
                continue;
            mpi = try_download(p, src, pool);
            if (mpi)
                goto done;
        }
    }

done:

    if (!mpi)
        MP_ERR(ctx, "failed to get surface data.\n");
    return mpi;
}

// Set the hw_subfmt from the surface's real format. Because of this bug:
//      https://bugs.freedesktop.org/show_bug.cgi?id=79848
// it should be assumed that the real format is only known after an arbitrary
// vaCreateContext() call has been made, or even better, after the surface
// has been rendered to.
// If the hw_subfmt is already set, this is a NOP.
void va_surface_init_subformat(struct mp_image *mpi)
{
    VAStatus status;
    if (mpi->params.hw_subfmt)
        return;
    struct va_surface *p = va_surface_in_mp_image(mpi);
    if (!p)
        return;

    VAImage va_image = { .image_id = VA_INVALID_ID };

    status = vaDeriveImage(p->display, va_surface_id(mpi), &va_image);
    if (status != VA_STATUS_SUCCESS)
        goto err;

    mpi->params.hw_subfmt = va_fourcc_to_imgfmt(va_image.format.fourcc);

    status = vaDestroyImage(p->display, va_image.image_id);
    CHECK_VA_STATUS(p->ctx, "vaDestroyImage()");

err: ;
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
    const char *s = vaQueryVendorString(ctx->display);
    return s && strstr(s, "VDPAU backend");
}

struct va_native_display {
    void (*create)(VADisplay **out_display, void **out_native_ctx);
    void (*destroy)(void *native_ctx);
};

#if HAVE_VAAPI_X11
#include <X11/Xlib.h>
#include <va/va_x11.h>

static void x11_destroy(void *native_ctx)
{
    XCloseDisplay(native_ctx);
}

static void x11_create(VADisplay **out_display, void **out_native_ctx)
{
    void *native_display = XOpenDisplay(NULL);
    if (!native_display)
        return;
    *out_display = vaGetDisplay(native_display);
    if (*out_display) {
        *out_native_ctx = native_display;
    } else {
        XCloseDisplay(native_display);
    }
}

static const struct va_native_display disp_x11 = {
    .create = x11_create,
    .destroy = x11_destroy,
};
#endif

#if HAVE_VAAPI_DRM
#include <unistd.h>
#include <fcntl.h>
#include <va/va_drm.h>

struct va_native_display_drm {
    int drm_fd;
};

static void drm_destroy(void *native_ctx)
{
    struct va_native_display_drm *ctx = native_ctx;
    close(ctx->drm_fd);
    talloc_free(ctx);
}

static void drm_create(VADisplay **out_display, void **out_native_ctx)
{
    static const char *drm_device_paths[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
        NULL
    };

    for (int i = 0; drm_device_paths[i]; i++) {
        int drm_fd = open(drm_device_paths[i], O_RDWR);
        if (drm_fd < 0)
            continue;

        struct va_native_display_drm *ctx = talloc_ptrtype(NULL, ctx);
        ctx->drm_fd = drm_fd;
        *out_display = vaGetDisplayDRM(drm_fd);
        if (out_display) {
            *out_native_ctx = ctx;
            return;
        }

        close(drm_fd);
        talloc_free(ctx);
    }
}

static const struct va_native_display disp_drm = {
    .create = drm_create,
    .destroy = drm_destroy,
};
#endif

static const struct va_native_display *const native_displays[] = {
#if HAVE_VAAPI_DRM
    &disp_drm,
#endif
#if HAVE_VAAPI_X11
    &disp_x11,
#endif
    NULL
};

struct mp_vaapi_ctx *va_create_standalone(struct mp_log *plog, bool probing)
{
    for (int n = 0; native_displays[n]; n++) {
        VADisplay *display = NULL;
        void *native_ctx = NULL;
        native_displays[n]->create(&display, &native_ctx);
        if (display) {
            struct mp_vaapi_ctx *ctx = va_initialize(display, plog, probing);
            if (!ctx) {
                vaTerminate(display);
                native_displays[n]->destroy(native_ctx);
                return NULL;
            }
            ctx->native_ctx = native_ctx;
            ctx->destroy_native_ctx = native_displays[n]->destroy;
            return ctx;
        }
    }
    return NULL;
}
