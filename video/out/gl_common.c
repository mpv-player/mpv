/*
 * common OpenGL routines
 *
 * copyleft (C) 2005-2010 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 * Special thanks go to the xine team and Matthias Hopf, whose video_out_opengl.c
 * gave me lots of good ideas.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include "gl_common.h"
#include "common/common.h"
#include "options/options.h"
#include "options/m_option.h"

//! \defgroup glgeneral OpenGL general helper functions

// GLU has this as gluErrorString (we don't use GLU, as it is legacy-OpenGL)
static const char *gl_error_to_string(GLenum error)
{
    switch (error) {
    case GL_INVALID_ENUM: return "INVALID_ENUM";
    case GL_INVALID_VALUE: return "INVALID_VALUE";
    case GL_INVALID_OPERATION: return "INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
    default: return "unknown";
    }
}

void glCheckError(GL *gl, struct mp_log *log, const char *info)
{
    for (;;) {
        GLenum error = gl->GetError();
        if (error == GL_NO_ERROR)
            break;
        mp_msg(log, MSGL_ERR, "%s: OpenGL error %s.\n", info,
               gl_error_to_string(error));
    }
}

static int get_alignment(int stride)
{
    if (stride % 8 == 0)
        return 8;
    if (stride % 4 == 0)
        return 4;
    if (stride % 2 == 0)
        return 2;
    return 1;
}

struct feature {
    int id;
    const char *name;
};

static const struct feature features[] = {
    {MPGL_CAP_FB,               "Framebuffers"},
    {MPGL_CAP_VAO,              "VAOs"},
    {MPGL_CAP_SRGB_TEX,         "sRGB textures"},
    {MPGL_CAP_SRGB_FB,          "sRGB framebuffers"},
    {MPGL_CAP_FLOAT_TEX,        "Float textures"},
    {MPGL_CAP_TEX_RG,           "RG textures"},
    {MPGL_CAP_1ST_CLASS_ARRAYS, "1st class shader arrays"},
    {MPGL_CAP_1D_TEX,           "1D textures"},
    {MPGL_CAP_3D_TEX,           "3D textures"},
    {MPGL_CAP_DEBUG,            "debugging extensions"},
    {MPGL_CAP_SW,               "suspected software renderer"},
    {0},
};

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

#define FN_OFFS(name) offsetof(GL, name)

#define DEF_FN(name)            {FN_OFFS(name), {"gl" # name}}
#define DEF_FN_NAMES(name, ...) {FN_OFFS(name), {__VA_ARGS__}}

struct gl_function {
    ptrdiff_t offset;
    char *funcnames[7];
};

struct gl_functions {
    const char *extension;      // introduced with this extension in any version
    int provides;               // bitfield of MPGL_CAP_* constants
    int ver_core;               // introduced as required function
    int ver_removed;            // removed as required function (no replacement)
    int ver_es_core;            // introduced as required GL ES function
    int ver_es_removed;         // removed as required function (no replacement)
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
            DEF_FN(GetBooleanv),
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
        .provides = MPGL_CAP_ROW_LENGTH | MPGL_CAP_1D_TEX | MPGL_CAP_3D_TEX |
                    MPGL_CAP_1ST_CLASS_ARRAYS,
        .functions = (const struct gl_function[]) {
            DEF_FN(DrawBuffer),
            DEF_FN(GetTexLevelParameteriv),
            DEF_FN(MapBuffer),
            DEF_FN(ReadBuffer),
            DEF_FN(TexImage1D),
            DEF_FN(TexImage3D),
            DEF_FN(UnmapBuffer),
            {0}
        },
    },
    // GL 3.0+ and ES 3.x core only functions.
    {
        .ver_core = 300,
        .ver_es_core = 300,
        .provides = MPGL_CAP_SRGB_TEX | MPGL_CAP_SRGB_FB | MPGL_CAP_3D_TEX |
                    MPGL_CAP_1ST_CLASS_ARRAYS,
        .functions = (const struct gl_function[]) {
            DEF_FN(GetStringi),
            // for ES 3.0
            DEF_FN(GetTexLevelParameteriv),
            DEF_FN(ReadBuffer),
            DEF_FN(UnmapBuffer),
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
        .ver_es_core = 300,
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
    // Framebuffers, alternative extension name.
    {
        .ver_removed = 300, // don't touch these fn names in 3.x
        .ver_es_removed = 300,
        .extension = "GL_EXT_framebuffer_object",
        .provides = MPGL_CAP_FB,
        .functions = (const struct gl_function[]) {
            DEF_FN_NAMES(BindFramebuffer, "glBindFramebufferEXT"),
            DEF_FN_NAMES(GenFramebuffers, "glGenFramebuffersEXT"),
            DEF_FN_NAMES(DeleteFramebuffers, "glDeleteFramebuffersEXT"),
            DEF_FN_NAMES(CheckFramebufferStatus, "glCheckFramebufferStatusEXT"),
            DEF_FN_NAMES(FramebufferTexture2D, "glFramebufferTexture2DEXT"),
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
    // sRGB textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .extension = "GL_EXT_texture_sRGB",
        .provides = MPGL_CAP_SRGB_TEX,
    },
    // sRGB framebuffers, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .extension = "GL_EXT_framebuffer_sRGB",
        .provides = MPGL_CAP_SRGB_FB,
    },
    // Float textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .ver_es_core = 300,
        .extension = "GL_ARB_texture_float",
        .provides = MPGL_CAP_FLOAT_TEX,
    },
    // GL_RED / GL_RG textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = 300,
        .ver_es_core = 300,
        .extension = "GL_ARB_texture_rg",
        .provides = MPGL_CAP_TEX_RG,
    },
    // Swap control, always an OS specific extension
    {
        .extension = "_swap_control",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAMES(SwapInterval, "glXSwapIntervalSGI", "glXSwapInterval",
                         "wglSwapIntervalSGI", "wglSwapInterval",
                         "wglSwapIntervalEXT"),
            {0}
        },
    },
    {
        .extension = "GLX_SGI_video_sync",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAMES(GetVideoSync, "glXGetVideoSyncSGI"),
            DEF_FN_NAMES(WaitVideoSync, "glXWaitVideoSyncSGI"),
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
};

#undef FN_OFFS
#undef DEF_FN_HARD
#undef DEF_FN
#undef DEF_FN_NAMES


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
    if (strncmp(version_string, "OpenGL ES ", 10) == 0) {
        version_string += 10;
        gl->es = 100;
    }
    sscanf(version_string, "%d.%d", &major, &minor);
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
    mp_verbose(log, "GL_VERSION='%s'\n",  gl->GetString(GL_VERSION));
    const char *shader = gl->GetString(GL_SHADING_LANGUAGE_VERSION);
    if (shader)
        mp_verbose(log, "GL_SHADING_LANGUAGE_VERSION='%s'\n", shader);

    // Note: This code doesn't handle CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
    //       on OpenGL 3.0 correctly. Apparently there's no way to detect this
    //       situation, because GL_ARB_compatibility is specified only for 3.1
    //       and above.

    bool has_legacy = false;
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
            if (strcmp(ext, "GL_ARB_compatibility") == 0)
                has_legacy = true;
        }

        // This version doesn't have GL_ARB_compatibility yet, and always
        // includes legacy (except with CONTEXT_FORWARD_COMPATIBLE_BIT_ARB).
        if (gl->version == 300)
            has_legacy = true;
    } else {
        const char *ext = (char*)gl->GetString(GL_EXTENSIONS);
        gl->extensions = talloc_asprintf_append(gl->extensions, " %s", ext);

        has_legacy = true;
    }

    if (gl->es)
        has_legacy = false;

    if (has_legacy)
        mp_verbose(log, "OpenGL legacy compat. found.\n");
    mp_dbg(log, "Combined OpenGL extensions string:\n%s\n", gl->extensions);

    for (int n = 0; n < sizeof(gl_functions) / sizeof(gl_functions[0]); n++) {
        const struct gl_functions *section = &gl_functions[n];
        int version = gl->es ? gl->es : gl->version;
        int ver_core = gl->es ? section->ver_es_core : section->ver_core;
        int ver_removed = gl->es ? section->ver_es_removed : section->ver_removed;

        // With has_legacy, the legacy functions are still available, and
        // functions are never actually removed. (E.g. the context could be at
        // version >= 3.0, but functions like glBegin still exist and work.)
        if (!has_legacy && ver_removed && version >= ver_removed)
            continue;

        // NOTE: Function entrypoints can exist, even if they do not work.
        //       We must always check extension strings and versions.

        bool exists = false, must_exist = false;
        if (ver_core)
            must_exist = version >= ver_core;

        if (section->extension && strstr(gl->extensions, section->extension))
            exists = true;

        exists |= must_exist;
        if (!exists)
            continue;

        void *loaded[MAX_FN_COUNT] = {0};
        bool all_loaded = true;
        const struct gl_function *fnlist = section->functions;

        for (int i = 0; fnlist && fnlist[i].funcnames[0]; i++) {
            const struct gl_function *fn = &fnlist[i];
            void *ptr = NULL;
            for (int x = 0; fn->funcnames[x]; x++) {
                ptr = get_fn(fn_ctx, fn->funcnames[x]);
                if (ptr)
                    break;
            }
            if (!ptr) {
                all_loaded = false;
                mp_warn(log, "Required function '%s' not "
                        "found for %s OpenGL %d.%d.\n", fn->funcnames[0],
                        section->extension ? section->extension : "builtin",
                        MPGL_VER_GET_MAJOR(ver_core),
                        MPGL_VER_GET_MINOR(ver_core));
                if (must_exist) {
                    gl->mpgl_caps = 0;
                    goto error;
                }
                break;
            }
            assert(i < MAX_FN_COUNT);
            loaded[i] = ptr;
        }

        if (all_loaded) {
            gl->mpgl_caps |= section->provides;
            for (int i = 0; fnlist && fnlist[i].funcnames[0]; i++) {
                const struct gl_function *fn = &fnlist[i];
                void **funcptr = (void**)(((char*)gl) + fn->offset);
                if (loaded[i])
                    *funcptr = loaded[i];
            }
        }
    }

    gl->glsl_version = 0;
    if (gl->es) {
        if (gl->es >= 200)
            gl->glsl_version = 100;
        if (gl->es >= 300)
            gl->glsl_version = 300;
    } else {
        if (gl->version >= 200)
            gl->glsl_version = 110;
        if (gl->version >= 210)
            gl->glsl_version = 120;
        if (gl->version >= 300)
            gl->glsl_version = 130;
        // Specifically needed for OSX (normally we request 3.0 contexts only, but
        // OSX always creates 3.2 contexts when requesting a core context).
        if (gl->version >= 320)
            gl->glsl_version = 150;
    }

    if (is_software_gl(gl))
        gl->mpgl_caps |= MPGL_CAP_SW;

    if (gl->mpgl_caps) {
        mp_verbose(log, "Detected OpenGL features:\n");
        for (const struct feature *f = &features[0]; f->id; f++) {
            if ((f->id & gl->mpgl_caps))
                mp_verbose(log, "  - %s\n", f->name);
        }
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

/**
 * \brief return the number of bytes per pixel for the given format
 * \param format OpenGL format
 * \param type OpenGL type
 * \return bytes per pixel
 * \ingroup glgeneral
 *
 * Does not handle all possible variants, just those used by MPlayer
 */
