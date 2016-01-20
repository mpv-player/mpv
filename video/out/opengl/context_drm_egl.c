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
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include "context.h"
#include "common/common.h"
#include "video/out/drm_common.h"

#define USE_MASTER 0

struct framebuffer
{
    struct gbm_bo *bo;
    int width, height;
    int fd;
    int id;
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
    struct kms *kms;

    drmEventContext ev;
    drmModeCrtc *old_crtc;

    struct egl egl;
    struct gbm gbm;
    struct framebuffer fb;

    bool active;
    bool waiting_for_flip;

    bool vt_switcher_active;
    struct vt_switcher vt_switcher;
};

static EGLConfig select_fb_config_egl(struct MPGLContext *ctx, bool es)
{
    struct priv *p = ctx->priv;
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 1,
        EGL_RENDERABLE_TYPE, es ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_NONE
    };
    EGLint config_count;
    EGLConfig config;
    if (!eglChooseConfig(p->egl.display, attributes, &config, 1, &config_count)) {
        MP_FATAL(ctx->vo, "Failed to configure EGL.\n");
        return NULL;
    }
    if (!config_count) {
        MP_FATAL(ctx->vo, "Could not find EGL configuration!\n");
        return NULL;
    }
    return config;
}

static bool init_egl(struct MPGLContext *ctx, bool es)
{
    struct priv *p = ctx->priv;
    MP_VERBOSE(ctx->vo, "Initializing EGL\n");
    p->egl.display = eglGetDisplay(p->gbm.device);
    if (p->egl.display == EGL_NO_DISPLAY) {
        MP_ERR(ctx->vo, "Failed to get EGL display.\n");
        return false;
    }
    if (!eglInitialize(p->egl.display, NULL, NULL)) {
        MP_ERR(ctx->vo, "Failed to initialize EGL.\n");
        return false;
    }
    if (!eglBindAPI(es ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
        MP_ERR(ctx->vo, "Failed to set EGL API version.\n");
        return false;
    }
    EGLConfig config = select_fb_config_egl(ctx, es);
    if (!config) {
        MP_ERR(ctx->vo, "Failed to configure EGL.\n");
        return false;
    }
    p->egl.context = eglCreateContext(p->egl.display, config, EGL_NO_CONTEXT, NULL);
    if (!p->egl.context) {
        MP_ERR(ctx->vo, "Failed to create EGL context.\n");
        return false;
    }
    MP_VERBOSE(ctx->vo, "Initializing EGL surface\n");
    p->egl.surface = eglCreateWindowSurface(p->egl.display, config, p->gbm.surface, NULL);
    if (p->egl.surface == EGL_NO_SURFACE) {
        MP_ERR(ctx->vo, "Failed to create EGL surface.\n");
        return false;
    }
    return true;
}

static bool init_gbm(struct MPGLContext *ctx)
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
        GBM_BO_FORMAT_XRGB8888,
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

static void update_framebuffer_from_bo(
    const struct MPGLContext *ctx, struct gbm_bo *bo)
{
    struct priv *p = ctx->priv;
    p->fb.bo = bo;
    p->fb.fd = p->kms->fd;
    p->fb.width = gbm_bo_get_width(bo);
    p->fb.height = gbm_bo_get_height(bo);
    int stride = gbm_bo_get_stride(bo);
    int handle = gbm_bo_get_handle(bo).u32;

    int ret = drmModeAddFB(p->kms->fd, p->fb.width, p->fb.height,
                       24, 32, stride, handle, &p->fb.id);
    if (ret) {
        MP_ERR(ctx->vo, "Failed to create framebuffer: %s\n", mp_strerror(errno));
    }
    gbm_bo_set_user_data(bo, &p->fb, framebuffer_destroy_callback);
}

static void page_flipped(int fd, unsigned int frame, unsigned int sec,
                                 unsigned int usec, void *data)
{
    struct priv *p = data;
    p->waiting_for_flip = false;
}

static bool crtc_setup(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    if (p->active)
        return true;
    p->old_crtc = drmModeGetCrtc(p->kms->fd, p->kms->crtc_id);
    int ret = drmModeSetCrtc(p->kms->fd, p->kms->crtc_id,
                             p->fb.id,
                             0,
                             0,
                             &p->kms->connector->connector_id,
                             1,
                             &p->kms->mode);
    p->active = true;
    return ret == 0;
}

static void crtc_release(struct MPGLContext *ctx)
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
                       p->old_crtc->crtc_id,
                       p->old_crtc->buffer_id,
                       p->old_crtc->x,
                       p->old_crtc->y,
                       &p->kms->connector->connector_id,
                       1,
                       &p->old_crtc->mode);
        drmModeFreeCrtc(p->old_crtc);
        p->old_crtc = NULL;
    }
}

static void release_vt(void *data)
{
    struct MPGLContext *ctx = data;
    MP_VERBOSE(ctx->vo, "Releasing VT");
    crtc_release(ctx);
    if (USE_MASTER) {
        //this function enables support for switching to x, weston etc.
        //however, for whatever reason, it can be called only by root users.
        //until things change, this is commented.
        struct priv *p = ctx->priv;
        if (drmDropMaster(p->kms->fd)) {
            MP_WARN(ctx->vo, "Failed to drop DRM master: %s\n", mp_strerror(errno));
        }
    }
}

