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

// FreeBSD 10.0-CURRENT lacks the GLX_ARB_create_context extension completely
#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB           0x2092
#define GLX_CONTEXT_FLAGS_ARB                   0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB            0x9126
#ifndef __APPLE__
// These are respectively 0x00000001 and 0x00000002 on OSX
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#endif
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif
// GLX_EXT_create_context_es2_profile
#ifndef GLX_CONTEXT_ES2_PROFILE_BIT_EXT
#define GLX_CONTEXT_ES2_PROFILE_BIT_EXT         0x00000004
#endif

#include "osdep/timer.h"
#include "video/out/x11_common.h"
#include "context.h"
#include "utils.h"

struct priv {
    GL gl;
    XVisualInfo *vinfo;
    GLXContext context;
    GLXFBConfig fbc;

    Bool (*XGetSyncValues)(Display*, GLXDrawable, int64_t*, int64_t*, int64_t*);
    int64_t last_ust;
    int64_t last_msc;
    int64_t last_sbc;
    int64_t last_sbc_mp_time;
    int64_t user_sbc;
    int64_t vsync_duration;
    int64_t last_skipped_vsyncs;
    int64_t last_queue_display_time;
};

static void glx_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

    if (p->vinfo)
        XFree(p->vinfo);
    if (p->context) {
        Display *display = ctx->vo->x11->display;
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, p->context);
    }

    vo_x11_uninit(ctx->vo);
}

static bool create_context_x11_old(struct ra_ctx *ctx, GL *gl)
{
    struct priv *p = ctx->priv;
    Display *display = ctx->vo->x11->display;
    struct vo *vo = ctx->vo;

    if (p->context)
        return true;

    if (!p->vinfo) {
        MP_FATAL(vo, "Can't create a legacy GLX context without X visual\n");
        return false;
    }

    GLXContext new_context = glXCreateContext(display, p->vinfo, NULL, True);
    if (!new_context) {
        MP_FATAL(vo, "Could not create GLX context!\n");
        return false;
    }

    if (!glXMakeCurrent(display, ctx->vo->x11->window, new_context)) {
        MP_FATAL(vo, "Could not set GLX context!\n");
        glXDestroyContext(display, new_context);
        return false;
    }

    const char *glxstr = glXQueryExtensionsString(display, ctx->vo->x11->screen);

    mpgl_load_functions(gl, (void *)glXGetProcAddressARB, glxstr, vo->log);

    p->context = new_context;

    return true;
}

typedef GLXContext (*glXCreateContextAttribsARBProc)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

static bool create_context_x11_gl3(struct ra_ctx *ctx, GL *gl, int gl_version,
                                   bool es)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    if (p->context)
        return true;

    if (!ra_gl_ctx_test_version(ctx, gl_version, es))
        return false;

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
    int profile_mask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

    if (es) {
        profile_mask = GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
        if (!(glxstr && strstr(glxstr, "GLX_EXT_create_context_es2_profile")))
            return false;
    }

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, MPGL_VER_GET_MAJOR(gl_version),
        GLX_CONTEXT_MINOR_VERSION_ARB, MPGL_VER_GET_MINOR(gl_version),
        GLX_CONTEXT_PROFILE_MASK_ARB, profile_mask,
        GLX_CONTEXT_FLAGS_ARB, ctx_flags,
        None
    };
    vo_x11_silence_xlib(1);
    GLXContext context = glXCreateContextAttribsARB(vo->x11->display,
                                                    p->fbc, 0, True,
                                                    context_attribs);
    vo_x11_silence_xlib(-1);
    if (!context)
        return false;

    // set context
    if (!glXMakeCurrent(vo->x11->display, vo->x11->window, context)) {
        MP_FATAL(vo, "Could not set GLX context!\n");
        glXDestroyContext(vo->x11->display, context);
        return false;
    }

    p->context = context;

    mpgl_load_functions(gl, (void *)glXGetProcAddressARB, glxstr, vo->log);

    if (gl_check_extension(glxstr, "GLX_OML_sync_control")) {
        p->XGetSyncValues =
            (void *)glXGetProcAddressARB((const GLubyte *)"glXGetSyncValuesOML");
    }
    if (p->XGetSyncValues)
        MP_VERBOSE(vo, "Using GLX_OML_sync_control.\n");

    return true;
}

// The GL3/FBC initialization code roughly follows/copies from:
//  http://www.opengl.org/wiki/Tutorial:_OpenGL_3.0_Context_Creation_(GLX)
// but also uses some of the old code.