int glFmt2bpp(GLenum format, GLenum type)
{
    int component_size = 0;
    switch (type) {
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
        return 1;
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
        return 2;
    case GL_UNSIGNED_BYTE:
        component_size = 1;
        break;
    case GL_UNSIGNED_SHORT:
        component_size = 2;
        break;
    }
    switch (format) {
    case GL_LUMINANCE:
    case GL_ALPHA:
        return component_size;
    case GL_RGB_422_APPLE:
        return 2;
    case GL_RGB:
    case GL_BGR:
    case GL_RGB_INTEGER:
        return 3 * component_size;
    case GL_RGBA:
    case GL_BGRA:
    case GL_RGBA_INTEGER:
        return 4 * component_size;
    case GL_RED:
    case GL_RED_INTEGER:
        return component_size;
    case GL_RG:
    case GL_LUMINANCE_ALPHA:
    case GL_RG_INTEGER:
        return 2 * component_size;
    }
    abort(); // unknown
}

/**
 * \brief upload a texture, handling things like stride and slices
 * \param target texture target, usually GL_TEXTURE_2D
 * \param format OpenGL format of data
 * \param type OpenGL type of data
 * \param dataptr data to upload
 * \param stride data stride
 * \param x x offset in texture
 * \param y y offset in texture
 * \param w width of the texture part to upload
 * \param h height of the texture part to upload
 * \param slice height of an upload slice, 0 for all at once
 * \ingroup gltexture
 */
