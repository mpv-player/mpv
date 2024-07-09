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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm_fourcc.h>

#include "libmpv/render_gl.h"
#include "common/common.h"
#include "osdep/timer.h"
#include "video/out/drm_atomic.h"
#include "video/out/drm_common.h"
#include "video/out/present_sync.h"

#include "egl_helpers.h"
#include "common.h"
#include "context.h"

#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

struct gbm_frame {
    struct gbm_bo *bo;
};

struct gbm {
    struct gbm_surface *surface;
    struct gbm_device *device;
    struct gbm_frame **bo_queue;
    unsigned int num_bos;
};

struct egl {
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
};

struct priv {
    GL gl;

    struct egl egl;
    struct gbm gbm;

    GLsync *vsync_fences;
    unsigned int num_vsync_fences;

    uint32_t gbm_format;
    uint64_t *gbm_modifiers;
    unsigned int num_gbm_modifiers;

    struct mpv_opengl_drm_params_v2 drm_params;
    struct mpv_opengl_drm_draw_surface_size draw_surface_size;
};

// Not general. Limited to only the formats being used in this module
static const char *gbm_format_to_string(uint32_t format)
{
    switch (format) {
    case GBM_FORMAT_XRGB8888:
        return "GBM_FORMAT_XRGB8888";
    case GBM_FORMAT_ARGB8888:
        return "GBM_FORMAT_ARGB8888";
    case GBM_FORMAT_XBGR8888:
        return "GBM_FORMAT_XBGR8888";
    case GBM_FORMAT_ABGR8888:
        return "GBM_FORMAT_ABGR8888";
    case GBM_FORMAT_XRGB2101010:
        return "GBM_FORMAT_XRGB2101010";
    case GBM_FORMAT_ARGB2101010:
        return "GBM_FORMAT_ARGB2101010";
    case GBM_FORMAT_XBGR2101010:
        return "GBM_FORMAT_XBGR2101010";
    case GBM_FORMAT_ABGR2101010:
        return "GBM_FORMAT_ABGR2101010";
    default:
        return "UNKNOWN";
    }
}

// Allow falling back to an ARGB EGLConfig when we have an XRGB framebuffer.
// Also allow falling back to an XRGB EGLConfig for ARGB framebuffers, since
// this seems necessary to work with broken Mali drivers that don't report
// their EGLConfigs as supporting alpha properly.
static uint32_t fallback_format_for(uint32_t format)
{
    switch (format) {
    case GBM_FORMAT_XRGB8888:
        return GBM_FORMAT_ARGB8888;
    case GBM_FORMAT_ARGB8888:
        return GBM_FORMAT_XRGB8888;
    case GBM_FORMAT_XBGR8888:
        return GBM_FORMAT_ABGR8888;
    case GBM_FORMAT_ABGR8888:
        return GBM_FORMAT_XBGR8888;
    case GBM_FORMAT_XRGB2101010:
        return GBM_FORMAT_ARGB2101010;
    case GBM_FORMAT_ARGB2101010:
        return GBM_FORMAT_XRGB2101010;
    case GBM_FORMAT_XBGR2101010:
        return GBM_FORMAT_ABGR2101010;
    case GBM_FORMAT_ABGR2101010:
        return GBM_FORMAT_XBGR2101010;
    default:
        return 0;
    }
}

static int match_config_to_visual(void *user_data, EGLConfig *configs, int num_configs)
{
    struct ra_ctx *ctx = (struct ra_ctx*)user_data;
    struct priv *p = ctx->priv;
    const EGLint visual_id[] = {
        (EGLint)p->gbm_format,
        (EGLint)fallback_format_for(p->gbm_format),
        0
    };

    for (unsigned int i = 0; visual_id[i] != 0; ++i) {
        MP_VERBOSE(ctx, "Attempting to find EGLConfig matching %s\n",
                   gbm_format_to_string(visual_id[i]));
        for (unsigned int j = 0; j < num_configs; ++j) {
            EGLint id;

            if (!eglGetConfigAttrib(p->egl.display, configs[j], EGL_NATIVE_VISUAL_ID, &id))
                continue;

            if (visual_id[i] == id) {
                MP_VERBOSE(ctx, "Found matching EGLConfig for %s\n",
                           gbm_format_to_string(visual_id[i]));
                return j;
            }
        }
        MP_VERBOSE(ctx, "No matching EGLConfig for %s\n", gbm_format_to_string(visual_id[i]));
    }

    MP_ERR(ctx, "Could not find EGLConfig matching the GBM visual (%s).\n",
           gbm_format_to_string(p->gbm_format));
    return -1;
}