static GLXFBConfig select_fb_config(struct vo *vo, const int *attribs, bool alpha)
{
    int fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(vo->x11->display, vo->x11->screen,
                                         attribs, &fbcount);
    if (!fbc)
        return NULL;

    // The list in fbc is sorted (so that the first element is the best).
    GLXFBConfig fbconfig = fbcount > 0 ? fbc[0] : NULL;

    if (alpha) {
        for (int n = 0; n < fbcount; n++) {
            XVisualInfo *v = glXGetVisualFromFBConfig(vo->x11->display, fbc[n]);
            if (v) {
                bool is_rgba = vo_x11_is_rgba_visual(v);
                XFree(v);
                if (is_rgba) {
                    fbconfig = fbc[n];
                    break;
                }
            }
        }
    }

    XFree(fbc);

    return fbconfig;
}

static void set_glx_attrib(int *attribs, int name, int value)
{
    for (int n = 0; attribs[n * 2 + 0] != None; n++) {
        if (attribs[n * 2 + 0] == name) {
            attribs[n * 2 + 1] = value;
            break;
        }
    }
}

static void update_vsync_oml(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    assert(p->XGetSyncValues);

    p->last_skipped_vsyncs = 0;
    p->user_sbc += 1;

    // This extension returns two unrelated values:
    //  (ust, msc): clock time and incrementing counter of last vsync (this is
    //              reported continuously, even if we don't swap)
    //  sbc:        swap counter of frame that was last displayed (every swap
    //              increments the user_sbc, and the reported sbc is the sbc
    //              of the frame that was just displayed)
    // Invariants:
    //  - ust and msc change in lockstep (no value can change with the other)
    //  - msc is incremented; if you query it in a loop, and your thread isn't
    //    frozen or starved by the scheduler, it will usually not change or
    //    be incremented by 1 (while the ust will be incremented by vsync
    //    duration)
    //  - sbc is never higher than the user_sbc
    //  - (ust, msc) are equal to or higher by vsync increments than the display
    //    time of the frame referenced by the sbc
    // Note that (ust, msc) and sbc are not locked to each other. The following
    // can easily happen if vsync skips occur:
    //  - you draw a frame, in the meantime hardware swaps sbc_1
    //  - another display vsync happens during drawing
    //  - you call swap()
    //  - query (ust, msc) and sbc
    //  - sbc contains sbc_1, but (ust, msc) contains the vsync after it
    // As a consequence, it's hard to detect the latency or vsync skips.
    int64_t ust, msc, sbc;
    if (!p->XGetSyncValues(ctx->vo->x11->display, ctx->vo->x11->window,
                           &ust, &msc, &sbc))
    {
        // Assume the extension is effectively unsupported.
        p->vsync_duration = -1;
        p->last_skipped_vsyncs = -1;
        p->last_queue_display_time = -1;
        return;
    }

    int64_t ust_passed = p->last_ust ? ust - p->last_ust : 0;
    p->last_ust = ust;

    int64_t msc_passed = p->last_msc ? msc - p->last_msc : 0;
    p->last_msc = msc;

    int64_t sbc_passed = sbc - p->last_sbc;
    p->last_sbc = sbc;

    // Display frame duration. This makes assumptions about UST (see below).
    if (msc_passed && ust_passed)
        p->vsync_duration = ust_passed / msc_passed;

    // Only if a new frame was displayed (sbc increased) we have sort-of a
    // chance that the current (ust, msc) is for the sbc. But this is racy,
    // because skipped frames drops could have increased the msc right after the
    // display event and before we queried the values. This code hopes for the
    // best and ignores this.
    if (sbc_passed) {
        // The extension spec doesn't define what the UST is (not even its unit).
        // Simply assume UST is a simple CLOCK_MONOTONIC usec value. The swap
        // buffer call happened "some" but very small time ago, so we can get
        // away with querying the current time. There is also the implicit
        // assumption that mpv's timer and the UST use the same clock (which it
        // does on POSIX).
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts))
            return;
        uint64_t now_monotonic = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
        uint64_t ust_mp_time = mp_time_us() - (now_monotonic - ust);

        // Assume this is exactly when the actual display event for this sbc
        // happened. This is subject to the race mentioned above.
        p->last_sbc_mp_time = ust_mp_time;
    }

    // At least one frame needs to be actually displayed before
    // p->last_sbc_mp_time is set.
    if (!sbc)
        return;

    // Extrapolate from the last sbc time (when a frame was actually displayed),
    // and by adding the number of frames that were queued since to it.
    // For every unit the sbc is smaller than user_sbc, the actual display
    // is one frame ahead (assumes glx_swap_buffers() is called for every
    // vsync).
    p->last_queue_display_time =
        p->last_sbc_mp_time + (p->user_sbc - sbc) * p->vsync_duration;
}

static void glx_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    glXSwapBuffers(ctx->vo->x11->display, ctx->vo->x11->window);

    if (p->XGetSyncValues)
        update_vsync_oml(ctx);
}

