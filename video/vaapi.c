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

#include <assert.h>

#include "config.h"

#include "vaapi.h"
#include "common/common.h"
#include "common/msg.h"
#include "osdep/threads.h"
#include "mp_image.h"
#include "img_format.h"
#include "mp_image_pool.h"
#include "options/m_config.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

struct vaapi_opts {
    char *path;
};

#define OPT_BASE_STRUCT struct vaapi_opts
const struct m_sub_options vaapi_conf = {
    .opts = (const struct m_option[]) {
        OPT_STRING("device", path, 0),
        {0},
    },
    .defaults = &(const struct vaapi_opts) {
        .path = "/dev/dri/renderD128",
    },
    .size = sizeof(struct vaapi_opts),
};

int va_get_colorspace_flag(enum mp_csp csp)
{
    switch (csp) {
    case MP_CSP_BT_601:         return VA_SRC_BT601;
    case MP_CSP_BT_709:         return VA_SRC_BT709;
    case MP_CSP_SMPTE_240M:     return VA_SRC_SMPTE_240;
    }
    return 0;
}

#if VA_CHECK_VERSION(1, 0, 0)
static void va_message_callback(void *context, const char *msg, int mp_level)
{
    struct mp_vaapi_ctx *res = context;
    mp_msg(res->log, mp_level, "libva: %s", msg);
}

static void va_error_callback(void *context, const char *msg)
{
    va_message_callback(context, msg, MSGL_ERR);
}

static void va_info_callback(void *context, const char *msg)
{
    va_message_callback(context, msg, MSGL_V);
}
#else
// Pre-libva2 VA message callbacks are global and do not have a context
// parameter, so it's impossible to know from which VADisplay they
// originate.  Try to route them to existing mpv/libmpv instances within
// this process.
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
#endif

static void free_device_ref(struct AVHWDeviceContext *hwctx)
{
    struct mp_vaapi_ctx *ctx = hwctx->user_opaque;

    if (ctx->display)
        vaTerminate(ctx->display);

    if (ctx->destroy_native_ctx)
        ctx->destroy_native_ctx(ctx->native_ctx);

#if !VA_CHECK_VERSION(1, 0, 0)
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
#endif

    talloc_free(ctx);
}

struct mp_vaapi_ctx *va_initialize(VADisplay *display, struct mp_log *plog,
                                   bool probing)
{
    AVBufferRef *avref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!avref)
        return NULL;

    AVHWDeviceContext *hwctx = (void *)avref->data;
    AVVAAPIDeviceContext *vactx = hwctx->hwctx;

    struct mp_vaapi_ctx *res = talloc_ptrtype(NULL, res);
    *res = (struct mp_vaapi_ctx) {
        .log = mp_log_new(res, plog, "/vaapi"),
        .display = display,
        .av_device_ref = avref,
        .hwctx = {
            .av_device_ref = avref,
        },
    };

    hwctx->free = free_device_ref;
    hwctx->user_opaque = res;

#if VA_CHECK_VERSION(1, 0, 0)
    vaSetErrorCallback(display, va_error_callback, res);
    vaSetInfoCallback(display,  va_info_callback,  res);
#else
    pthread_mutex_lock(&va_log_mutex);
    MP_TARRAY_APPEND(NULL, va_mpv_clients, num_va_mpv_clients, res);
    pthread_mutex_unlock(&va_log_mutex);

    // Check some random symbol added after message callbacks.
    // VA_MICRO_VERSION wasn't bumped at the time.
#ifdef VA_FOURCC_I010
    vaSetErrorCallback(va_error_callback);
    vaSetInfoCallback(va_info_callback);
#endif
#endif

    int major, minor;
    int status = vaInitialize(display, &major, &minor);
    if (status != VA_STATUS_SUCCESS) {
        if (!probing)
            MP_ERR(res, "Failed to initialize VAAPI: %s\n", vaErrorStr(status));
        goto error;
    }
    MP_VERBOSE(res, "Initialized VAAPI: version %d.%d\n", major, minor);

    vactx->display = res->display;

    if (av_hwdevice_ctx_init(res->av_device_ref) < 0)
        goto error;

    return res;

error:
    res->display = NULL; // do not vaTerminate this
    va_destroy(res);
    return NULL;
}

// Undo va_initialize, and close the VADisplay.
void va_destroy(struct mp_vaapi_ctx *ctx)
{
    if (!ctx)
        return;

    AVBufferRef *ref = ctx->av_device_ref;
    av_buffer_unref(&ref); // frees ctx as well
}

VASurfaceID va_surface_id(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_VAAPI ?
        (VASurfaceID)(uintptr_t)mpi->planes[3] : VA_INVALID_ID;
}

static bool is_emulated(struct AVBufferRef *hw_device_ctx)
{
    AVHWDeviceContext *hwctx = (void *)hw_device_ctx->data;
    AVVAAPIDeviceContext *vactx = hwctx->hwctx;

    const char *s = vaQueryVendorString(vactx->display);
    return s && strstr(s, "VDPAU backend");
}


bool va_guess_if_emulated(struct mp_vaapi_ctx *ctx)
{
    return is_emulated(ctx->av_device_ref);
}

struct va_native_display {
    void (*create)(VADisplay **out_display, void **out_native_ctx,
                   const char *path);
    void (*destroy)(void *native_ctx);
};

#if HAVE_VAAPI_X11
#include <X11/Xlib.h>
#include <va/va_x11.h>

static void x11_destroy(void *native_ctx)
{
    XCloseDisplay(native_ctx);
}

static void x11_create(VADisplay **out_display, void **out_native_ctx,
                       const char *path)
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

static void drm_create(VADisplay **out_display, void **out_native_ctx,
                       const char *path)
{
    int drm_fd = open(path, O_RDWR);
    if (drm_fd < 0)
        return;

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

static struct AVBufferRef *va_create_standalone(struct mpv_global *global,
        struct mp_log *log, struct hwcontext_create_dev_params *params)
{
    struct AVBufferRef *ret = NULL;
    struct vaapi_opts *opts = mp_get_config_group(NULL, global, &vaapi_conf);

    for (int n = 0; native_displays[n]; n++) {
        VADisplay *display = NULL;
        void *native_ctx = NULL;
        native_displays[n]->create(&display, &native_ctx, opts->path);
        if (display) {
            struct mp_vaapi_ctx *ctx =
                va_initialize(display, log, params->probing);
            if (!ctx) {
                vaTerminate(display);
                native_displays[n]->destroy(native_ctx);
                goto end;
            }
            ctx->native_ctx = native_ctx;
            ctx->destroy_native_ctx = native_displays[n]->destroy;
            ret = ctx->hwctx.av_device_ref;
            goto end;
        }
    }

end:
    talloc_free(opts);
    return ret;
}

const struct hwcontext_fns hwcontext_fns_vaapi = {
    .av_hwdevice_type = AV_HWDEVICE_TYPE_VAAPI,
    .create_dev = va_create_standalone,
    .is_emulated = is_emulated,
};