void glUploadTex(GL *gl, GLenum target, GLenum format, GLenum type,
                 const void *dataptr, int stride,
                 int x, int y, int w, int h, int slice)
{
    const uint8_t *data = dataptr;
    int y_max = y + h;
    if (w <= 0 || h <= 0)
        return;
    if (slice <= 0)
        slice = h;
    if (stride < 0) {
        data += (h - 1) * stride;
        stride = -stride;
    }
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(stride));
    bool use_rowlength = slice > 1 && (gl->mpgl_caps & MPGL_CAP_ROW_LENGTH);
    if (use_rowlength) {
        // this is not always correct, but should work for MPlayer
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, stride / glFmt2bpp(format, type));
    } else {
        if (stride != glFmt2bpp(format, type) * w)
            slice = 1; // very inefficient, but at least it works
    }
    for (; y + slice <= y_max; y += slice) {
        gl->TexSubImage2D(target, 0, x, y, w, slice, format, type, data);
        data += stride * slice;
    }
    if (y < y_max)
        gl->TexSubImage2D(target, 0, x, y, w, y_max - y, format, type, data);
    if (use_rowlength)
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

// Like glUploadTex, but upload a byte array with all elements set to val.
// If scratch is not NULL, points to a resizeable talloc memory block than can
// be freely used by the function (for avoiding temporary memory allocations).
void glClearTex(GL *gl, GLenum target, GLenum format, GLenum type,
                int x, int y, int w, int h, uint8_t val, void **scratch)
{
    int bpp = glFmt2bpp(format, type);
    int stride = w * bpp;
    int size = h * stride;
    if (size < 1)
        return;
    void *data = scratch ? *scratch : NULL;
    if (talloc_get_size(data) < size)
        data = talloc_realloc(NULL, data, char *, size);
    memset(data, val, size);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(stride));
    gl->TexSubImage2D(target, 0, x, y, w, h, format, type, data);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (scratch) {
        *scratch = data;
    } else {
        talloc_free(data);
    }
}

