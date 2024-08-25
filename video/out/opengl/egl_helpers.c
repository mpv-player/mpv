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

#include "config.h"
#include "options/m_config.h"

#if HAVE_LIBDL
#include <dlfcn.h>
#endif

#include "common/common.h"

#include "egl_helpers.h"
#include "common.h"
#include "utils.h"
#include "context.h"

#if HAVE_EGL_ANGLE
// On Windows, egl_helpers.c is only used by ANGLE, where the EGL functions may
// be loaded dynamically from ANGLE DLLs
#include "angle_dynamic.h"
#endif

// EGL 1.5
#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK
#define EGL_CONTEXT_MAJOR_VERSION               0x3098
#define EGL_CONTEXT_MINOR_VERSION               0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK         0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT     0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE   0x31B1
typedef intptr_t EGLAttrib;
#endif

// Not every EGL provider (like RPI) has these.
#ifndef EGL_CONTEXT_FLAGS_KHR
#define EGL_CONTEXT_FLAGS_KHR EGL_NONE
#endif

#ifndef EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0
#endif

#ifndef EGL_COLOR_COMPONENT_TYPE_EXT
#define EGL_COLOR_COMPONENT_TYPE_EXT EGL_NONE
#define EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT EGL_NONE
#define EGL_COLOR_COMPONENT_TYPE_FIXED_EXT EGL_NONE
#endif

struct mp_egl_config_attr {
    int attrib;
    const char *name;
};

#define MP_EGL_ATTRIB(id) {id, # id}

static const struct mp_egl_config_attr mp_egl_attribs[] = {
    MP_EGL_ATTRIB(EGL_CONFIG_ID),
    MP_EGL_ATTRIB(EGL_RED_SIZE),
    MP_EGL_ATTRIB(EGL_GREEN_SIZE),
    MP_EGL_ATTRIB(EGL_BLUE_SIZE),
    MP_EGL_ATTRIB(EGL_ALPHA_SIZE),
    MP_EGL_ATTRIB(EGL_COLOR_BUFFER_TYPE),
    MP_EGL_ATTRIB(EGL_CONFIG_CAVEAT),
    MP_EGL_ATTRIB(EGL_CONFORMANT),
    MP_EGL_ATTRIB(EGL_NATIVE_VISUAL_ID),
};

static void dump_egl_config(struct mp_log *log, int msgl, EGLDisplay display,
                            EGLConfig config)
{
    for (int n = 0; n < MP_ARRAY_SIZE(mp_egl_attribs); n++) {
        const char *name = mp_egl_attribs[n].name;
        EGLint v = -1;
        if (eglGetConfigAttrib(display, config, mp_egl_attribs[n].attrib, &v)) {
            mp_msg(log, msgl, "  %s=0x%x\n", name, v);
        } else {
            mp_msg(log, msgl, "  %s=<error>\n", name);
        }
    }
}

static void *mpegl_get_proc_address(mp_unused void *ctx, const char *name)
{
    void *p = eglGetProcAddress(name);
#if defined(__GLIBC__) && HAVE_LIBDL
    // Some crappy ARM/Linux things do not provide EGL 1.5, so above call does
    // not necessarily return function pointers for core functions. Try to get
    // them from a loaded GLES lib. As POSIX leaves RTLD_DEFAULT "reserved",
    // use it only with glibc.
    if (!p)
        p = dlsym(RTLD_DEFAULT, name);
#endif
    return p;
}

struct egl_opts {
    int config_id;
    int output_format;
};

#define RGBA_FORMAT(r, g, b, a) ((r) << 18 | (g) << 12 | (b) << 6 | (a))
#define FLOAT_FORMAT (1 << 24)
static void unpack_format(int format, EGLint *r_size, EGLint *g_size, EGLint *b_size, EGLint *a_size, bool *is_float) {
    *is_float = format & FLOAT_FORMAT;
    *r_size = format >> 18 & 0x3f;
    *g_size = (format >> 12) & 0x3f;
    *b_size = (format >> 6) & 0x3f;
    *a_size = format & 0x3f;
}

#define OPT_BASE_STRUCT struct egl_opts
const struct m_sub_options egl_conf = {
    .opts = (const struct m_option[]) {
        {"config-id", OPT_INT(config_id)},
        {"output-format", OPT_CHOICE(output_format,
            {"auto", 0},
            {"rgb8", RGBA_FORMAT(8, 8, 8, 0)},
            {"rgba8", RGBA_FORMAT(8, 8, 8, 8)},
            {"rgb10", RGBA_FORMAT(10, 10, 10, 0)},
            {"rgb10_a2", RGBA_FORMAT(10, 10, 10, 2)},
            {"rgb16", RGBA_FORMAT(16, 16, 16, 0)},
            {"rgba16", RGBA_FORMAT(16, 16, 16, 16)},
            {"rgb16f", RGBA_FORMAT(16, 16, 16, 0) | FLOAT_FORMAT},
            {"rgba16f", RGBA_FORMAT(16, 16, 16, 16) | FLOAT_FORMAT},
            {"rgb32f", RGBA_FORMAT(32, 32, 32, 0) | FLOAT_FORMAT},
            {"rgba32f", RGBA_FORMAT(32, 32, 32, 32) | FLOAT_FORMAT})},
        {0},
    },
    .size = sizeof(struct egl_opts),
};

