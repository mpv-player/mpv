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
#include <time.h>
#include <unistd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "libmpv/opengl_cb.h"
#include "video/out/drm_common.h"
#include "common/common.h"

#include "egl_helpers.h"
#include "common.h"
#include "context.h"

#define USE_MASTER 0

struct framebuffer
{
    int fd;
    uint32_t width, height;
    uint32_t id;
};

struct gbm
{
    struct gbm_surface *surface;
    struct gbm_device *device;
    struct gbm_bo *bo;
    struct gbm_bo *next_bo;
};

struct egl
{
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
};

struct priv {
    GL gl;
    struct kms *kms;

    drmEventContext ev;
    drmModeCrtc *old_crtc;

    struct egl egl;
    struct gbm gbm;
    struct framebuffer *fb;

    bool active;
    bool waiting_for_flip;

    bool vt_switcher_active;
    struct vt_switcher vt_switcher;

    struct mpv_opengl_cb_drm_params drm_params;
};

static bool init_egl(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    MP_VERBOSE(ctx, "Initializing EGL\n");
    p->egl.display = eglGetDisplay(p->gbm.device);
    if (p->egl.display == EGL_NO_DISPLAY) {
        MP_ERR(ctx, "Failed to get EGL display.\n");
        return false;
    }
    if (!eglInitialize(p->egl.display, NULL, NULL)) {
        MP_ERR(ctx, "Failed to initialize EGL.\n");
        return false;
    }
    EGLConfig config;
    if (!mpegl_create_context(ctx, p->egl.display, &p->egl.context, &config))
        return false;
    MP_VERBOSE(ctx, "Initializing EGL surface\n");
    p->egl.surface
        = eglCreateWindowSurface(p->egl.display, config, p->gbm.surface, NULL);
    if (p->egl.surface == EGL_NO_SURFACE) {
        MP_ERR(ctx, "Failed to create EGL surface.\n");
        return false;
    }
    return true;
}

static bool init_gbm(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    MP_VERBOSE(ctx->vo, "Creating GBM device\n");
    p->gbm.device = gbm_create_device(p->kms->fd);
    if (!p->gbm.device) {
        MP_ERR(ctx->vo, "Failed to create GBM device.\n");
        return false;
    }

    MP_VERBOSE(ctx->vo, "Initializing GBM surface (%d x %d)\n",
        p->kms->mode.hdisplay, p->kms->mode.vdisplay);
    p->gbm.surface = gbm_surface_create(
        p->gbm.device,
        p->kms->mode.hdisplay,
        p->kms->mode.vdisplay,
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
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
    struct framebuffer *fb = gbm_bo_get_user_data(bo);
    if (fb) {
        p->fb = fb;
        return;
    }

    fb = talloc_zero(ctx, struct framebuffer);
    fb->fd     = p->kms->fd;
    fb->width  = gbm_bo_get_width(bo);
    fb->height = gbm_bo_get_height(bo);
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;

    int ret = drmModeAddFB(fb->fd, fb->width, fb->height,
                           32, 32, stride, handle, &fb->id);
    if (ret) {
        MP_ERR(ctx->vo, "Failed to create framebuffer: %s\n", mp_strerror(errno));
    }
    gbm_bo_set_user_data(bo, fb, framebuffer_destroy_callback);
    p->fb = fb;
}

static bool crtc_setup(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->active)
        return true;
    p->old_crtc = drmModeGetCrtc(p->kms->fd, p->kms->crtc_id);
    int ret = drmModeSetCrtc(p->kms->fd, p->kms->crtc_id, p->fb->id,
                             0, 0, &p->kms->connector->connector_id, 1,
                             &p->kms->mode);
    p->active = true;
    return ret == 0;
}

static void crtc_release(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->active)
        return;
    p->active = false;

    // wait for current page flip
    while (p->waiting_for_flip) {
        int ret = drmHandleEvent(p->kms->fd, &p->ev);
        if (ret) {
            MP_ERR(ctx->vo, "drmHandleEvent failed: %i\n", ret);
            break;
        }
    }

    if (p->old_crtc) {
        drmModeSetCrtc(p->kms->fd,
                       p->old_crtc->crtc_id, p->old_crtc->buffer_id,
                       p->old_crtc->x, p->old_crtc->y,
                       &p->kms->connector->connector_id, 1,
                       &p->old_crtc->mode);
        drmModeFreeCrtc(p->old_crtc);
        p->old_crtc = NULL;
    }
}