static EGLDisplay egl_get_display(struct gbm_device *gbm_device)
{
    EGLDisplay ret;

    ret = mpegl_get_display(EGL_PLATFORM_GBM_MESA, "EGL_MESA_platform_gbm", gbm_device);
    if (ret != EGL_NO_DISPLAY)
        return ret;

    ret = mpegl_get_display(EGL_PLATFORM_GBM_KHR, "EGL_KHR_platform_gbm", gbm_device);
    if (ret != EGL_NO_DISPLAY)
        return ret;

    return eglGetDisplay(gbm_device);
}

static bool init_egl(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    MP_VERBOSE(ctx, "Initializing EGL\n");
    p->egl.display = egl_get_display(p->gbm.device);

    if (p->egl.display == EGL_NO_DISPLAY) {
        MP_ERR(ctx, "Failed to get EGL display.\n");
        return false;
    }
    if (!eglInitialize(p->egl.display, NULL, NULL)) {
        MP_ERR(ctx, "Failed to initialize EGL.\n");
        return false;
    }
    EGLConfig config;
    if (!mpegl_create_context_cb(ctx,
                                 p->egl.display,
                                 (struct mpegl_cb){match_config_to_visual, ctx},
                                 &p->egl.context,
                                 &config))
        return false;

    MP_VERBOSE(ctx, "Initializing EGL surface\n");
    p->egl.surface = mpegl_create_window_surface(
        p->egl.display, config, p->gbm.surface);
    if (p->egl.surface == EGL_NO_SURFACE) {
        p->egl.surface = eglCreateWindowSurface(
            p->egl.display, config, p->gbm.surface, NULL);
    }
    if (p->egl.surface == EGL_NO_SURFACE) {
        MP_ERR(ctx, "Failed to create EGL surface.\n");
        return false;
    }
    return true;
}

static bool init_gbm(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;
    MP_VERBOSE(ctx->vo, "Creating GBM device\n");
    p->gbm.device = gbm_create_device(drm->fd);
    if (!p->gbm.device) {
        MP_ERR(ctx->vo, "Failed to create GBM device.\n");
        return false;
    }

    MP_VERBOSE(ctx->vo, "Initializing GBM surface (%d x %d)\n",
        p->draw_surface_size.width, p->draw_surface_size.height);
    if (p->num_gbm_modifiers == 0) {
        p->gbm.surface = gbm_surface_create(
            p->gbm.device,
            p->draw_surface_size.width,
            p->draw_surface_size.height,
            p->gbm_format,
            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    } else {
        p->gbm.surface = gbm_surface_create_with_modifiers(
            p->gbm.device,
            p->draw_surface_size.width,
            p->draw_surface_size.height,
            p->gbm_format,
            p->gbm_modifiers,
            p->num_gbm_modifiers);
    }
    if (!p->gbm.surface) {
        MP_ERR(ctx->vo, "Failed to create GBM surface.\n");
        return false;
    }
    return true;
}

static void framebuffer_destroy_callback(struct gbm_bo *bo, void *data)
{
    struct framebuffer *fb = data;
    if (fb) {
        drmModeRmFB(fb->fd, fb->id);
    }
}

static void update_framebuffer_from_bo(struct ra_ctx *ctx, struct gbm_bo *bo)
{
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;
    struct framebuffer *fb = gbm_bo_get_user_data(bo);
    if (fb) {
        drm->fb = fb;
        return;
    }

    fb = talloc_zero(ctx, struct framebuffer);
    fb->fd     = drm->fd;
    fb->width  = gbm_bo_get_width(bo);
    fb->height = gbm_bo_get_height(bo);
    uint64_t modifier = gbm_bo_get_modifier(bo);

    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};
    uint32_t flags = 0;

    const int num_planes = gbm_bo_get_plane_count(bo);
    for (int i = 0; i < num_planes; ++i) {
        handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
        strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        offsets[i] = gbm_bo_get_offset(bo, i);
        modifiers[i] = modifier;
    }

    if (modifier && modifier != DRM_FORMAT_MOD_INVALID) {
        MP_VERBOSE(ctx, "GBM surface using modifier 0x%"PRIX64"\n", modifier);
        flags = DRM_MODE_FB_MODIFIERS;
    }

    int ret = drmModeAddFB2WithModifiers(fb->fd, fb->width, fb->height,
                                         p->gbm_format,
                                         handles, strides, offsets, modifiers,
                                         &fb->id, flags);
    if (ret) {
        ret = drmModeAddFB2(fb->fd, fb->width, fb->height, p->gbm_format,
                            handles, strides, offsets, &fb->id, 0);
    }
    if (ret) {
        MP_ERR(ctx->vo, "Failed to create framebuffer: %s\n", mp_strerror(errno));
    }
    gbm_bo_set_user_data(bo, fb, framebuffer_destroy_callback);
    drm->fb = fb;
}