mp_image_t *glGetWindowScreenshot(GL *gl)
{
    if (gl->es)
        return NULL; // ES can't read from front buffer
    GLint vp[4]; //x, y, w, h
    gl->GetIntegerv(GL_VIEWPORT, vp);
    mp_image_t *image = mp_image_alloc(IMGFMT_RGB24, vp[2], vp[3]);
    if (!image)
        return NULL;
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->ReadBuffer(GL_FRONT);
    //flip image while reading (and also avoid stride-related trouble)
    for (int y = 0; y < vp[3]; y++) {
        gl->ReadPixels(vp[0], vp[1] + vp[3] - y - 1, vp[2], 1,
                       GL_RGB, GL_UNSIGNED_BYTE,
                       image->planes[0] + y * image->stride[0]);
    }
    gl->PixelStorei(GL_PACK_ALIGNMENT, 4);
    return image;
}

typedef void (*MPGLSetBackendFn)(MPGLContext *ctx);

struct backend {
    const char *name;
    MPGLSetBackendFn init;
};

static const struct backend backends[] = {
#if HAVE_GL_COCOA
    {"cocoa", mpgl_set_backend_cocoa},
#endif
#if HAVE_GL_WIN32
    {"win", mpgl_set_backend_w32},
#endif

//Add the wayland backend before x11, in order to probe for a wayland-server before a x11-server and avoid using xwayland
#if HAVE_GL_WAYLAND
    {"wayland", mpgl_set_backend_wayland},
#endif
#if HAVE_GL_X11
    {"x11", mpgl_set_backend_x11},
    {"x11es", mpgl_set_backend_x11es},
#endif
#if HAVE_EGL_X11
    {"x11egl", mpgl_set_backend_x11egl},
    {"x11egles", mpgl_set_backend_x11egles},
#endif
    {0}
};

