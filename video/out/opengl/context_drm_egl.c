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

#include "libmpv/render_gl.h"
#include "video/out/drm_common.h"
#include "common/common.h"
#include "osdep/timer.h"

#include "egl_helpers.h"
#include "common.h"
#include "context.h"

#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

#ifndef EGL_EXT_platform_base
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC)
    (EGLenum platform, void *native_display, const EGLint *attrib_list);
#endif

struct framebuffer
{
    int fd;
    uint32_t width, height;
    uint32_t id;
};

struct gbm_frame {
    struct gbm_bo *bo;
    struct drm_vsync_tuple vsync;
};

struct gbm
{
    struct gbm_surface *surface;
    struct gbm_device *device;
    struct gbm_frame **bo_queue;
    unsigned int num_bos;
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

    GLsync *vsync_fences;
    unsigned int num_vsync_fences;

    uint32_t gbm_format;

    bool active;
    bool waiting_for_flip;

    bool vt_switcher_active;
    struct vt_switcher vt_switcher;

    bool still;
    bool paused;

    struct drm_vsync_tuple vsync;
    struct vo_vsync_info vsync_info;

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
    case GBM_FORMAT_XRGB2101010:
        return "GBM_FORMAT_XRGB2101010";
    case GBM_FORMAT_ARGB2101010:
        return "GBM_FORMAT_ARGB2101010";
    default:
        return "UNKNOWN";
    }
}

// Allow falling back to an ARGB EGLConfig when we have an XRGB framebuffer.
// Also allow falling back to an XRGB EGLConfig for ARGB framebuffers, since
// this seems neccessary to work with broken Mali drivers that don't report
// their EGLConfigs as supporting alpha properly.
static uint32_t fallback_format_for(uint32_t format)
{
    switch (format) {
    case GBM_FORMAT_XRGB8888:
        return GBM_FORMAT_ARGB8888;
    case GBM_FORMAT_ARGB8888:
        return GBM_FORMAT_XRGB8888;
    case GBM_FORMAT_XRGB2101010:
        return GBM_FORMAT_ARGB2101010;
    case GBM_FORMAT_ARGB2101010:
        return GBM_FORMAT_XRGB2101010;
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
    const char *ext = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (ext) {
        PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
        get_platform_display = (void *) eglGetProcAddress("eglGetPlatformDisplayEXT");

        if (get_platform_display && strstr(ext, "EGL_MESA_platform_gbm"))
            return get_platform_display(EGL_PLATFORM_GBM_MESA, gbm_device, NULL);

        if (get_platform_display && strstr(ext, "EGL_KHR_platform_gbm"))
            return get_platform_display(EGL_PLATFORM_GBM_KHR, gbm_device, NULL);
    }

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
        p->draw_surface_size.width, p->draw_surface_size.height);
    p->gbm.surface = gbm_surface_create(
        p->gbm.device,
        p->draw_surface_size.width,
        p->draw_surface_size.height,
        p->gbm_format,
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

    int ret = drmModeAddFB2(fb->fd, fb->width, fb->height,
                            p->gbm_format,
                            (uint32_t[4]){handle, 0, 0, 0},
                            (uint32_t[4]){stride, 0, 0, 0},
                            (uint32_t[4]){0, 0, 0, 0},
                            &fb->id, 0);

    if (ret) {
        MP_ERR(ctx->vo, "Failed to create framebuffer: %s\n", mp_strerror(errno));
    }
    gbm_bo_set_user_data(bo, fb, framebuffer_destroy_callback);
    p->fb = fb;
}

static bool crtc_setup_atomic(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct drm_atomic_context *atomic_ctx = p->kms->atomic_context;

    if (!drm_atomic_save_old_state(atomic_ctx)) {
        MP_WARN(ctx->vo, "Failed to save old DRM atomic state\n");
    }

    drmModeAtomicReqPtr request = drmModeAtomicAlloc();
    if (!request) {
        MP_ERR(ctx->vo, "Failed to allocate drm atomic request\n");
        return false;
    }

    if (drm_object_set_property(request, atomic_ctx->connector, "CRTC_ID", p->kms->crtc_id) < 0) {
        MP_ERR(ctx->vo, "Could not set CRTC_ID on connector\n");
        return false;
    }

    if (!drm_mode_ensure_blob(p->kms->fd, &p->kms->mode)) {
        MP_ERR(ctx->vo, "Failed to create DRM mode blob\n");
        goto err;
    }
    if (drm_object_set_property(request, atomic_ctx->crtc, "MODE_ID", p->kms->mode.blob_id) < 0) {
        MP_ERR(ctx->vo, "Could not set MODE_ID on crtc\n");
        goto err;
    }
    if (drm_object_set_property(request, atomic_ctx->crtc, "ACTIVE", 1) < 0) {
        MP_ERR(ctx->vo, "Could not set ACTIVE on crtc\n");
        goto err;
    }

    drm_object_set_property(request, atomic_ctx->draw_plane, "FB_ID", p->fb->id);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_ID", p->kms->crtc_id);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_X",   0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_Y",   0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_W",   p->draw_surface_size.width << 16);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_H",   p->draw_surface_size.height << 16);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_X",  0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_Y",  0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_W",  p->kms->mode.mode.hdisplay);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_H",  p->kms->mode.mode.vdisplay);

    int ret = drmModeAtomicCommit(p->kms->fd, request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    if (ret)
        MP_ERR(ctx->vo, "Failed to commit ModeSetting atomic request (%d)\n", ret);

    drmModeAtomicFree(request);
    return ret == 0;

  err:
    drmModeAtomicFree(request);
    return false;
}