static void queue_flip(struct ra_ctx *ctx, struct gbm_frame *frame)
{
    struct vo_drm_state *drm = ctx->vo->drm;

    update_framebuffer_from_bo(ctx, frame->bo);

    struct drm_atomic_context *atomic_ctx = drm->atomic_context;
    drm_object_set_property(atomic_ctx->request, atomic_ctx->draw_plane, "FB_ID", drm->fb->id);
    drm_object_set_property(atomic_ctx->request, atomic_ctx->draw_plane, "CRTC_ID", atomic_ctx->crtc->id);
    drm_object_set_property(atomic_ctx->request, atomic_ctx->draw_plane, "ZPOS", 1);

    int ret = drmModeAtomicCommit(drm->fd, atomic_ctx->request,
                                  DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, drm);

    if (ret)
        MP_WARN(ctx->vo, "Failed to commit atomic request: %s\n", mp_strerror(ret));
    drm->waiting_for_flip = !ret;

    drmModeAtomicFree(atomic_ctx->request);
    atomic_ctx->request = drmModeAtomicAlloc();
}

static void enqueue_bo(struct ra_ctx *ctx, struct gbm_bo *bo)
{
    struct priv *p = ctx->priv;

    struct gbm_frame *new_frame = talloc(p, struct gbm_frame);
    new_frame->bo = bo;
    MP_TARRAY_APPEND(p, p->gbm.bo_queue, p->gbm.num_bos, new_frame);
}

static void dequeue_bo(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    talloc_free(p->gbm.bo_queue[0]);
    MP_TARRAY_REMOVE_AT(p->gbm.bo_queue, p->gbm.num_bos, 0);
}

static void swapchain_step(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!(p->gbm.num_bos > 0))
        return;

    if (p->gbm.bo_queue[0]->bo)
        gbm_surface_release_buffer(p->gbm.surface, p->gbm.bo_queue[0]->bo);
    dequeue_bo(ctx);
}

static void new_fence(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (p->gl.FenceSync) {
        GLsync fence = p->gl.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (fence)
            MP_TARRAY_APPEND(p, p->vsync_fences, p->num_vsync_fences, fence);
    }
}

static void wait_fence(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    while (p->num_vsync_fences && (p->num_vsync_fences >= p->gbm.num_bos)) {
        p->gl.ClientWaitSync(p->vsync_fences[0], GL_SYNC_FLUSH_COMMANDS_BIT, 1e9);
        p->gl.DeleteSync(p->vsync_fences[0]);
        MP_TARRAY_REMOVE_AT(p->vsync_fences, p->num_vsync_fences, 0);
    }
}

static bool drm_egl_start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct ra_ctx *ctx = sw->ctx;
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;

    if (!drm->atomic_context->request) {
        drm->atomic_context->request = drmModeAtomicAlloc();
        p->drm_params.atomic_request_ptr = &drm->atomic_context->request;
    }

    return ra_gl_ctx_start_frame(sw, out_fbo);
}