static void release_vt(void *data)
{
    struct ra_ctx *ctx = data;
    MP_VERBOSE(ctx->vo, "Releasing VT");
    crtc_release(ctx);
    if (USE_MASTER) {
        //this function enables support for switching to x, weston etc.
        //however, for whatever reason, it can be called only by root users.
        //until things change, this is commented.
        struct priv *p = ctx->priv;
        if (drmDropMaster(p->kms->fd)) {
            MP_WARN(ctx->vo, "Failed to drop DRM master: %s\n",
                    mp_strerror(errno));
        }
    }
}

static void acquire_vt(void *data)
{
    struct ra_ctx *ctx = data;
    MP_VERBOSE(ctx->vo, "Acquiring VT");
    if (USE_MASTER) {
        struct priv *p = ctx->priv;
        if (drmSetMaster(p->kms->fd)) {
            MP_WARN(ctx->vo, "Failed to acquire DRM master: %s\n",
                    mp_strerror(errno));
        }
    }

    crtc_setup(ctx);
}

static bool drm_atomic_egl_start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->ctx->priv;
    if (p->kms->atomic_context) {
        p->kms->atomic_context->request = drmModeAtomicAlloc();
        p->drm_params.atomic_request = p->kms->atomic_context->request;
        return ra_gl_ctx_start_frame(sw, out_fbo);
    }
    return false;
}

static const struct ra_swapchain_fns drm_atomic_swapchain = {
    .start_frame   = drm_atomic_egl_start_frame,
};

static void drm_egl_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct drm_atomic_context *atomic_ctx = p->kms->atomic_context;
    int ret;

    eglSwapBuffers(p->egl.display, p->egl.surface);
    p->gbm.next_bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    p->waiting_for_flip = true;
    update_framebuffer_from_bo(ctx, p->gbm.next_bo);

    if (atomic_ctx) {
        drm_object_set_property(atomic_ctx->request, atomic_ctx->primary_plane, "FB_ID", p->fb->id);
        drm_object_set_property(atomic_ctx->request, atomic_ctx->primary_plane, "CRTC_ID", atomic_ctx->crtc->id);
        drm_object_set_property(atomic_ctx->request, atomic_ctx->primary_plane, "ZPOS", 1);

        ret = drmModeAtomicCommit(p->kms->fd, atomic_ctx->request,
                                  DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, NULL);
        if (ret)
            MP_WARN(ctx->vo, "Failed to commit atomic request (%d)\n", ret);
    } else {
        ret = drmModePageFlip(p->kms->fd, p->kms->crtc_id, p->fb->id,
                                  DRM_MODE_PAGE_FLIP_EVENT, p);
        if (ret) {
            MP_WARN(ctx->vo, "Failed to queue page flip: %s\n", mp_strerror(errno));
        }
    }

    // poll page flip finish event
    const int timeout_ms = 3000;
    struct pollfd fds[1] = { { .events = POLLIN, .fd = p->kms->fd } };
    poll(fds, 1, timeout_ms);
    if (fds[0].revents & POLLIN) {
        ret = drmHandleEvent(p->kms->fd, &p->ev);
        if (ret != 0) {
            MP_ERR(ctx->vo, "drmHandleEvent failed: %i\n", ret);
            p->waiting_for_flip = false;
            return;
        }
    }
    p->waiting_for_flip = false;

    if (atomic_ctx) {
        drmModeAtomicFree(atomic_ctx->request);
        p->drm_params.atomic_request = atomic_ctx->request = NULL;
    }

    gbm_surface_release_buffer(p->gbm.surface, p->gbm.bo);
    p->gbm.bo = p->gbm.next_bo;
}

static void drm_egl_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

    crtc_release(ctx);
    if (p->vt_switcher_active)
        vt_switcher_destroy(&p->vt_switcher);

    eglMakeCurrent(p->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    eglDestroyContext(p->egl.display, p->egl.context);
    eglDestroySurface(p->egl.display, p->egl.surface);
    gbm_surface_destroy(p->gbm.surface);
    eglTerminate(p->egl.display);
    gbm_device_destroy(p->gbm.device);
    p->egl.context = EGL_NO_CONTEXT;
    eglDestroyContext(p->egl.display, p->egl.context);

    if (p->kms) {
        kms_destroy(p->kms);
        p->kms = 0;
    }
}