int mpgl_find_backend(const char *name)
{
    if (name == NULL || strcmp(name, "auto") == 0)
        return -1;
    for (const struct backend *entry = backends; entry->name; entry++) {
        if (strcmp(entry->name, name) == 0)
            return entry - backends;
    }
    return -2;
}

int mpgl_validate_backend_opt(struct mp_log *log, const struct m_option *opt,
                              struct bstr name, struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        mp_info(log, "OpenGL windowing backends:\n");
        mp_info(log, "    auto (autodetect)\n");
        for (const struct backend *entry = backends; entry->name; entry++)
            mp_info(log, "    %s\n", entry->name);
        return M_OPT_EXIT - 1;
    }
    char s[20];
    snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
    return mpgl_find_backend(s) >= -1 ? 1 : M_OPT_INVALID;
}

static MPGLContext *init_backend(struct vo *vo, MPGLSetBackendFn set_backend,
                                 bool probing)
{
    MPGLContext *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (MPGLContext) {
        .gl = talloc_zero(ctx, GL),
        .vo = vo,
    };
    bool old_probing = vo->probing;
    vo->probing = probing; // hack; kill it once backends are separate
    set_backend(ctx);
    if (!ctx->vo_init(vo)) {
        talloc_free(ctx);
        ctx = NULL;
    }
    vo->probing = old_probing;
    return ctx;
}

static MPGLContext *mpgl_create(struct vo *vo, const char *backend_name)
{
    MPGLContext *ctx = NULL;
    int index = mpgl_find_backend(backend_name);
    if (index == -1) {
        for (const struct backend *entry = backends; entry->name; entry++) {
            ctx = init_backend(vo, entry->init, true);
            if (ctx)
                break;
        }
    } else if (index >= 0) {
        ctx = init_backend(vo, backends[index].init, false);
    }
    return ctx;
}

MPGLContext *mpgl_init(struct vo *vo, const char *backend_name, int vo_flags)
{
    MPGLContext *ctx = mpgl_create(vo, backend_name);
    if (!ctx)
        goto cleanup;

    ctx->requested_gl_version = 300;

    if (!ctx->config_window(ctx, vo_flags | VOFLAG_HIDDEN))
        goto cleanup;

    if (!ctx->gl->version && !ctx->gl->es)
        goto cleanup;

    if (ctx->gl->es && vo->probing) {
        MP_INFO(ctx->vo, "Skipping experimental GLES support (use --vo=opengl).\n");
        goto cleanup;
    }

    if (ctx->gl->mpgl_caps & MPGL_CAP_SW) {
        MP_WARN(ctx->vo, "Suspected software renderer or indirect context.\n");
        if (vo->probing)
            goto cleanup;
    }

    ctx->gl->debug_context = !!(vo_flags & VOFLAG_GL_DEBUG);

    return ctx;

cleanup:
    mpgl_uninit(ctx);
    return NULL;
}

bool mpgl_reconfig_window(struct MPGLContext *ctx, int flags)
{
    return ctx->config_window(ctx, flags);
}

void mpgl_uninit(MPGLContext *ctx)
{
    if (ctx) {
        ctx->releaseGlContext(ctx);
        ctx->vo_uninit(ctx->vo);
    }
    talloc_free(ctx);
}

void mpgl_set_context(MPGLContext *ctx)
{
    if (ctx->set_current)
        ctx->set_current(ctx, true);
}

void mpgl_unset_context(MPGLContext *ctx)
{
    if (ctx->set_current)
        ctx->set_current(ctx, false);
}

void mpgl_lock(MPGLContext *ctx)
{
    mpgl_set_context(ctx);
}

void mpgl_unlock(MPGLContext *ctx)
{
    mpgl_unset_context(ctx);
}

bool mpgl_is_thread_safe(MPGLContext *ctx)
{
    return !!ctx->set_current;
}

void mp_log_source(struct mp_log *log, int lev, const char *src)
{
    int line = 1;
    if (!src)
        return;
    while (*src) {
        const char *end = strchr(src, '\n');
        const char *next = end + 1;
        if (!end)
            next = end = src + strlen(src);
        mp_msg(log, lev, "[%3d] %.*s\n", line, (int)(end - src), src);
        line++;
        src = next;
    }
}