static bool crtc_release_atomic(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    struct drm_atomic_context *atomic_ctx = p->kms->atomic_context;
    drmModeAtomicReqPtr request = drmModeAtomicAlloc();
    if (!request) {
        MP_ERR(ctx->vo, "Failed to allocate drm atomic request\n");
        return false;
    }

    if (!drm_atomic_restore_old_state(request, atomic_ctx)) {
        MP_WARN(ctx->vo, "Got error while restoring old state\n");
    }

    int ret = drmModeAtomicCommit(p->kms->fd, request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

    if (ret)
        MP_WARN(ctx->vo, "Failed to commit ModeSetting atomic request (%d)\n", ret);

    drmModeAtomicFree(request);
    return ret == 0;
}

static bool crtc_setup(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->active)
        return true;

    if (p->kms->atomic_context) {
        int ret = crtc_setup_atomic(ctx);
        p->active = true;
        return ret;
    } else {
        p->old_crtc = drmModeGetCrtc(p->kms->fd, p->kms->crtc_id);
        int ret = drmModeSetCrtc(p->kms->fd, p->kms->crtc_id, p->fb->id,
                                 0, 0, &p->kms->connector->connector_id, 1,
                                 &p->kms->mode.mode);
        p->active = true;
        return ret == 0;
    }
}

static void crtc_release(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->active)
        return;
    p->active = false;

    if (p->kms->atomic_context) {
        if (p->kms->atomic_context->old_state.saved) {
            if (!crtc_release_atomic(ctx))
                MP_ERR(ctx->vo, "Failed to restore previous mode\n");
        }
    } else {
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
}

static void release_vt(void *data)
{
    struct ra_ctx *ctx = data;
    MP_VERBOSE(ctx->vo, "Releasing VT\n");
    crtc_release(ctx);

    const struct priv *p = ctx->priv;
    if (drmDropMaster(p->kms->fd)) {
        MP_WARN(ctx->vo, "Failed to drop DRM master: %s\n",
                mp_strerror(errno));
    }
}

static void acquire_vt(void *data)
{
    struct ra_ctx *ctx = data;
    MP_VERBOSE(ctx->vo, "Acquiring VT\n");

    const struct priv *p = ctx->priv;
    if (drmSetMaster(p->kms->fd)) {
        MP_WARN(ctx->vo, "Failed to acquire DRM master: %s\n",
                mp_strerror(errno));
    }

    crtc_setup(ctx);
}

