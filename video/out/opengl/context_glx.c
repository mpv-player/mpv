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

#include "video/out/x11_common.h"
#include "context.h"
#include "utils.h"

struct priv {
    GL gl;
    XVisualInfo *vinfo;
    GLXContext context;
    GLXFBConfig fbc;
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

static void glx_swap_buffers(struct ra_ctx *ctx)
{
    glXSwapBuffers(ctx->vo->x11->display, ctx->vo->x11->window);
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
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        goto uninit;

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
