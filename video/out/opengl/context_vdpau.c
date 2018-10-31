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
    GL gl;
    GLXContext context;
    struct mp_vdpau_ctx *vdp;
    VdpPresentationQueueTarget vdp_target;
    VdpPresentationQueue vdp_queue;
    struct surface *surfaces;
    int num_surfaces;
    int idx_surfaces;
};

typedef GLXContext (*glXCreateContextAttribsARBProc)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

static bool create_context_x11(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    int glx_major, glx_minor;
    if (!glXQueryVersion(vo->x11->display, &glx_major, &glx_minor)) {
        MP_ERR(vo, "GLX not found.\n");
        return false;
    }

    if (!ra_gl_ctx_test_version(ctx, MPGL_VER(glx_major, glx_minor), false))
        return false;

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

    int ctx_flags = ctx->opts.debug ? GLX_CONTEXT_DEBUG_BIT_ARB : 0;
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

    p->context = context;
    mpgl_load_functions(&p->gl, (void *)glXGetProcAddressARB, glxstr, vo->log);
    return true;
}

static int create_vdpau_objects(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct GL *gl = &p->gl;
    VdpDevice dev = p->vdp->vdp_device;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;

    gl->VDPAUInitNV(BRAINDEATH(dev), p->vdp->get_proc_address);

    vdp_st = vdp->presentation_queue_target_create_x11(dev, ctx->vo->x11->window,
                                                       &p->vdp_target);
    CHECK_VDP_ERROR(ctx, "creating vdp target");

    vdp_st = vdp->presentation_queue_create(dev, p->vdp_target, &p->vdp_queue);
    CHECK_VDP_ERROR(ctx, "creating vdp presentation queue");

    return 0;
}

static void destroy_vdpau_surface(struct ra_ctx *ctx,
                                  struct surface *surface)
{
    struct priv *p = ctx->priv;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;
    GL *gl = &p->gl;

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

static bool recreate_vdpau_surface(struct ra_ctx *ctx,
                                   struct surface *surface)
{
    struct priv *p = ctx->priv;
    VdpDevice dev = p->vdp->vdp_device;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;
    GL *gl = &p->gl;

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

    return true;

error:
    destroy_vdpau_surface(ctx, surface);
    return false;
}

static void vdpau_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;

    // This is the *next* surface we will be rendering to. By delaying the
    // block_until_idle, we're essentially allowing p->num_surfaces - 1
    // in-flight surfaces, plus the one currently visible surface.
    struct surface *surf = &p->surfaces[p->idx_surfaces];
    if (surf->surface == VDP_INVALID_HANDLE)
        return;

    VdpTime prev_vsync_time;
    vdp_st = vdp->presentation_queue_block_until_surface_idle(p->vdp_queue,
                                                              surf->surface,
                                                              &prev_vsync_time);
    CHECK_VDP_WARNING(ctx, "waiting for surface failed");
}

static void vdpau_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

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

static const struct ra_swapchain_fns vdpau_swapchain;

static bool vdpau_init(struct ra_ctx *ctx)
{
    struct vo *vo = ctx->vo;
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);

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

    if (!create_context_x11(ctx))
        goto uninit;

    if (!(p->gl.mpgl_caps & MPGL_CAP_VDPAU))
        goto uninit;

    if (create_vdpau_objects(ctx) < 0)
        goto uninit;

    p->num_surfaces = ctx->opts.swapchain_depth + 1; // +1 for the visible image
    p->surfaces = talloc_zero_array(p, struct surface, p->num_surfaces);
    for (int n = 0; n < p->num_surfaces; n++)
        p->surfaces[n].surface = VDP_INVALID_HANDLE;

    struct ra_gl_ctx_params params = {
        .swap_buffers = vdpau_swap_buffers,
        .external_swapchain = &vdpau_swapchain,
        .disable_vsync_fences = true,
        .flipped = true,
    };

    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        goto uninit;

    return true;

uninit:
    vdpau_uninit(ctx);
    return false;
}

static bool vdpau_start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->ctx->priv;
    struct vo *vo = sw->ctx->vo;
    GL *gl = &p->gl;

    struct surface *surf = &p->surfaces[p->idx_surfaces];
    if (surf->w != vo->dwidth || surf->h != vo->dheight ||
        surf->surface == VDP_INVALID_HANDLE)
    {
        if (!recreate_vdpau_surface(sw->ctx, surf))
            return NULL;
    }

    assert(!surf->mapped);
    gl->VDPAUMapSurfacesNV(1, &surf->registered);
    surf->mapped = true;

    ra_gl_ctx_resize(sw, surf->w, surf->h, surf->fbo);
    return ra_gl_ctx_start_frame(sw, out_fbo);
}

static bool vdpau_submit_frame(struct ra_swapchain *sw,
                               const struct vo_frame *frame)
{
    struct priv *p = sw->ctx->priv;
    GL *gl = &p->gl;
    struct vdp_functions *vdp = &p->vdp->vdp;
    VdpStatus vdp_st;

    struct surface *surf = &p->surfaces[p->idx_surfaces];
    assert(surf->surface != VDP_INVALID_HANDLE);
    assert(surf->mapped);
    gl->VDPAUUnmapSurfacesNV(1, &surf->registered);
    surf->mapped = false;

    vdp_st = vdp->presentation_queue_display(p->vdp_queue, surf->surface, 0, 0, 0);
    CHECK_VDP_WARNING(sw->ctx, "trying to present vdp surface");

    p->idx_surfaces = (p->idx_surfaces + 1) % p->num_surfaces;
    return ra_gl_ctx_submit_frame(sw, frame) && vdp_st == VDP_STATUS_OK;
}

static bool vdpau_reconfig(struct ra_ctx *ctx)
{
    vo_x11_config_vo_window(ctx->vo);
    return true;
}

static int vdpau_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    return vo_x11_control(ctx->vo, events, request, arg);
}

static void vdpau_wakeup(struct ra_ctx *ctx)
{
    vo_x11_wakeup(ctx->vo);
}

static void vdpau_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_x11_wait_events(ctx->vo, until_time_us);
}

static const struct ra_swapchain_fns vdpau_swapchain = {
    .start_frame   = vdpau_start_frame,
    .submit_frame  = vdpau_submit_frame,
};

const struct ra_ctx_fns ra_ctx_vdpauglx = {
    .type           = "opengl",
    .name           = "vdpauglx",
    .reconfig       = vdpau_reconfig,
    .control        = vdpau_control,
    .wakeup         = vdpau_wakeup,
    .wait_events    = vdpau_wait_events,
    .init           = vdpau_init,
    .uninit         = vdpau_uninit,
};