static bool create_context(struct ra_ctx *ctx, EGLDisplay display,
                           bool es, struct mpegl_cb cb, struct egl_opts *opts,
                           EGLContext *out_context, EGLConfig *out_config)
{
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_FATAL;

    EGLenum api;
    EGLint rend;
    const char *name;

    if (!es) {
        api = EGL_OPENGL_API;
        rend = EGL_OPENGL_BIT;
        name = "Desktop OpenGL";
    } else {
        api = EGL_OPENGL_ES_API;
        rend = EGL_OPENGL_ES2_BIT;
        name = "GLES 2.x +";
    }

    const char *egl_exts = eglQueryString(display, EGL_EXTENSIONS);

    MP_VERBOSE(ctx, "Trying to create %s context.\n", name);

    if (!eglBindAPI(api)) {
        MP_VERBOSE(ctx, "Could not bind API!\n");
        return false;
    }

    bool request_float_fmt;
    EGLint r_size, g_size, b_size, a_size;
    unpack_format(opts->output_format, &r_size, &g_size, &b_size, &a_size, &request_float_fmt);
    bool has_float_format_ext = gl_check_extension(egl_exts, "EGL_EXT_pixel_format_float");
    if (request_float_fmt && !has_float_format_ext) {
        MP_MSG(ctx, msgl, "Could not request floating point pixel format for %s!\n", name);
        return false;
    }

    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, rend,
        EGL_RED_SIZE, MPMAX(r_size, 8),
        EGL_GREEN_SIZE, MPMAX(g_size, 8),
        EGL_BLUE_SIZE, MPMAX(b_size, 8),
        EGL_ALPHA_SIZE, opts->output_format ? a_size : (ctx->opts.want_alpha ? 8 : 0),
        opts->output_format && has_float_format_ext ? EGL_COLOR_COMPONENT_TYPE_EXT : EGL_NONE,
        request_float_fmt ? EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT : EGL_COLOR_COMPONENT_TYPE_FIXED_EXT,
        EGL_NONE
    };
    if (opts->config_id) {
        // Keep EGL_SURFACE_TYPE & EGL_RENDERABLE_TYPE
        attributes[4] = EGL_CONFIG_ID;
        attributes[5] = opts->config_id;
        attributes[6] = EGL_NONE;
    }

    EGLint num_configs;
    if (!eglChooseConfig(display, attributes, NULL, 0, &num_configs))
        num_configs = 0;

    EGLConfig *configs = talloc_array(NULL, EGLConfig, num_configs);
    if (!eglChooseConfig(display, attributes, configs, num_configs, &num_configs))
        num_configs = 0;

    if (!num_configs) {
        talloc_free(configs);
        MP_MSG(ctx, msgl, "Could not choose EGLConfig for %s!\n", name);
        return false;
    }

    for (int n = 0; n < num_configs; n++)
        dump_egl_config(ctx->log, MSGL_TRACE, display, configs[n]);

    int chosen = 0;
    if (opts->output_format) {
        for (; chosen < num_configs; chosen++) {
            EGLint real_r_size, real_g_size, real_b_size, real_a_size;
            eglGetConfigAttrib(display, configs[chosen], EGL_RED_SIZE, &real_r_size);
            eglGetConfigAttrib(display, configs[chosen], EGL_GREEN_SIZE, &real_g_size);
            eglGetConfigAttrib(display, configs[chosen], EGL_BLUE_SIZE, &real_b_size);
            eglGetConfigAttrib(display, configs[chosen], EGL_ALPHA_SIZE, &real_a_size);
            if (r_size == real_r_size && g_size == real_g_size && b_size == real_b_size && a_size == real_a_size)
                break;
        }
    } else if (cb.refine_config) {
        chosen = cb.refine_config(cb.user_data, configs, num_configs);
    }
    if (chosen < 0 || chosen == num_configs) {
        talloc_free(configs);
        MP_MSG(ctx, msgl, "Could not refine EGLConfig for %s!\n", name);
        return false;
    }
    EGLConfig config = configs[chosen];

    talloc_free(configs);

    MP_DBG(ctx, "Chosen EGLConfig:\n");
    dump_egl_config(ctx->log, MSGL_DEBUG, display, config);

    int ctx_flags = ctx->opts.debug ? EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0;
    EGLContext *egl_ctx = NULL;

    if (!es) {
        for (int n = 0; mpgl_min_required_gl_versions[n]; n++) {
            int ver = mpgl_min_required_gl_versions[n];

            EGLint attrs[] = {
                EGL_CONTEXT_MAJOR_VERSION, MPGL_VER_GET_MAJOR(ver),
                EGL_CONTEXT_MINOR_VERSION, MPGL_VER_GET_MINOR(ver),
                EGL_CONTEXT_OPENGL_PROFILE_MASK,
                    ver >= 320 ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT : 0,
                EGL_CONTEXT_FLAGS_KHR, ctx_flags,
                EGL_NONE
            };

            egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
            if (egl_ctx)
                break;
        }
    }
    if (!egl_ctx) {
        // Fallback for EGL 1.4 without EGL_KHR_create_context or GLES
        EGLint attrs[] = {
            EGL_CONTEXT_FLAGS_KHR, ctx_flags,
            es ? EGL_CONTEXT_CLIENT_VERSION : EGL_NONE, 2,
            EGL_NONE
        };

        egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
        if (!egl_ctx)
            egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, &attrs[2]);
    }

    if (!egl_ctx) {
        MP_MSG(ctx, msgl, "Could not create EGL context for %s (error=%d)!\n",
               name, eglGetError());
        return false;
    }

    *out_context = egl_ctx;
    *out_config = config;
    return true;
}

