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

#include <X11/Xlib.h>
#include <GL/glx.h>

#include "video/vdpau.h"
#include "video/out/x11_common.h"
#include "context.h"

// This is a GL_NV_vdpau_interop specification bug, and headers (unfortunately)
// follow it. I'm not sure about the original nvidia headers.
#define BRAINDEATH(x) ((void *)(uintptr_t)(x))

#define NUM_SURFACES 4

struct surface {
    int w, h;
    VdpOutputSurface surface;
    // This nested shitshow of handles to the same object piss me off.
    GLvdpauSurfaceNV registered;
    bool mapped;
    GLuint texture;
    GLuint fbo;
};

struct priv {
    GLXContext context;
    struct mp_vdpau_ctx *vdp;
    VdpPresentationQueueTarget vdp_target;
    VdpPresentationQueue vdp_queue;
    int num_surfaces;
    struct surface surfaces[NUM_SURFACES];
    int current_surface;
};

typedef GLXContext (*glXCreateContextAttribsARBProc)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

static bool create_context_x11(struct MPGLContext *ctx, int vo_flags)
{
    struct priv *glx_ctx = ctx->priv;
    struct vo *vo = ctx->vo;

    int glx_major, glx_minor;
    if (!glXQueryVersion(vo->x11->display, &glx_major, &glx_minor)) {
        MP_ERR(vo, "GLX not found.\n");
        return false;
    }

    int glx_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DOUBLEBUFFER, True,
        None
    };
    int fbcount;
    GLXFBConfig *fbcs = glXChooseFBConfig(vo->x11->display, vo->x11->screen,
                                          glx_attribs, &fbcount);
    if (!fbcs)
        return false;
    // The list in fbc is sorted (so that the first element is the best).
    GLXFBConfig fbc = fbcount > 0 ? fbcs[0] : NULL;
    XFree(fbcs);
    if (!fbc) {
        MP_ERR(vo, "no GLX support present\n");
        return false;
    }

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)
            glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    const char *glxstr =
        glXQueryExtensionsString(vo->x11->display, vo->x11->screen);
    bool have_ctx_ext = glxstr && !!strstr(glxstr, "GLX_ARB_create_context");

    if (!(have_ctx_ext && glXCreateContextAttribsARB)) {
        return false;
    }

    int ctx_flags = vo_flags & VOFLAG_GL_DEBUG ? GLX_CONTEXT_DEBUG_BIT_ARB : 0;
    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB, ctx_flags,
        None
    };
    GLXContext context = glXCreateContextAttribsARB(vo->x11->display, fbc, 0,
                                                    True, context_attribs);
    if (!context)
        return false;

    // Pass 0 as drawable for offscreen use. This is probably (?) not valid in
    // standard GLX, but the nVidia drivers accept it.
    if (!glXMakeCurrent(vo->x11->display, 0, context)) {
        MP_FATAL(vo, "Could not set GLX context!\n");
        glXDestroyContext(vo->x11->display, context);
        return false;
    }

    glx_ctx->context = context;
    mpgl_load_functions(ctx->gl, (void *)glXGetProcAddressARB, glxstr, vo->log);
    return true;
}

static int create_vdpau_objects(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    VdpDevice dev = p->vdp->vdp_device;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;

    ctx->gl->VDPAUInitNV(BRAINDEATH(dev), p->vdp->get_proc_address);

    vdp_st = vdp->presentation_queue_target_create_x11(dev, ctx->vo->x11->window,
                                                       &p->vdp_target);
    CHECK_VDP_ERROR(ctx, "creating vdp target");

    vdp_st = vdp->presentation_queue_create(dev, p->vdp_target, &p->vdp_queue);
    CHECK_VDP_ERROR(ctx, "creating vdp presentation queue");

    return 0;
}

static void destroy_vdpau_surface(struct MPGLContext *ctx,
                                  struct surface *surface)
{
    struct priv *p = ctx->priv;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;
    GL *gl = ctx->gl;

    if (surface->mapped)
        gl->VDPAUUnmapSurfacesNV(1, &surface->registered);

    gl->DeleteFramebuffers(1, &surface->fbo);
    gl->DeleteTextures(1, &surface->texture);

    if (surface->registered)
        gl->VDPAUUnregisterSurfaceNV(surface->registered);

    if (surface->surface != VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_destroy(surface->surface);
        CHECK_VDP_WARNING(ctx, "destroying vdpau surface");
    }

    *surface = (struct surface){
        .surface = VDP_INVALID_HANDLE,
    };
}

static int recreate_vdpau_surface(struct MPGLContext *ctx,
                                  struct surface *surface)
{
    struct priv *p = ctx->priv;
    VdpDevice dev = p->vdp->vdp_device;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;
    GL *gl = ctx->gl;

    destroy_vdpau_surface(ctx, surface);

    surface->w = ctx->vo->dwidth;
    surface->h = ctx->vo->dheight;

    vdp_st = vdp->output_surface_create(dev, VDP_RGBA_FORMAT_B8G8R8A8,
                                        surface->w, surface->h,
                                        &surface->surface);
    CHECK_VDP_ERROR_NORETURN(ctx, "creating vdp output surface");
    if (vdp_st != VDP_STATUS_OK)
        goto error;

    gl->GenTextures(1, &surface->texture);

    surface->registered =
        gl->VDPAURegisterOutputSurfaceNV(BRAINDEATH(surface->surface),
                                         GL_TEXTURE_2D,
                                         1, &surface->texture);
    if (!surface->registered) {
        MP_ERR(ctx, "could not register vdpau surface with GL\n");
        goto error;
    }