static bool drm_egl_submit_frame(struct ra_swapchain *sw, const struct vo_frame *frame)
{
    struct ra_ctx *ctx = sw->ctx;
    struct vo_drm_state *drm = ctx->vo->drm;

    drm->still = frame->still;

    return ra_gl_ctx_submit_frame(sw, frame);
}

static void drm_egl_swap_buffers(struct ra_swapchain *sw)
{
    struct ra_ctx *ctx = sw->ctx;
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;
    const bool drain = drm->paused || drm->still;  // True when we need to drain the swapchain

    if (!drm->active)
        return;

    wait_fence(ctx);

    eglSwapBuffers(p->egl.display, p->egl.surface);

    struct gbm_bo *new_bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    if (!new_bo) {
        MP_ERR(ctx->vo, "Couldn't lock front buffer\n");
        return;
    }
    enqueue_bo(ctx, new_bo);
    new_fence(ctx);

    while (drain || p->gbm.num_bos > ctx->vo->opts->swapchain_depth ||
           !gbm_surface_has_free_buffers(p->gbm.surface)) {
        if (drm->waiting_for_flip) {
            vo_drm_wait_on_flip(drm);
            swapchain_step(ctx);
        }
        if (p->gbm.num_bos <= 1)
            break;
        if (!p->gbm.bo_queue[1] || !p->gbm.bo_queue[1]->bo) {
            MP_ERR(ctx->vo, "Hole in swapchain?\n");
            swapchain_step(ctx);
            continue;
        }
        queue_flip(ctx, p->gbm.bo_queue[1]);
    }
}

static const struct ra_swapchain_fns drm_egl_swapchain = {
    .start_frame   = drm_egl_start_frame,
    .submit_frame  = drm_egl_submit_frame,
    .swap_buffers  = drm_egl_swap_buffers,
};

static void drm_egl_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;
    if (drm) {
        struct drm_atomic_context *atomic_ctx = drm->atomic_context;

        if (drmModeAtomicCommit(drm->fd, atomic_ctx->request, 0, NULL))
            MP_ERR(ctx->vo, "Failed to commit atomic request: %s\n",
                    mp_strerror(errno));

        drmModeAtomicFree(atomic_ctx->request);
    }

    ra_gl_ctx_uninit(ctx);
    vo_drm_uninit(ctx->vo);

    if (p) {
        // According to GBM documentation all BO:s must be released
        // before gbm_surface_destroy can be called on the surface.
        while (p->gbm.num_bos) {
            swapchain_step(ctx);
        }

        eglMakeCurrent(p->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (p->egl.display != EGL_NO_DISPLAY) {
            eglDestroySurface(p->egl.display, p->egl.surface);
            eglDestroyContext(p->egl.display, p->egl.context);
        }
        if (p->gbm.surface)
            gbm_surface_destroy(p->gbm.surface);
        eglTerminate(p->egl.display);
        if (p->gbm.device)
            gbm_device_destroy(p->gbm.device);

        if (p->drm_params.render_fd != -1)
            close(p->drm_params.render_fd);
    }
}

// If the draw plane supports ARGB we want to use that, but if it doesn't we
// fall back on XRGB. If we do not have atomic there is no particular reason to
// be using ARGB (drmprime hwdec will not work without atomic, anyway), so we
// fall back to XRGB (another reason is that we do not have the convenient
// atomic_ctx and its convenient plane fields).
static bool probe_gbm_format(struct ra_ctx *ctx, uint32_t argb_format, uint32_t xrgb_format)
{
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;

    drmModePlane *drmplane = drmModeGetPlane(drm->fd, drm->atomic_context->draw_plane->id);
    bool have_argb = false;
    bool have_xrgb = false;
    bool result = false;
    for (unsigned int i = 0; i < drmplane->count_formats; ++i) {
        if (drmplane->formats[i] == argb_format) {
            have_argb = true;
        } else if (drmplane->formats[i] == xrgb_format) {
            have_xrgb = true;
        }
    }

    if (have_argb) {
        p->gbm_format = argb_format;
        MP_VERBOSE(ctx->vo, "%s supported by draw plane.\n", gbm_format_to_string(argb_format));
        result = true;
    } else if (have_xrgb) {
        p->gbm_format = xrgb_format;
        MP_VERBOSE(ctx->vo, "%s not supported by draw plane: Falling back to %s.\n",
                   gbm_format_to_string(argb_format), gbm_format_to_string(xrgb_format));
        result = true;
    }

    drmModeFreePlane(drmplane);
    return result;
}