static void glx_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct priv *p = ctx->priv;
    info->vsync_duration = p->vsync_duration;
    info->skipped_vsyncs = p->last_skipped_vsyncs;
    info->last_queue_display_time = p->last_queue_display_time;
}

static bool glx_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct vo *vo = ctx->vo;
    GL *gl = &p->gl;

    if (!vo_x11_init(ctx->vo))
        goto uninit;

    int glx_major, glx_minor;

    if (!glXQueryVersion(vo->x11->display, &glx_major, &glx_minor)) {
        MP_ERR(ctx, "GLX not found.\n");
        goto uninit;
    }
    // FBConfigs were added in GLX version 1.3.
    if (MPGL_VER(glx_major, glx_minor) <  MPGL_VER(1, 3)) {
        MP_ERR(ctx, "GLX version older than 1.3.\n");
        goto uninit;
    }

    int glx_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 0,
        GLX_DOUBLEBUFFER, True,
        None
    };
    GLXFBConfig fbc = NULL;
    if (ctx->opts.want_alpha) {
        set_glx_attrib(glx_attribs, GLX_ALPHA_SIZE, 1);
        fbc = select_fb_config(vo, glx_attribs, true);
        if (!fbc)
            set_glx_attrib(glx_attribs, GLX_ALPHA_SIZE, 0);
    }
    if (!fbc)
        fbc = select_fb_config(vo, glx_attribs, false);
    if (!fbc) {
        MP_ERR(ctx, "no GLX support present\n");
        goto uninit;
    }

    int fbid = -1;
    if (!glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_FBCONFIG_ID, &fbid))
        MP_VERBOSE(ctx, "GLX chose FB config with ID 0x%x\n", fbid);

    p->fbc = fbc;
    p->vinfo = glXGetVisualFromFBConfig(vo->x11->display, fbc);
    if (p->vinfo) {
        MP_VERBOSE(ctx, "GLX chose visual with ID 0x%x\n",
                   (int)p->vinfo->visualid);
    } else {
        MP_WARN(ctx, "Selected GLX FB config has no associated X visual\n");
    }

    if (!vo_x11_create_vo_window(vo, p->vinfo, "gl"))
        goto uninit;

    bool success = false;
    for (int n = 0; mpgl_preferred_gl_versions[n]; n++) {
        int version = mpgl_preferred_gl_versions[n];
        MP_VERBOSE(ctx, "Creating OpenGL %d.%d context...\n",
                   MPGL_VER_P(version));
        if (version >= 300) {
            success = create_context_x11_gl3(ctx, gl, version, false);
        } else {
            success = create_context_x11_old(ctx, gl);
        }
        if (success)
            break;
    }
    if (!success) // try again for GLES
        success = create_context_x11_gl3(ctx, gl, 200, true);
    if (success && !glXIsDirect(vo->x11->display, p->context))
        gl->mpgl_caps |= MPGL_CAP_SW;
    if (!success)
        goto uninit;

    struct ra_gl_ctx_params params = {
        .swap_buffers = glx_swap_buffers,
        .get_vsync    = glx_get_vsync,
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        goto uninit;

    p->vsync_duration = -1;
    p->last_skipped_vsyncs = -1;
    p->last_queue_display_time = -1;

    return true;

uninit:
    glx_uninit(ctx);
    return false;
}

static bool glx_init_probe(struct ra_ctx *ctx)
{
    if (!glx_init(ctx))
        return false;

    struct priv *p = ctx->priv;
    if (!(p->gl.mpgl_caps & MPGL_CAP_VDPAU)) {
        MP_VERBOSE(ctx, "No vdpau support found - probing more things.\n");
        glx_uninit(ctx);
        return false;
    }

    return true;
}

static void resize(struct ra_ctx *ctx)
{
    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
}

static bool glx_reconfig(struct ra_ctx *ctx)
{
    vo_x11_config_vo_window(ctx->vo);
    resize(ctx);
    return true;
}

static int glx_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_x11_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

static void glx_wakeup(struct ra_ctx *ctx)
{
    vo_x11_wakeup(ctx->vo);
}

static void glx_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_x11_wait_events(ctx->vo, until_time_us);
}

const struct ra_ctx_fns ra_ctx_glx = {
    .type           = "opengl",
    .name           = "x11",
    .reconfig       = glx_reconfig,
    .control        = glx_control,
    .wakeup         = glx_wakeup,
    .wait_events    = glx_wait_events,
    .init           = glx_init,
    .uninit         = glx_uninit,
};

const struct ra_ctx_fns ra_ctx_glx_probe = {
    .type           = "opengl",
    .name           = "x11probe",
    .reconfig       = glx_reconfig,
    .control        = glx_control,
    .wakeup         = glx_wakeup,
    .wait_events    = glx_wait_events,
    .init           = glx_init_probe,
    .uninit         = glx_uninit,
};