    gl->VDPAUSurfaceAccessNV(surface->registered, GL_WRITE_DISCARD_NV);
    gl->VDPAUMapSurfacesNV(1, &surface->registered);
    surface->mapped = true;

    gl->GenFramebuffers(1, &surface->fbo);
    gl->BindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, surface->texture, 0);
    GLenum err = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (err != GL_FRAMEBUFFER_COMPLETE) {
        MP_ERR(ctx, "Framebuffer completeness check failed (error=%d).\n",
               (int)err);
        goto error;
    }
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    gl->VDPAUUnmapSurfacesNV(1, &surface->registered);
    surface->mapped = false;

    return 0;

error:
    destroy_vdpau_surface(ctx, surface);
    return -1;
}

static void glx_uninit(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;

    if (p->vdp) {
        struct vdp_functions *vdp = &p->vdp->vdp;
        VdpStatus vdp_st;

        for (int n = 0; n < p->num_surfaces; n++)
            destroy_vdpau_surface(ctx, &p->surfaces[n]);

        if (p->vdp_queue != VDP_INVALID_HANDLE) {
            vdp_st = vdp->presentation_queue_destroy(p->vdp_queue);
            CHECK_VDP_WARNING(ctx, "destroying presentation queue");
        }

        if (p->vdp_target != VDP_INVALID_HANDLE) {
            vdp_st = vdp->presentation_queue_target_destroy(p->vdp_target);
            CHECK_VDP_WARNING(ctx, "destroying presentation target");
        }

        mp_vdpau_destroy(p->vdp);
    }

    if (p->context) {
        Display *display = ctx->vo->x11->display;
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, p->context);
    }

    vo_x11_uninit(ctx->vo);
}

static int glx_init(struct MPGLContext *ctx, int flags)
{
    struct vo *vo = ctx->vo;
    struct priv *p = ctx->priv;

    p->vdp_queue = VDP_INVALID_HANDLE;
    p->vdp_target = VDP_INVALID_HANDLE;

    if (ctx->vo->probing)
        goto uninit;

    if (!vo_x11_init(ctx->vo))
        goto uninit;

    p->vdp = mp_vdpau_create_device_x11(ctx->log, ctx->vo->x11->display, false);
    if (!p->vdp)
        goto uninit;

    if (!vo_x11_create_vo_window(vo, NULL, "vdpauglx"))
        goto uninit;

    if (!create_context_x11(ctx, flags))
        goto uninit;

    if (!(ctx->gl->mpgl_caps & MPGL_CAP_VDPAU))
        goto uninit;

    if (create_vdpau_objects(ctx) < 0)
        goto uninit;

    p->num_surfaces = NUM_SURFACES;
    for (int n = 0; n < p->num_surfaces; n++)
        p->surfaces[n].surface = VDP_INVALID_HANDLE;

    ctx->flip_v = true;

    return 0;

uninit:
    glx_uninit(ctx);
    return -1;
}

static int glx_reconfig(struct MPGLContext *ctx)
{
    vo_x11_config_vo_window(ctx->vo);
    return 0;
}

static int glx_control(struct MPGLContext *ctx, int *events, int request,
                       void *arg)
{
    return vo_x11_control(ctx->vo, events, request, arg);
}

static void glx_start_frame(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;
    GL *gl = ctx->gl;

    struct surface *surface = &p->surfaces[p->current_surface];

    if (surface->surface != VDP_INVALID_HANDLE) {
        VdpTime prev_vsync_time;
        vdp_st = vdp->presentation_queue_block_until_surface_idle(p->vdp_queue,
                                                                  surface->surface,
                                                                  &prev_vsync_time);
        CHECK_VDP_WARNING(ctx, "waiting for surface failed");
    }

    if (surface->w != ctx->vo->dwidth || surface->h != ctx->vo->dheight)
        recreate_vdpau_surface(ctx, surface);


    ctx->main_fb = surface->fbo; // 0 if creating the surface failed

    if (surface->surface != VDP_INVALID_HANDLE) {
        gl->VDPAUMapSurfacesNV(1, &surface->registered);
        surface->mapped = true;
    }
}

static void glx_swap_buffers(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;
    GL *gl = ctx->gl;

    struct surface *surface = &p->surfaces[p->current_surface];
    if (surface->surface == VDP_INVALID_HANDLE)
        return; // surface alloc probably failed before

    if (surface->mapped)
        gl->VDPAUUnmapSurfacesNV(1, &surface->registered);
    surface->mapped = false;

    vdp_st = vdp->presentation_queue_display(p->vdp_queue, surface->surface,
                                             0, 0, 0);
    CHECK_VDP_WARNING(ctx, "trying to present vdp surface");

    p->current_surface = (p->current_surface + 1) % p->num_surfaces;
}

static void glx_wakeup(struct MPGLContext *ctx)
{
    vo_x11_wakeup(ctx->vo);
}

static void glx_wait_events(struct MPGLContext *ctx, int64_t until_time_us)
{
    vo_x11_wait_events(ctx->vo, until_time_us);
}

const struct mpgl_driver mpgl_driver_vdpauglx = {
    .name           = "vdpauglx",
    .priv_size      = sizeof(struct priv),
    .init           = glx_init,
    .reconfig       = glx_reconfig,
    .start_frame    = glx_start_frame,
    .swap_buffers   = glx_swap_buffers,
    .control        = glx_control,
    .wakeup         = glx_wakeup,
    .wait_events    = glx_wait_events,
    .uninit         = glx_uninit,
};