static void queue_flip(struct ra_ctx *ctx, struct gbm_frame *frame)
{
    struct priv *p = ctx->priv;
    struct drm_atomic_context *atomic_ctx = p->kms->atomic_context;
    int ret;

    update_framebuffer_from_bo(ctx, frame->bo);

    // Alloc and fill the data struct for the page flip callback
    struct drm_pflip_cb_closure *data = talloc(ctx, struct drm_pflip_cb_closure);
    data->frame_vsync = &frame->vsync;
    data->vsync = &p->vsync;
    data->vsync_info = &p->vsync_info;
    data->waiting_for_flip = &p->waiting_for_flip;
    data->log = ctx->log;

    if (atomic_ctx) {
        drm_object_set_property(atomic_ctx->request, atomic_ctx->draw_plane, "FB_ID", p->fb->id);
        drm_object_set_property(atomic_ctx->request, atomic_ctx->draw_plane, "CRTC_ID", atomic_ctx->crtc->id);
        drm_object_set_property(atomic_ctx->request, atomic_ctx->draw_plane, "ZPOS", 1);

        ret = drmModeAtomicCommit(p->kms->fd, atomic_ctx->request,
                                  DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, data);
        if (ret) {
            MP_WARN(ctx->vo, "Failed to commit atomic request (%d)\n", ret);
            talloc_free(data);
        }
    } else {
        ret = drmModePageFlip(p->kms->fd, p->kms->crtc_id, p->fb->id,
                              DRM_MODE_PAGE_FLIP_EVENT, data);
        if (ret) {
            MP_WARN(ctx->vo, "Failed to queue page flip: %s\n", mp_strerror(errno));
            talloc_free(data);
        }
    }
    p->waiting_for_flip = true;

    if (atomic_ctx) {
        drmModeAtomicFree(atomic_ctx->request);
        atomic_ctx->request = drmModeAtomicAlloc();
    }
}

static void wait_on_flip(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    // poll page flip finish event
    while (p->waiting_for_flip) {
        const int timeout_ms = 3000;
        struct pollfd fds[1] = { { .events = POLLIN, .fd = p->kms->fd } };
        poll(fds, 1, timeout_ms);
        if (fds[0].revents & POLLIN) {
            const int ret = drmHandleEvent(p->kms->fd, &p->ev);
            if (ret != 0) {
                MP_ERR(ctx->vo, "drmHandleEvent failed: %i\n", ret);
                return;
            }
        }
    }
}

static void enqueue_bo(struct ra_ctx *ctx, struct gbm_bo *bo)
{
    struct priv *p = ctx->priv;

    p->vsync.sbc++;
    struct gbm_frame *new_frame = talloc(p, struct gbm_frame);
    new_frame->bo = bo;
    new_frame->vsync = p->vsync;
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

    if (p->kms->atomic_context && !p->kms->atomic_context->request) {
        p->kms->atomic_context->request = drmModeAtomicAlloc();
        p->drm_params.atomic_request_ptr = &p->kms->atomic_context->request;
    }

    return ra_gl_ctx_start_frame(sw, out_fbo);
}

static bool drm_egl_submit_frame(struct ra_swapchain *sw, const struct vo_frame *frame)
{
    struct ra_ctx *ctx = sw->ctx;
    struct priv *p = ctx->priv;

    p->still = frame->still;

    return ra_gl_ctx_submit_frame(sw, frame);
}