#define STR_OR_ERR(s) ((s) ? (s) : "(error)")

// Create a context and return it and the config it was created with. If it
// returns false, the out_* pointers are set to NULL.
// vo_flags is a combination of VOFLAG_* values.
bool mpegl_create_context(struct ra_ctx *ctx, EGLDisplay display,
                          EGLContext *out_context, EGLConfig *out_config)
{
    return mpegl_create_context_cb(ctx, display, (struct mpegl_cb){0},
                                   out_context, out_config);
}

// Create a context and return it and the config it was created with. If it
// returns false, the out_* pointers are set to NULL.
bool mpegl_create_context_cb(struct ra_ctx *ctx, EGLDisplay display,
                             struct mpegl_cb cb, EGLContext *out_context,
                             EGLConfig *out_config)
{
    *out_context = NULL;
    *out_config = NULL;

    const char *version = eglQueryString(display, EGL_VERSION);
    const char *vendor = eglQueryString(display, EGL_VENDOR);
    const char *apis = eglQueryString(display, EGL_CLIENT_APIS);
    MP_VERBOSE(ctx, "EGL_VERSION=%s\nEGL_VENDOR=%s\nEGL_CLIENT_APIS=%s\n",
               STR_OR_ERR(version), STR_OR_ERR(vendor), STR_OR_ERR(apis));

    enum gles_mode mode = ra_gl_ctx_get_glesmode(ctx);

    void *tmp = talloc_new(NULL);
    struct egl_opts *opts = mp_get_config_group(tmp, ctx->global, &egl_conf);

    bool r = false;
    if (!r && (mode == GLES_NO || mode == GLES_AUTO))
        r = create_context(ctx, display, false, cb, opts, out_context, out_config);
    if (!r && (mode == GLES_YES || mode == GLES_AUTO))
        r = create_context(ctx, display, true, cb, opts, out_context, out_config);

    talloc_free(tmp);

    if (!r) {
        int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;
        MP_MSG(ctx, msgl, "Could not create a GL context.\n");
    }

    return r;
}

static int GLAPIENTRY swap_interval(int interval)
{
    EGLDisplay display = eglGetCurrentDisplay();
    if (!display)
        return 1;
    return !eglSwapInterval(display, interval);
}

// Load gl version and function pointers into *gl.
// Expects a current EGL context set.
void mpegl_load_functions(struct GL *gl, struct mp_log *log)
{
    const char *egl_exts = "";
    EGLDisplay display = eglGetCurrentDisplay();
    if (display != EGL_NO_DISPLAY)
        egl_exts = eglQueryString(display, EGL_EXTENSIONS);

    mpgl_load_functions2(gl, mpegl_get_proc_address, NULL, egl_exts, log);
    if (!gl->SwapInterval)
        gl->SwapInterval = swap_interval;
}