static bool drm_egl_init(struct ra_ctx *ctx)
{
    if (ctx->opts.probing) {
        MP_VERBOSE(ctx, "DRM EGL backend can be activated only manually.\n");
        return false;
    }

    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    p->ev.version = DRM_EVENT_CONTEXT_VERSION;

    p->vt_switcher_active = vt_switcher_init(&p->vt_switcher, ctx->vo->log);
    if (p->vt_switcher_active) {
        vt_switcher_acquire(&p->vt_switcher, acquire_vt, ctx);
        vt_switcher_release(&p->vt_switcher, release_vt, ctx);
    } else {
        MP_WARN(ctx, "Failed to set up VT switcher. Terminal switching will be unavailable.\n");
    }

    MP_VERBOSE(ctx, "Initializing KMS\n");
    p->kms = kms_create(ctx->log, ctx->vo->opts->drm_connector_spec,
                        ctx->vo->opts->drm_mode_id, ctx->vo->opts->drm_overlay_id);
    if (!p->kms) {
        MP_ERR(ctx, "Failed to create KMS.\n");
        return false;
    }

    if (!init_gbm(ctx)) {
        MP_ERR(ctx->vo, "Failed to setup GBM.\n");
        return false;
    }

    if (!init_egl(ctx)) {
        MP_ERR(ctx->vo, "Failed to setup EGL.\n");
        return false;
    }

    if (!eglMakeCurrent(p->egl.display, p->egl.surface, p->egl.surface,
                        p->egl.context)) {
        MP_ERR(ctx->vo, "Failed to make context current.\n");
        return false;
    }

    mpegl_load_functions(&p->gl, ctx->vo->log);
    // required by gbm_surface_lock_front_buffer
    eglSwapBuffers(p->egl.display, p->egl.surface);

    MP_VERBOSE(ctx, "Preparing framebuffer\n");
    p->gbm.bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    if (!p->gbm.bo) {
        MP_ERR(ctx, "Failed to lock GBM surface.\n");
        return false;
    }
    update_framebuffer_from_bo(ctx, p->gbm.bo);
    if (!p->fb || !p->fb->id) {
        MP_ERR(ctx, "Failed to create framebuffer.\n");
        return false;
    }

    if (!crtc_setup(ctx)) {
        MP_ERR(ctx, "Failed to set CRTC for connector %u: %s\n",
               p->kms->connector->connector_id, mp_strerror(errno));
        return false;
    }

    p->drm_params.fd = p->kms->fd;
    p->drm_params.crtc_id = p->kms->crtc_id;
    p->drm_params.atomic_request = p->kms->atomic_context->request;
    struct ra_gl_ctx_params params = {
        .swap_buffers = drm_egl_swap_buffers,
        .native_display_type = "opengl-cb-drm-params",
        .native_display = &p->drm_params,
        .external_swapchain = p->kms->atomic_context ? &drm_atomic_swapchain :
                                                       NULL,
    };
    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        return false;

    return true;
}

static bool drm_egl_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ctx->vo->dwidth  = p->fb->width;
    ctx->vo->dheight = p->fb->height;
    ra_gl_ctx_resize(ctx->swapchain, p->fb->width, p->fb->height, 0);
    return true;
}

static int drm_egl_control(struct ra_ctx *ctx, int *events, int request,
                           void *arg)
{
    struct priv *p = ctx->priv;
    switch (request) {
    case VOCTRL_GET_DISPLAY_FPS: {
        double fps = kms_get_display_fps(p->kms);
        if (fps <= 0)
            break;
        *(double*)arg = fps;
        return VO_TRUE;
    }
    }
    return VO_NOTIMPL;
}

const struct ra_ctx_fns ra_ctx_drm_egl = {
    .type           = "opengl",
    .name           = "drm",
    .reconfig       = drm_egl_reconfig,
    .control        = drm_egl_control,
    .init           = drm_egl_init,
    .uninit         = drm_egl_uninit,
};