static bool probe_gbm_modifiers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo_drm_state *drm = ctx->vo->drm;

    drmModePropertyBlobPtr blob =  drm_object_get_property_blob(drm->atomic_context->draw_plane,
                                                                "IN_FORMATS");
    if (!blob) {
        MP_VERBOSE(ctx->vo, "Failed to find IN_FORMATS property\n");
        return false;
    }

    struct drm_format_modifier_blob *data = blob->data;
    uint32_t *fmts = (uint32_t *)((char *)data + data->formats_offset);
    struct drm_format_modifier *mods =
        (struct drm_format_modifier *)((char *)data + data->modifiers_offset);

    for (unsigned int j = 0; j < data->count_modifiers; ++j) {
        struct drm_format_modifier *mod = &mods[j];
        for (uint64_t k = 0; k < 64; ++k) {
            if (mod->formats & (1ull << k)) {
                uint32_t fmt = fmts[k + mod->offset];
                if (fmt == p->gbm_format) {
                    MP_TARRAY_APPEND(p, p->gbm_modifiers,
                                        p->num_gbm_modifiers, mod->modifier);
                    MP_VERBOSE(ctx->vo, "Supported modifier: 0x%"PRIX64"\n",
                                (uint64_t)mod->modifier);
                    break;
                }
            }
        }
    }
    drmModeFreePropertyBlob(blob);

    if (p->num_gbm_modifiers == 0) {
        MP_VERBOSE(ctx->vo, "No supported DRM modifiers found.\n");
    }
    return true;
}

static void drm_egl_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct vo_drm_state *drm = ctx->vo->drm;
    present_sync_get_info(drm->present, info);
}