static bool is_egl15(void)
{
    // It appears that EGL 1.4 is specified to _require_ an initialized display
    // for EGL_VERSION, while EGL 1.5 is _required_ to return the EGL version.
    const char *ver = eglQueryString(EGL_NO_DISPLAY, EGL_VERSION);
    // Of course we have to go through the excruciating pain of parsing a
    // version string, since EGL provides no other way without a display. In
    // theory version!=NULL is already proof enough that it's 1.5, but be
    // extra defensive, since this should have been true for EGL_EXTENSIONS as
    // well, but then they added an extension that modified standard behavior.
    int ma = 0, mi = 0;
    return ver && sscanf(ver, "%d.%d", &ma, &mi) == 2 && (ma > 1 || mi >= 5);
}

// This is similar to eglGetPlatformDisplay(platform, native_display, NULL),
// except that it 1. may use eglGetPlatformDisplayEXT, 2. checks for the
// platform client extension platform_ext_name, and 3. does not support passing
// an attrib list, because the type for that parameter is different in the EXT
// and standard functions (EGL can't not fuck up, no matter what).
//  platform: e.g. EGL_PLATFORM_X11_KHR
//  platform_ext_name: e.g. "EGL_KHR_platform_x11"
//  native_display: e.g. X11 Display*
// Returns EGL_NO_DISPLAY on failure.
// Warning: the EGL version can be different at runtime depending on the chosen
// platform, so this might return a display corresponding to some older EGL
// version (often 1.4).
// Often, there are two extension variants of a platform (KHR and EXT). If you
// need to check both, call this function twice. (Why do they define them twice?
// They're crazy.)
EGLDisplay mpegl_get_display(EGLenum platform, const char *platform_ext_name,
                             void *native_display)
{
    // EGL is awful. Designed as ultra-portable library, it fails at dealing
    // with slightly more complex environment than its short-sighted design
    // could deal with. So they invented an awful, awful kludge that modifies
    // EGL standard behavior, the EGL_EXT_client_extensions extension. EGL 1.4
    // normally is to return NULL when querying EGL_EXTENSIONS on EGL_NO_DISPLAY,
    // however, with that extension, it'll return the set of "client extensions",
    // which may include EGL_EXT_platform_base.

    // Prerequisite: check the platform extension.
    // If this is either EGL 1.5, or 1.4 with EGL_EXT_client_extensions, then
    // this must return a valid extension string.
    const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!gl_check_extension(exts, platform_ext_name))
        return EGL_NO_DISPLAY;

    // Before we go through the EGL 1.4 BS, try if we can use native EGL 1.5
    if (is_egl15()) {
        // This is EGL 1.5. It must support querying standard functions through
        // eglGetProcAddress(). Note that on EGL 1.4, even if the function is
        // unknown, it could return non-NULL anyway (because EGL is crazy).
        EGLDisplay (EGLAPIENTRYP GetPlatformDisplay)
            (EGLenum, void *, const EGLAttrib *) =
            (void *)eglGetProcAddress("eglGetPlatformDisplay");
        // (It should be impossible to be NULL, but uh.)
        if (GetPlatformDisplay)
            return GetPlatformDisplay(platform, native_display, NULL);
    }

    if (!gl_check_extension(exts, "EGL_EXT_platform_base"))
        return EGL_NO_DISPLAY;

    EGLDisplay (EGLAPIENTRYP GetPlatformDisplayEXT)(EGLenum, void*, const EGLint*)
        = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");

    // (It should be impossible to be NULL, but uh.)
    if (GetPlatformDisplayEXT)
        return GetPlatformDisplayEXT(platform, native_display, NULL);

    return EGL_NO_DISPLAY;
}

// The same mess but with eglCreatePlatformWindowSurface(EXT)
// again no support for an attribute list because the type differs
// Returns EGL_NO_SURFACE on failure.
EGLSurface mpegl_create_window_surface(EGLDisplay dpy, EGLConfig config,
                                       void *native_window)
{
    // Use the EGL 1.5 function if possible
    if (is_egl15()) {
        EGLSurface (EGLAPIENTRYP CreatePlatformWindowSurface)
            (EGLDisplay, EGLConfig, void *, const EGLAttrib *) =
            (void *)eglGetProcAddress("eglCreatePlatformWindowSurface");
        // (It should be impossible to be NULL, but uh.)
        if (CreatePlatformWindowSurface)
            return CreatePlatformWindowSurface(dpy, config, native_window, NULL);
    }

    // Check the extension that provides the *EXT function
    const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!gl_check_extension(exts, "EGL_EXT_platform_base"))
        return EGL_NO_SURFACE;

    EGLSurface (EGLAPIENTRYP CreatePlatformWindowSurfaceEXT)
        (EGLDisplay, EGLConfig, void *, const EGLint *) =
        (void *)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
    // (It should be impossible to be NULL, but uh.)
    if (CreatePlatformWindowSurfaceEXT)
        return CreatePlatformWindowSurfaceEXT(dpy, config, native_window, NULL);

    return EGL_NO_SURFACE;
}