static void acquire_vt(void *data)
{
    struct MPGLContext *ctx = data;
    MP_VERBOSE(ctx->vo, "Acquiring VT");
    if (USE_MASTER) {
        struct priv *p = ctx->priv;
        if (drmSetMaster(p->kms->fd)) {
            MP_WARN(ctx->vo, "Failed to acquire DRM master: %s\n", mp_strerror(errno));
        }
    }

    crtc_setup(ctx);
}

static void drm_egl_uninit(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    crtc_release(ctx);

    if (p->vt_switcher_active)
        vt_switcher_destroy(&p->vt_switcher);

    eglMakeCurrent(p->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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

static int drm_egl_init(struct MPGLContext *ctx, int flags)
{
    if (ctx->vo->probing) {
        MP_VERBOSE(ctx->vo, "DRM EGL backend can be activated only manually.\n");
        return -1;
    }
    struct priv *p = ctx->priv;
    p->kms = NULL;
    p->old_crtc = NULL;
    p->gbm.surface = NULL;
    p->gbm.device = NULL;
    p->active = false;
    p->waiting_for_flip = false;
    p->ev.version = DRM_EVENT_CONTEXT_VERSION;
    p->ev.page_flip_handler = page_flipped;

    p->vt_switcher_active = vt_switcher_init(&p->vt_switcher, ctx->vo->log);
    if (p->vt_switcher_active) {
        vt_switcher_acquire(&p->vt_switcher, acquire_vt, ctx);
        vt_switcher_release(&p->vt_switcher, release_vt, ctx);
    } else {
        MP_WARN(ctx->vo, "Failed to set up VT switcher. Terminal switching will be unavailable.\n");
    }

    MP_VERBOSE(ctx->vo, "Initializing KMS\n");
    p->kms = kms_create(ctx->vo->log);
    if (!p->kms) {
        MP_ERR(ctx->vo, "Failed to create KMS.\n");
        return -1;
    }

    // TODO: arguments should be configurable
    if (!kms_setup(p->kms, "/dev/dri/card0", -1, 0)) {
        MP_ERR(ctx->vo, "Failed to configure KMS.\n");
        return -1;
    }

    if (!init_gbm(ctx)) {
        MP_ERR(ctx->vo, "Failed to setup GBM.\n");
        return -1;
    }

    if (!init_egl(ctx, flags & VOFLAG_GLES)) {
        MP_ERR(ctx->vo, "Failed to setup EGL.\n");
        return -1;
    }

    if (!eglMakeCurrent(p->egl.display, p->egl.surface, p->egl.surface, p->egl.context)) {
        MP_ERR(ctx->vo, "Failed to make context current.\n");
        return -1;
    }

    const char *egl_exts = eglQueryString(p->egl.display, EGL_EXTENSIONS);
    void *(*gpa)(const GLubyte*) = (void *(*)(const GLubyte*))eglGetProcAddress;
    mpgl_load_functions(ctx->gl, gpa, egl_exts, ctx->vo->log);

    ctx->native_display_type = "drm";
    ctx->native_display = (void *)(intptr_t)p->kms->fd;

    // required by gbm_surface_lock_front_buffer
    eglSwapBuffers(p->egl.display, p->egl.surface);

    MP_VERBOSE(ctx->vo, "Preparing framebuffer\n");
    p->gbm.bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    if (!p->gbm.bo) {
        MP_ERR(ctx->vo, "Failed to lock GBM surface.\n");
        return -1;
    }
    update_framebuffer_from_bo(ctx, p->gbm.bo);
    if (!p->fb.id) {
        MP_ERR(ctx->vo, "Failed to create framebuffer.\n");
        return -1;
    }

    if (!crtc_setup(ctx)) {
        MP_ERR(
               ctx->vo,
               "Failed to set CRTC for connector %u: %s\n",
               p->kms->connector->connector_id,
               mp_strerror(errno));
        return -1;
    }

    return 0;
}

static int drm_egl_reconfig(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    ctx->vo->dwidth = p->fb.width;
    ctx->vo->dheight = p->fb.height;
    return 0;
}

static int drm_egl_control(struct MPGLContext *ctx, int *events, int request,
                           void *arg)
{
    return VO_NOTIMPL;
}

static void drm_egl_swap_buffers(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl.display, p->egl.surface);
    p->gbm.next_bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    p->waiting_for_flip = true;
    update_framebuffer_from_bo(ctx, p->gbm.next_bo);
    int ret = drmModePageFlip(p->kms->fd, p->kms->crtc_id, p->fb.id,
                              DRM_MODE_PAGE_FLIP_EVENT, p);
    if (ret) {
        MP_WARN(ctx->vo, "Failed to queue page flip: %s\n", mp_strerror(errno));
    }

    // poll page flip finish event
    const int timeout_ms = 3000;
    struct pollfd fds[1] = { { .events = POLLIN, .fd = p->kms->fd } };
    poll(fds, 1, timeout_ms);
    if (fds[0].revents & POLLIN) {
        ret = drmHandleEvent(p->kms->fd, &p->ev);
        if (ret != 0) {
            MP_ERR(ctx->vo, "drmHandleEvent failed: %i\n", ret);
            return;
        }
    }

    gbm_surface_release_buffer(p->gbm.surface, p->gbm.bo);
    p->gbm.bo = p->gbm.next_bo;
}

const struct mpgl_driver mpgl_driver_drm_egl = {
    .name           = "drm-egl",
    .priv_size      = sizeof(struct priv),
    .init           = drm_egl_init,
    .reconfig       = drm_egl_reconfig,
    .swap_buffers   = drm_egl_swap_buffers,
    .control        = drm_egl_control,
    .uninit         = drm_egl_uninit,
};