static bool drm_egl_init(struct ra_ctx *ctx)
{
    if (!vo_drm_init(ctx->vo))
        goto err;

    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct vo_drm_state *drm = ctx->vo->drm;

    if (ctx->vo->drm->opts->draw_surface_size.wh_valid) {
        p->draw_surface_size.width = ctx->vo->drm->opts->draw_surface_size.w;
        p->draw_surface_size.height = ctx->vo->drm->opts->draw_surface_size.h;
    } else {
        p->draw_surface_size.width = drm->mode.mode.hdisplay;
        p->draw_surface_size.height = drm->mode.mode.vdisplay;
    }

    drm->width = p->draw_surface_size.width;
    drm->height = p->draw_surface_size.height;

    uint32_t argb_format;
    uint32_t xrgb_format;
    switch (ctx->vo->drm->opts->drm_format) {
    case DRM_OPTS_FORMAT_XRGB2101010:
        argb_format = GBM_FORMAT_ARGB2101010;
        xrgb_format = GBM_FORMAT_XRGB2101010;
        break;
    case DRM_OPTS_FORMAT_XBGR2101010:
        argb_format = GBM_FORMAT_ABGR2101010;
        xrgb_format = GBM_FORMAT_XBGR2101010;
        break;
    case DRM_OPTS_FORMAT_XBGR8888:
        argb_format = GBM_FORMAT_ABGR8888;
        xrgb_format = GBM_FORMAT_XBGR8888;
        break;
    default:
        if (drm->opts->drm_format != DRM_OPTS_FORMAT_XRGB8888) {
            MP_VERBOSE(ctx->vo, "Requested format not supported by context, "
                       "falling back to xrgb8888\n");
        }
        argb_format = GBM_FORMAT_ARGB8888;
        xrgb_format = GBM_FORMAT_XRGB8888;
        break;
    }

    if (!probe_gbm_format(ctx, argb_format, xrgb_format)) {
        MP_ERR(ctx->vo, "No suitable format found on draw plane (tried: %s and %s).\n",
               gbm_format_to_string(argb_format), gbm_format_to_string(xrgb_format));
        goto err;
    }

    // It is not fatal if this fails. We'll just try without modifiers.
    probe_gbm_modifiers(ctx);

    if (!init_gbm(ctx)) {
        MP_ERR(ctx->vo, "Failed to setup GBM.\n");
        goto err;
    }

    if (!init_egl(ctx)) {
        MP_ERR(ctx->vo, "Failed to setup EGL.\n");
        goto err;
    }

    if (!eglMakeCurrent(p->egl.display, p->egl.surface, p->egl.surface,
                        p->egl.context)) {
        MP_ERR(ctx->vo, "Failed to make context current.\n");
        goto err;
    }

    mpegl_load_functions(&p->gl, ctx->vo->log);
    // required by gbm_surface_lock_front_buffer
    eglSwapBuffers(p->egl.display, p->egl.surface);

    MP_VERBOSE(ctx, "Preparing framebuffer\n");
    struct gbm_bo *new_bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    if (!new_bo) {
        MP_ERR(ctx, "Failed to lock GBM surface.\n");
        goto err;
    }

    enqueue_bo(ctx, new_bo);
    update_framebuffer_from_bo(ctx, new_bo);
    if (!drm->fb || !drm->fb->id) {
        MP_ERR(ctx, "Failed to create framebuffer.\n");
        goto err;
    }

    if (!vo_drm_acquire_crtc(ctx->vo->drm)) {
        MP_ERR(ctx, "Failed to set CRTC for connector %u: %s\n",
               drm->connector->connector_id, mp_strerror(errno));
        goto err;
    }

    vo_drm_set_monitor_par(ctx->vo);

    p->drm_params.fd = drm->fd;
    p->drm_params.crtc_id = drm->crtc_id;
    p->drm_params.connector_id = drm->connector->connector_id;
    p->drm_params.atomic_request_ptr = &drm->atomic_context->request;
    char *rendernode_path = drmGetRenderDeviceNameFromFd(drm->fd);
    if (rendernode_path) {
        MP_VERBOSE(ctx, "Opening render node \"%s\"\n", rendernode_path);
        p->drm_params.render_fd = open(rendernode_path, O_RDWR | O_CLOEXEC);
        if (p->drm_params.render_fd == -1) {
            MP_WARN(ctx, "Cannot open render node: %s\n", mp_strerror(errno));
        }
        free(rendernode_path);
    } else {
        p->drm_params.render_fd = -1;
        MP_VERBOSE(ctx, "Could not find path to render node.\n");
    }

    struct ra_gl_ctx_params params = {
        .external_swapchain = &drm_egl_swapchain,
        .get_vsync          = &drm_egl_get_vsync,
    };
    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        goto err;

    ra_add_native_resource(ctx->ra, "drm_params_v2", &p->drm_params);
    ra_add_native_resource(ctx->ra, "drm_draw_surface_size", &p->draw_surface_size);

    return true;

err:
    drm_egl_uninit(ctx);
    return false;
}

static bool drm_egl_reconfig(struct ra_ctx *ctx)
{
    struct vo_drm_state *drm = ctx->vo->drm;
    ctx->vo->dwidth  = drm->fb->width;
    ctx->vo->dheight = drm->fb->height;
    ra_gl_ctx_resize(ctx->swapchain, drm->fb->width, drm->fb->height, 0);
    return true;
}

static int drm_egl_control(struct ra_ctx *ctx, int *events, int request,
                           void *arg)
{
    int ret = vo_drm_control(ctx->vo, events, request, arg);
    return ret;
}

static void drm_egl_wait_events(struct ra_ctx *ctx, int64_t until_time_ns)
{
    vo_drm_wait_events(ctx->vo, until_time_ns);
}

static void drm_egl_wakeup(struct ra_ctx *ctx)
{
    vo_drm_wakeup(ctx->vo);
}

const struct ra_ctx_fns ra_ctx_drm_egl = {
    .type           = "opengl",
    .name           = "drm",
    .description    = "DRM/EGL",
    .reconfig       = drm_egl_reconfig,
    .control        = drm_egl_control,
    .init           = drm_egl_init,
    .uninit         = drm_egl_uninit,
    .wait_events    = drm_egl_wait_events,
    .wakeup         = drm_egl_wakeup,
};
