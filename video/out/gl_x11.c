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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <X11/Xlib.h>
#include <GL/glx.h>

#define MP_GET_GLX_WORKAROUNDS
#include "gl_header_fixes.h"

#include "x11_common.h"
#include "gl_common.h"

struct glx_context {
    XVisualInfo *vinfo;
    GLXContext context;
    GLXFBConfig fbc;
    bool force_es;
};

static bool create_context_x11_old(struct MPGLContext *ctx)
{
    struct glx_context *glx_ctx = ctx->priv;
    Display *display = ctx->vo->x11->display;
    struct vo *vo = ctx->vo;
    GL *gl = ctx->gl;

    if (glx_ctx->context)
        return true;

    if (!glx_ctx->vinfo) {
        MP_FATAL(vo, "Can't create a legacy GLX context without X visual\n");
        return false;
    }

    GLXContext new_context = glXCreateContext(display, glx_ctx->vinfo, NULL,
                                              True);
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

    glx_ctx->context = new_context;

    return true;
}

typedef GLXContext (*glXCreateContextAttribsARBProc)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

static bool create_context_x11_gl3(struct MPGLContext *ctx, int vo_flags,
                                   int gl_version, bool es)
{
    struct glx_context *glx_ctx = ctx->priv;
    struct vo *vo = ctx->vo;

    if (glx_ctx->context)
        return true;

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
    GLXContext context = glXCreateContextAttribsARB(vo->x11->display,
                                                    glx_ctx->fbc, 0, True,
                                                    context_attribs);
    if (!context) {
        MP_INFO(vo, "Could not create GL3 context. Retrying with legacy context.\n");
        return false;
    }

    // set context
    if (!glXMakeCurrent(vo->x11->display, vo->x11->window, context)) {
        MP_FATAL(vo, "Could not set GLX context!\n");
        glXDestroyContext(vo->x11->display, context);
        return false;
    }

    glx_ctx->context = context;

    mpgl_load_functions(ctx->gl, (void *)glXGetProcAddressARB, glxstr, vo->log);

    return true;
}

// The GL3/FBC initialization code roughly follows/copies from:
//  http://www.opengl.org/wiki/Tutorial:_OpenGL_3.0_Context_Creation_(GLX)
// but also uses some of the old code.

static GLXFBConfig select_fb_config(struct vo *vo, const int *attribs, int flags)
{
    int fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(vo->x11->display, vo->x11->screen,
                                         attribs, &fbcount);
    if (!fbc)
        return NULL;

    // The list in fbc is sorted (so that the first element is the best).
    GLXFBConfig fbconfig = fbcount > 0 ? fbc[0] : NULL;

    if (flags & VOFLAG_ALPHA) {
        for (int n = 0; n < fbcount; n++) {
            XVisualInfo *v = glXGetVisualFromFBConfig(vo->x11->display, fbc[n]);
            if (!v)
                continue;
            // This is a heuristic at best. Note that normal 8 bit Visuals use
            // a depth of 24, even if the pixels are padded to 32 bit. If the
            // depth is higher than 24, the remaining bits must be alpha.
            // Note: vinfo->bits_per_rgb appears to be useless (is always 8).
            unsigned long mask = v->depth == 32 ?
                (unsigned long)-1 : (1 << (unsigned long)v->depth) - 1;
            if (mask & ~(v->red_mask | v->green_mask | v->blue_mask)) {
                fbconfig = fbc[n];
                break;
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

static bool config_window_x11(struct MPGLContext *ctx, int flags)
{
    struct vo *vo = ctx->vo;
    struct glx_context *glx_ctx = ctx->priv;

    if (glx_ctx->context) {
        // GL context and window already exist.
        // Only update window geometry etc.
        vo_x11_config_vo_window(vo, glx_ctx->vinfo, flags, "gl");
        return true;
    }

    int glx_major, glx_minor;

    if (!glXQueryVersion(vo->x11->display, &glx_major, &glx_minor)) {
        MP_ERR(vo, "GLX not found.\n");
        return false;
    }
    // FBConfigs were added in GLX version 1.3.
    if (MPGL_VER(glx_major, glx_minor) <  MPGL_VER(1, 3)) {
        MP_ERR(vo, "GLX version older than 1.3.\n");
        return false;
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
    if (flags & VOFLAG_ALPHA) {
        set_glx_attrib(glx_attribs, GLX_ALPHA_SIZE, 1);
        fbc = select_fb_config(vo, glx_attribs, flags);
        if (!fbc) {
            set_glx_attrib(glx_attribs, GLX_ALPHA_SIZE, 0);
            flags &= ~VOFLAG_ALPHA;
        }
    }
    if (!fbc)
        fbc = select_fb_config(vo, glx_attribs, flags);
    if (!fbc) {
        MP_ERR(vo, "no GLX support present\n");
        return false;
    }
    MP_VERBOSE(vo, "GLX chose FB config with ID 0x%x\n", (int)(intptr_t)fbc);

    glx_ctx->fbc = fbc;
    glx_ctx->vinfo = glXGetVisualFromFBConfig(vo->x11->display, fbc);
    if (glx_ctx->vinfo) {
        MP_VERBOSE(vo, "GLX chose visual with ID 0x%x\n",
                   (int)glx_ctx->vinfo->visualid);
    } else {
        MP_WARN(vo, "Selected GLX FB config has no associated X visual\n");
    }


    glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_RED_SIZE, &ctx->depth_r);
    glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_GREEN_SIZE, &ctx->depth_g);
    glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_BLUE_SIZE, &ctx->depth_b);

    vo_x11_config_vo_window(vo, glx_ctx->vinfo, flags, "gl");

    int gl_version = ctx->requested_gl_version;
    bool success = false;
    if (!glx_ctx->force_es) {
        success = create_context_x11_gl3(ctx, flags, gl_version, false);
        if (!success)
            success = create_context_x11_old(ctx);
    }
    if (!success) // try ES
        success = create_context_x11_gl3(ctx, flags, 200, true);
    if (success && !glXIsDirect(vo->x11->display, glx_ctx->context))
        ctx->gl->mpgl_caps |= MPGL_CAP_SW;
    return success;
}

static void releaseGlContext_x11(MPGLContext *ctx)
{
    struct glx_context *glx_ctx = ctx->priv;
    XVisualInfo **vinfo = &glx_ctx->vinfo;
    GLXContext *context = &glx_ctx->context;
    Display *display = ctx->vo->x11->display;
    if (*vinfo)
        XFree(*vinfo);
    *vinfo = NULL;
    if (*context) {
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, *context);
    }
    *context = 0;
}

static void swapGlBuffers_x11(MPGLContext *ctx)
{
    glXSwapBuffers(ctx->vo->x11->display, ctx->vo->x11->window);
}

void mpgl_set_backend_x11(MPGLContext *ctx)
{
    ctx->priv = talloc_zero(ctx, struct glx_context);
    ctx->config_window = config_window_x11;
    ctx->releaseGlContext = releaseGlContext_x11;
    ctx->swapGlBuffers = swapGlBuffers_x11;
    ctx->vo_init = vo_x11_init;
    ctx->vo_uninit = vo_x11_uninit;
    ctx->vo_control = vo_x11_control;
}

void mpgl_set_backend_x11es(MPGLContext *ctx)
{
    mpgl_set_backend_x11(ctx);
    struct glx_context *glx_ctx = ctx->priv;
    glx_ctx->force_es = true;
}
