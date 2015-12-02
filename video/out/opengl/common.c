/*
 * common OpenGL routines
 *
 * copyleft (C) 2005-2010 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 * Special thanks go to the xine team and Matthias Hopf, whose video_out_opengl.c
 * gave me lots of good ideas.
 *
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

/**
 * \file gl_common.c
 * \brief OpenGL helper functions used by vo_gl.c and vo_gl2.c
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "talloc.h"
#include "common.h"
#include "common/common.h"
#include "options/options.h"
#include "options/m_option.h"

// This guesses if the current GL context is a suspected software renderer.
static bool is_software_gl(GL *gl)
{
    const char *renderer = gl->GetString(GL_RENDERER);
    const char *vendor = gl->GetString(GL_VENDOR);
    return !(renderer && vendor) ||
           strcmp(renderer, "Software Rasterizer") == 0 ||
           strstr(renderer, "llvmpipe") ||
           strcmp(vendor, "Microsoft Corporation") == 0 ||
           strcmp(renderer, "Mesa X11") == 0;
}

static void GLAPIENTRY dummy_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    assert(framebuffer == 0);
}

static bool check_ext(GL *gl, const char *name)
{
    const char *exts = gl->extensions;
    char *s = strstr(exts, name);
    char *e = s ? s + strlen(name) : NULL;
    return s && (s == exts || s[-1] == ' ') && (e[0] == ' ' || !e[0]);
}

#define FN_OFFS(name) offsetof(GL, name)

#define DEF_FN(name)            {FN_OFFS(name), "gl" # name}
#define DEF_FN_NAME(name, str)  {FN_OFFS(name), str}

struct gl_function {
    ptrdiff_t offset;
    char *name;
};

struct gl_functions {
    const char *extension;      // introduced with this extension in any version
    int provides;               // bitfield of MPGL_CAP_* constants
    int ver_core;               // introduced as required function
    int ver_es_core;            // introduced as required GL ES function
    const struct gl_function *functions;
};

#define MAX_FN_COUNT 100        // max functions per gl_functions section

// Note: to keep the number of sections low, some functions are in multiple
//       sections (if there are tricky combinations of GL/ES versions)
static const struct gl_functions gl_functions[] = {
    // GL 2.1+ desktop and GLES 2.0+ (anything we support)
    // Probably all of these are in GL 2.0 too, but we require GLSL 120.
    {
        .ver_core = 210,
        .ver_es_core = 200,
        .functions = (const struct gl_function[]) {
            DEF_FN(ActiveTexture),
            DEF_FN(AttachShader),
            DEF_FN(BindAttribLocation),
            DEF_FN(BindBuffer),
            DEF_FN(BindTexture),
            DEF_FN(BlendFuncSeparate),
            DEF_FN(BufferData),
            DEF_FN(Clear),
            DEF_FN(ClearColor),
            DEF_FN(CompileShader),
            DEF_FN(CreateProgram),
            DEF_FN(CreateShader),
            DEF_FN(DeleteBuffers),
            DEF_FN(DeleteProgram),
            DEF_FN(DeleteShader),
            DEF_FN(DeleteTextures),
            DEF_FN(Disable),
            DEF_FN(DisableVertexAttribArray),
            DEF_FN(DrawArrays),
            DEF_FN(Enable),
            DEF_FN(EnableVertexAttribArray),
            DEF_FN(Finish),
            DEF_FN(Flush),
            DEF_FN(GenBuffers),
            DEF_FN(GenTextures),
            DEF_FN(GetAttribLocation),
            DEF_FN(GetError),
            DEF_FN(GetIntegerv),
            DEF_FN(GetProgramInfoLog),
            DEF_FN(GetProgramiv),
            DEF_FN(GetShaderInfoLog),
            DEF_FN(GetShaderiv),
            DEF_FN(GetString),
            DEF_FN(GetUniformLocation),
            DEF_FN(LinkProgram),
            DEF_FN(PixelStorei),
            DEF_FN(ReadPixels),
            DEF_FN(ShaderSource),
            DEF_FN(TexImage2D),
            DEF_FN(TexParameteri),
            DEF_FN(TexSubImage2D),
            DEF_FN(Uniform1f),
            DEF_FN(Uniform2f),
            DEF_FN(Uniform3f),
            DEF_FN(Uniform1i),
            DEF_FN(UniformMatrix2fv),
            DEF_FN(UniformMatrix3fv),
            DEF_FN(UseProgram),
            DEF_FN(VertexAttribPointer),
            DEF_FN(Viewport),
            {0}
        },
    },
    // GL 2.1+ desktop only (and GLSL 120 shaders)
    {
        .ver_core = 210,
        .provides = MPGL_CAP_ROW_LENGTH | MPGL_CAP_1D_TEX,
        .functions = (const struct gl_function[]) {
            DEF_FN(DrawBuffer),
            DEF_FN(GetTexLevelParameteriv),
            DEF_FN(MapBuffer),
            DEF_FN(ReadBuffer),
            DEF_FN(TexImage1D),
            DEF_FN(UnmapBuffer),
            {0}
        },
    },
    // GL 3.0+ and ES 3.x core only functions.
    {
        .ver_core = 300,
        .ver_es_core = 300,
        .functions = (const struct gl_function[]) {
            DEF_FN(BindBufferBase),
            DEF_FN(BlitFramebuffer),
            DEF_FN(GetStringi),
            // for ES 3.0
            DEF_FN(ReadBuffer),
            DEF_FN(UnmapBuffer),
            {0}
        },
    },
    // For ES 3.1 core
    {
        .ver_es_core = 310,
        .functions = (const struct gl_function[]) {
            DEF_FN(GetTexLevelParameteriv),
            {0}
        },
    },
    {
        .ver_core = 210,
        .ver_es_core = 300,
        .provides = MPGL_CAP_3D_TEX,
        .functions = (const struct gl_function[]) {
            DEF_FN(TexImage3D),
            {0}
        },
    },
    // Useful for ES 2.0
    {
        .ver_core = 110,
        .ver_es_core = 300,
        .extension = "GL_EXT_unpack_subimage",
        .provides = MPGL_CAP_ROW_LENGTH,
    },
    // Framebuffers, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .ver_es_core = 200,
        .extension = "GL_ARB_framebuffer_object",
        .provides = MPGL_CAP_FB,
        .functions = (const struct gl_function[]) {
            DEF_FN(BindFramebuffer),
            DEF_FN(GenFramebuffers),
            DEF_FN(DeleteFramebuffers),
            DEF_FN(CheckFramebufferStatus),
            DEF_FN(FramebufferTexture2D),
            {0}
        },
    },
    // VAOs, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .ver_es_core = 300,
        .extension = "GL_ARB_vertex_array_object",
        .provides = MPGL_CAP_VAO,
        .functions = (const struct gl_function[]) {
            DEF_FN(GenVertexArrays),
            DEF_FN(BindVertexArray),
            DEF_FN(DeleteVertexArrays),
            {0}
        }
    },
    // GL_RED / GL_RG textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .ver_es_core = 300,
        .extension = "GL_ARB_texture_rg",
        .provides = MPGL_CAP_TEX_RG,
    },
    {
        .ver_core = 320,
        .extension = "GL_ARB_sync",
        .functions = (const struct gl_function[]) {
            DEF_FN(FenceSync),
            DEF_FN(ClientWaitSync),
            DEF_FN(DeleteSync),
            {0}
        },
    },
    // Swap control, always an OS specific extension
    // The OSX code loads this manually.
    {
        .extension = "GLX_SGI_swap_control",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(SwapInterval, "glXSwapIntervalSGI"),
            {0},
        },
    },
    {
        .extension = "WGL_EXT_swap_control",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(SwapInterval, "wglSwapIntervalEXT"),
            {0},
        },
    },
    {
        .extension = "GLX_SGI_video_sync",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(GetVideoSync, "glXGetVideoSyncSGI"),
            DEF_FN_NAME(WaitVideoSync, "glXWaitVideoSyncSGI"),
            {0},
        },
    },
    // For gl_hwdec_vdpau.c
    // http://www.opengl.org/registry/specs/NV/vdpau_interop.txt
    {
        .extension = "GL_NV_vdpau_interop",
        .provides = MPGL_CAP_VDPAU,
        .functions = (const struct gl_function[]) {
            // (only functions needed by us)
            DEF_FN(VDPAUInitNV),
            DEF_FN(VDPAUFiniNV),
            DEF_FN(VDPAURegisterOutputSurfaceNV),
            DEF_FN(VDPAUUnregisterSurfaceNV),
            DEF_FN(VDPAUSurfaceAccessNV),
            DEF_FN(VDPAUMapSurfacesNV),
            DEF_FN(VDPAUUnmapSurfacesNV),
            {0}
        },
    },
    // Apple Packed YUV Formats
    // For gl_hwdec_vda.c
    // http://www.opengl.org/registry/specs/APPLE/rgb_422.txt
    {
        .extension = "GL_APPLE_rgb_422",
        .provides = MPGL_CAP_APPLE_RGB_422,
    },
    {
        .ver_core = 430,
        .extension = "GL_ARB_debug_output",
        .provides = MPGL_CAP_DEBUG,
        .functions = (const struct gl_function[]) {
            // (only functions needed by us)
            DEF_FN(DebugMessageCallback),
            {0}
        },
    },
    // These don't exist - they are for the sake of mpv internals, and libmpv
    // interaction (see libmpv/opengl_cb.h).
    {
        .extension = "GL_MP_MPGetNativeDisplay",
        .functions = (const struct gl_function[]) {
            DEF_FN(MPGetNativeDisplay),
            {0}
        },
    },
    // Same, but using the old name.
    {
        .extension = "GL_MP_D3D_interfaces",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(MPGetNativeDisplay, "glMPGetD3DInterface"),
            {0}
        },
    },
    // uniform buffer object extensions, requires OpenGL 3.1.
    {
        .ver_core = 310,
        .ver_es_core = 300,
        .extension = "GL_ARB_uniform_buffer_object",
        .functions = (const struct gl_function[]) {
            DEF_FN(GetUniformBlockIndex),
            DEF_FN(UniformBlockBinding),
            {0}
        },
    },
};

#undef FN_OFFS
#undef DEF_FN_HARD
#undef DEF_FN
#undef DEF_FN_NAME


// Fill the GL struct with function pointers and extensions from the current
// GL context. Called by the backend.
// getProcAddress: function to resolve function names, may be NULL
// ext2: an extra extension string
// log: used to output messages
// Note: if you create a CONTEXT_FORWARD_COMPATIBLE_BIT_ARB with OpenGL 3.0,
//       you must append "GL_ARB_compatibility" to ext2.
void mpgl_load_functions2(GL *gl, void *(*get_fn)(void *ctx, const char *n),
                          void *fn_ctx, const char *ext2, struct mp_log *log)
{
    talloc_free_children(gl);
    *gl = (GL) {
        .extensions = talloc_strdup(gl, ext2 ? ext2 : ""),
    };

    gl->GetString = get_fn(fn_ctx, "glGetString");
    if (!gl->GetString) {
        mp_err(log, "Can't load OpenGL functions.\n");
        goto error;
    }

    int major = 0, minor = 0;
    const char *version_string = gl->GetString(GL_VERSION);
    if (!version_string)
        goto error;
    mp_verbose(log, "GL_VERSION='%s'\n",  version_string);
    if (strncmp(version_string, "OpenGL ES ", 10) == 0) {
        version_string += 10;
        gl->es = 100;
    }
    if (sscanf(version_string, "%d.%d", &major, &minor) < 2)
        goto error;
    gl->version = MPGL_VER(major, minor);
    mp_verbose(log, "Detected %s %d.%d.\n", gl->es ? "GLES" : "desktop OpenGL",
               major, minor);

    if (gl->es) {
        gl->es = gl->version;
        gl->version = 0;
        if (gl->es < 200) {
            mp_fatal(log, "At least GLESv2 required.\n");
            goto error;
        }
    }

    mp_verbose(log, "GL_VENDOR='%s'\n",   gl->GetString(GL_VENDOR));
    mp_verbose(log, "GL_RENDERER='%s'\n", gl->GetString(GL_RENDERER));
    const char *shader = gl->GetString(GL_SHADING_LANGUAGE_VERSION);
    if (shader)
        mp_verbose(log, "GL_SHADING_LANGUAGE_VERSION='%s'\n", shader);

    if (gl->version >= 300) {
        gl->GetStringi = get_fn(fn_ctx, "glGetStringi");
        gl->GetIntegerv = get_fn(fn_ctx, "glGetIntegerv");

        if (!(gl->GetStringi && gl->GetIntegerv))
            goto error;

        GLint exts;
        gl->GetIntegerv(GL_NUM_EXTENSIONS, &exts);
        for (int n = 0; n < exts; n++) {
            const char *ext = gl->GetStringi(GL_EXTENSIONS, n);
            gl->extensions = talloc_asprintf_append(gl->extensions, " %s", ext);
        }

    } else {
        const char *ext = (char*)gl->GetString(GL_EXTENSIONS);
        gl->extensions = talloc_asprintf_append(gl->extensions, " %s", ext);
    }

    mp_dbg(log, "Combined OpenGL extensions string:\n%s\n", gl->extensions);

    for (int n = 0; n < MP_ARRAY_SIZE(gl_functions); n++) {
        const struct gl_functions *section = &gl_functions[n];
        int version = gl->es ? gl->es : gl->version;
        int ver_core = gl->es ? section->ver_es_core : section->ver_core;

        // NOTE: Function entrypoints can exist, even if they do not work.
        //       We must always check extension strings and versions.

        bool exists = false, must_exist = false;
        if (ver_core)
            must_exist = version >= ver_core;

        if (section->extension && check_ext(gl, section->extension))
            exists = true;

        exists |= must_exist;
        if (!exists)
            continue;

        void *loaded[MAX_FN_COUNT] = {0};
        bool all_loaded = true;
        const struct gl_function *fnlist = section->functions;

        for (int i = 0; fnlist && fnlist[i].name; i++) {
            const struct gl_function *fn = &fnlist[i];
            void *ptr = get_fn(fn_ctx, fn->name);
            if (!ptr) {
                all_loaded = false;
                mp_warn(log, "Required function '%s' not "
                        "found for %s OpenGL %d.%d.\n", fn->name,
                        section->extension ? section->extension : "builtin",
                        MPGL_VER_GET_MAJOR(ver_core),
                        MPGL_VER_GET_MINOR(ver_core));
                if (must_exist)
                    goto error;
                break;
            }
            assert(i < MAX_FN_COUNT);
            loaded[i] = ptr;
        }

        if (all_loaded) {
            gl->mpgl_caps |= section->provides;
            for (int i = 0; fnlist && fnlist[i].name; i++) {
                const struct gl_function *fn = &fnlist[i];
                void **funcptr = (void**)(((char*)gl) + fn->offset);
                if (loaded[i])
                    *funcptr = loaded[i];
            }
            mp_verbose(log, "Loaded functions for %d/%s.\n", ver_core,
                       section->extension ? section->extension : "builtin");
        }
    }

    gl->glsl_version = 0;
    if (gl->es) {
        if (gl->es >= 200)
            gl->glsl_version = 100;
        if (gl->es >= 300)
            gl->glsl_version = 300;
    } else {
        gl->glsl_version = 110;
        int glsl_major = 0, glsl_minor = 0;
        if (shader && sscanf(shader, "%d.%d", &glsl_major, &glsl_minor) == 2)
            gl->glsl_version = glsl_major * 100 + glsl_minor;
        // GLSL 400 defines "sample" as keyword - breaks custom shaders.
        gl->glsl_version = MPMIN(gl->glsl_version, 330);
    }

    if (is_software_gl(gl)) {
        gl->mpgl_caps |= MPGL_CAP_SW;
        mp_verbose(log, "Detected suspected software renderer.\n");
    }

    // Detect 16F textures that work with GL_LINEAR filtering.
    if ((!gl->es && (gl->version >= 300 || check_ext(gl, "GL_ARB_texture_float"))) ||
        (gl->es && (gl->version >= 310 || check_ext(gl, "GL_OES_texture_half_float_linear"))))
    {
        mp_verbose(log, "Filterable half-float textures supported.\n");
        gl->mpgl_caps |= MPGL_CAP_FLOAT_TEX;
    }

    // Provided for simpler handling if no framebuffer support is available.
    if (!gl->BindFramebuffer)
        gl->BindFramebuffer = &dummy_glBindFramebuffer;
    return;

error:
    gl->version = 0;
    gl->es = 0;
    gl->mpgl_caps = 0;
}

static void *get_procaddr_wrapper(void *ctx, const char *name)
{
    void *(*getProcAddress)(const GLubyte *) = ctx;
    return getProcAddress ? getProcAddress((const GLubyte*)name) : NULL;
}

void mpgl_load_functions(GL *gl, void *(*getProcAddress)(const GLubyte *),
                         const char *ext2, struct mp_log *log)
{
    mpgl_load_functions2(gl, get_procaddr_wrapper, getProcAddress, ext2, log);
}

extern const struct mpgl_driver mpgl_driver_x11;
extern const struct mpgl_driver mpgl_driver_x11egl;
extern const struct mpgl_driver mpgl_driver_x11_probe;
extern const struct mpgl_driver mpgl_driver_drm_egl;
extern const struct mpgl_driver mpgl_driver_cocoa;
extern const struct mpgl_driver mpgl_driver_wayland;
extern const struct mpgl_driver mpgl_driver_w32;
extern const struct mpgl_driver mpgl_driver_angle;
extern const struct mpgl_driver mpgl_driver_rpi;

static const struct mpgl_driver *const backends[] = {
#if HAVE_RPI
    &mpgl_driver_rpi,
#endif
#if HAVE_GL_COCOA
    &mpgl_driver_cocoa,
#endif
#if HAVE_EGL_ANGLE
    &mpgl_driver_angle,
#endif
#if HAVE_GL_WIN32
    &mpgl_driver_w32,
#endif
#if HAVE_GL_WAYLAND
    &mpgl_driver_wayland,
#endif
#if HAVE_GL_X11
    &mpgl_driver_x11_probe,
#endif
#if HAVE_EGL_X11
    &mpgl_driver_x11egl,
#endif
#if HAVE_GL_X11
    &mpgl_driver_x11,
#endif
#if HAVE_EGL_DRM
    &mpgl_driver_drm_egl,
#endif
};

int mpgl_find_backend(const char *name)
{
    if (name == NULL || strcmp(name, "auto") == 0)
        return -1;
    for (int n = 0; n < MP_ARRAY_SIZE(backends); n++) {
        if (strcmp(backends[n]->name, name) == 0)
            return n;
    }
    return -2;
}

int mpgl_validate_backend_opt(struct mp_log *log, const struct m_option *opt,
                              struct bstr name, struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        mp_info(log, "OpenGL windowing backends:\n");
        mp_info(log, "    auto (autodetect)\n");
        for (int n = 0; n < MP_ARRAY_SIZE(backends); n++)
            mp_info(log, "    %s\n", backends[n]->name);
        return M_OPT_EXIT - 1;
    }
    char s[20];
    snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
    return mpgl_find_backend(s) >= -1 ? 1 : M_OPT_INVALID;
}

#if HAVE_C11_TLS
static _Thread_local MPGLContext *current_context;

static void * GLAPIENTRY get_native_display(const char *name)
{
    if (current_context && current_context->native_display_type &&
        name && strcmp(current_context->native_display_type, name) == 0)
        return current_context->native_display;
    return NULL;
}

static void set_current_context(MPGLContext *context)
{
    current_context = context;
    if (context && !context->gl->MPGetNativeDisplay)
        context->gl->MPGetNativeDisplay = get_native_display;
}
#else
static void set_current_context(MPGLContext *context)
{
}
#endif

static MPGLContext *init_backend(struct vo *vo, const struct mpgl_driver *driver,
                                 bool probing, int vo_flags)
{
    MPGLContext *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (MPGLContext) {
        .gl = talloc_zero(ctx, GL),
        .vo = vo,
        .driver = driver,
    };
    bool old_probing = vo->probing;
    vo->probing = probing; // hack; kill it once backends are separate
    MP_VERBOSE(vo, "Initializing OpenGL backend '%s'\n", ctx->driver->name);
    ctx->priv = talloc_zero_size(ctx, ctx->driver->priv_size);
    if (ctx->driver->init(ctx, vo_flags) < 0) {
        vo->probing = old_probing;
        talloc_free(ctx);
        return NULL;
    }
    vo->probing = old_probing;

    if (!ctx->gl->version && !ctx->gl->es)
        goto cleanup;

    if (probing && ctx->gl->es && (vo_flags & VOFLAG_NO_GLES)) {
        MP_VERBOSE(ctx->vo, "Skipping GLES backend.\n");
        goto cleanup;
    }

    if (ctx->gl->mpgl_caps & MPGL_CAP_SW) {
        MP_WARN(ctx->vo, "Suspected software renderer or indirect context.\n");
        if (vo->probing && !(vo_flags & VOFLAG_SW))
            goto cleanup;
    }

    ctx->gl->debug_context = !!(vo_flags & VOFLAG_GL_DEBUG);

    set_current_context(ctx);

    return ctx;

cleanup:
    mpgl_uninit(ctx);
    return NULL;
}

// Create a VO window and create a GL context on it.
//  vo_flags: passed to the backend's create window function
MPGLContext *mpgl_init(struct vo *vo, const char *backend_name, int vo_flags)
{
    MPGLContext *ctx = NULL;
    int index = mpgl_find_backend(backend_name);
    if (index == -1) {
        for (int n = 0; n < MP_ARRAY_SIZE(backends); n++) {
            ctx = init_backend(vo, backends[n], true, vo_flags);
            if (ctx)
                break;
        }
        // VO forced, but no backend is ok => force the first that works at all
        if (!ctx && !vo->probing) {
            for (int n = 0; n < MP_ARRAY_SIZE(backends); n++) {
                ctx = init_backend(vo, backends[n], false, vo_flags);
                if (ctx)
                    break;
            }
        }
    } else if (index >= 0) {
        ctx = init_backend(vo, backends[index], false, vo_flags);
    }
    return ctx;
}

int mpgl_reconfig_window(struct MPGLContext *ctx)
{
    return ctx->driver->reconfig(ctx);
}

int mpgl_control(struct MPGLContext *ctx, int *events, int request, void *arg)
{
    return ctx->driver->control(ctx, events, request, arg);
}

void mpgl_swap_buffers(struct MPGLContext *ctx)
{
    ctx->driver->swap_buffers(ctx);
}

void mpgl_uninit(MPGLContext *ctx)
{
    set_current_context(NULL);
    if (ctx)
        ctx->driver->uninit(ctx);
    talloc_free(ctx);
}