static void drm_egl_swap_buffers(struct ra_swapchain *sw)
{
    struct ra_ctx *ctx = sw->ctx;
    struct priv *p = ctx->priv;
    const bool drain = p->paused || p->still;  // True when we need to drain the swapchain

    if (!p->active)
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
        if (p->waiting_for_flip) {
            wait_on_flip(ctx);
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
    struct drm_atomic_context *atomic_ctx = p->kms->atomic_context;

    if (atomic_ctx) {
        int ret = drmModeAtomicCommit(p->kms->fd, atomic_ctx->request, 0, NULL);
        if (ret)
            MP_ERR(ctx->vo, "Failed to commit atomic request (%d)\n", ret);
        drmModeAtomicFree(atomic_ctx->request);
    }

    ra_gl_ctx_uninit(ctx);

    crtc_release(ctx);
    if (p->vt_switcher_active)
        vt_switcher_destroy(&p->vt_switcher);

    // According to GBM documentation all BO:s must be released before
    // gbm_surface_destroy can be called on the surface.
    while (p->gbm.num_bos) {
        swapchain_step(ctx);
    }

    eglMakeCurrent(p->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    eglDestroyContext(p->egl.display, p->egl.context);
    eglDestroySurface(p->egl.display, p->egl.surface);
    gbm_surface_destroy(p->gbm.surface);
    eglTerminate(p->egl.display);
    gbm_device_destroy(p->gbm.device);
    p->egl.context = EGL_NO_CONTEXT;
    eglDestroyContext(p->egl.display, p->egl.context);

    close(p->drm_params.render_fd);

    if (p->kms) {
        kms_destroy(p->kms);
        p->kms = 0;
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

    if (!p->kms->atomic_context) {
        p->gbm_format = xrgb_format;
        MP_VERBOSE(ctx->vo, "Not using DRM Atomic: Use %s for draw plane.\n",
                   gbm_format_to_string(xrgb_format));
        return true;
    }

    drmModePlane *drmplane =
        drmModeGetPlane(p->kms->fd, p->kms->atomic_context->draw_plane->id);
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

static void drm_egl_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct priv *p = ctx->priv;
    *info = p->vsync_info;
}

static bool drm_egl_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    p->ev.version = DRM_EVENT_CONTEXT_VERSION;
    p->ev.page_flip_handler = &drm_pflip_cb;

    p->vt_switcher_active = vt_switcher_init(&p->vt_switcher, ctx->vo->log);
    if (p->vt_switcher_active) {
        vt_switcher_acquire(&p->vt_switcher, acquire_vt, ctx);
        vt_switcher_release(&p->vt_switcher, release_vt, ctx);
    } else {
        MP_WARN(ctx, "Failed to set up VT switcher. Terminal switching will be unavailable.\n");
    }

    MP_VERBOSE(ctx, "Initializing KMS\n");
    p->kms = kms_create(ctx->log,
                        ctx->vo->opts->drm_opts->drm_device_path,
                        ctx->vo->opts->drm_opts->drm_connector_spec,
                        ctx->vo->opts->drm_opts->drm_mode_spec,
                        ctx->vo->opts->drm_opts->drm_draw_plane,
                        ctx->vo->opts->drm_opts->drm_drmprime_video_plane,
                        ctx->vo->opts->drm_opts->drm_atomic);
    if (!p->kms) {
        MP_ERR(ctx, "Failed to create KMS.\n");
        return false;
    }

    if (ctx->vo->opts->drm_opts->drm_draw_surface_size.wh_valid) {
        if (p->kms->atomic_context) {
            p->draw_surface_size.width = ctx->vo->opts->drm_opts->drm_draw_surface_size.w;
            p->draw_surface_size.height = ctx->vo->opts->drm_opts->drm_draw_surface_size.h;
        } else {
            p->draw_surface_size.width = p->kms->mode.mode.hdisplay;
            p->draw_surface_size.height = p->kms->mode.mode.vdisplay;
            MP_WARN(ctx, "Setting draw plane size is only available with DRM atomic, defaulting to screen resolution\n");
        }
    } else {
        p->draw_surface_size.width = p->kms->mode.mode.hdisplay;
        p->draw_surface_size.height = p->kms->mode.mode.vdisplay;
    }

    uint32_t argb_format;
    uint32_t xrgb_format;
    if (DRM_OPTS_FORMAT_XRGB2101010 == ctx->vo->opts->drm_opts->drm_format) {
        argb_format = GBM_FORMAT_ARGB2101010;
        xrgb_format = GBM_FORMAT_XRGB2101010;
    } else {
        argb_format = GBM_FORMAT_ARGB8888;
        xrgb_format = GBM_FORMAT_XRGB8888;
    }

    if (!probe_gbm_format(ctx, argb_format, xrgb_format)) {
        MP_ERR(ctx->vo, "No suitable format found on draw plane (tried: %s and %s).\n",
               gbm_format_to_string(argb_format), gbm_format_to_string(xrgb_format));
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
    struct gbm_bo *new_bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    if (!new_bo) {
        MP_ERR(ctx, "Failed to lock GBM surface.\n");
        return false;
    }
    enqueue_bo(ctx, new_bo);
    update_framebuffer_from_bo(ctx, new_bo);
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
    p->drm_params.connector_id = p->kms->connector->connector_id;
    if (p->kms->atomic_context)
        p->drm_params.atomic_request_ptr = &p->kms->atomic_context->request;
    char *rendernode_path = drmGetRenderDeviceNameFromFd(p->kms->fd);
    if (rendernode_path) {
        MP_VERBOSE(ctx, "Opening render node \"%s\"\n", rendernode_path);
        p->drm_params.render_fd = open(rendernode_path, O_RDWR | O_CLOEXEC);
        if (p->drm_params.render_fd == -1) {
            MP_WARN(ctx, "Cannot open render node \"%s\": %s. VAAPI hwdec will be disabled\n",
                    rendernode_path, mp_strerror(errno));
        }
        free(rendernode_path);
    } else {
        p->drm_params.render_fd = -1;
        MP_VERBOSE(ctx, "Could not find path to render node. VAAPI hwdec will be disabled\n");
    }

    struct ra_gl_ctx_params params = {
        .external_swapchain = &drm_egl_swapchain,
        .get_vsync          = &drm_egl_get_vsync,
    };
    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        return false;

    ra_add_native_resource(ctx->ra, "drm_params_v2", &p->drm_params);
    ra_add_native_resource(ctx->ra, "drm_draw_surface_size", &p->draw_surface_size);

    p->vsync_info.vsync_duration = 0;
    p->vsync_info.skipped_vsyncs = -1;
    p->vsync_info.last_queue_display_time = -1;

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
    case VOCTRL_GET_DISPLAY_RES: {
        ((int *)arg)[0] = p->kms->mode.mode.hdisplay;
        ((int *)arg)[1] = p->kms->mode.mode.vdisplay;
        return VO_TRUE;
    }
    case VOCTRL_PAUSE:
        ctx->vo->want_redraw = true;
        p->paused = true;
        return VO_TRUE;
    case VOCTRL_RESUME:
        p->paused = false;
        p->vsync_info.last_queue_display_time = -1;
        p->vsync_info.skipped_vsyncs = 0;
        p->vsync.ust = 0;
        p->vsync.msc = 0;
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

static void wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    struct priv *p = ctx->priv;
    if (p->vt_switcher_active) {
        int64_t wait_us = until_time_us - mp_time_us();
        int timeout_ms = MPCLAMP((wait_us + 500) / 1000, 0, 10000);
        vt_switcher_poll(&p->vt_switcher, timeout_ms);
    } else {
        vo_wait_default(ctx->vo, until_time_us);
    }
}

static void wakeup(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->vt_switcher_active)
        vt_switcher_interrupt_poll(&p->vt_switcher);
}

const struct ra_ctx_fns ra_ctx_drm_egl = {
    .type           = "opengl",
    .name           = "drm",
    .reconfig       = drm_egl_reconfig,
    .control        = drm_egl_control,
    .init           = drm_egl_init,
    .uninit         = drm_egl_uninit,
    .wait_events    = wait_events,
    .wakeup         = wakeup,
};
