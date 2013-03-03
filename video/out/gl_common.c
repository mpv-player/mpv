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
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "talloc.h"
#include "gl_common.h"
#include "core/options.h"
#include "sub/sub.h"
#include "bitmap_packer.h"

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

void glCheckError(GL *gl, const char *info)
{
    for (;;) {
        GLenum error = gl->GetError();
        if (error == GL_NO_ERROR)
            break;
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] %s: OpenGL error %s.\n", info,
               gl_error_to_string(error));
    }
}

//! \defgroup glcontext OpenGL context management helper functions

//! \defgroup gltexture OpenGL texture handling helper functions

//! \defgroup glconversion OpenGL conversion helper functions

/**
 * \brief adjusts the GL_UNPACK_ALIGNMENT to fit the stride.
 * \param stride number of bytes per line for which alignment should fit.
 * \ingroup glgeneral
 */
void glAdjustAlignment(GL *gl, int stride)
{
    GLint gl_alignment;
    if (stride % 8 == 0)
        gl_alignment = 8;
    else if (stride % 4 == 0)
        gl_alignment = 4;
    else if (stride % 2 == 0)
        gl_alignment = 2;
    else
        gl_alignment = 1;
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, gl_alignment);
    gl->PixelStorei(GL_PACK_ALIGNMENT, gl_alignment);
}

struct feature {
    int id;
    const char *name;
};

static const struct feature features[] = {
    {MPGL_CAP_GL,               "Basic OpenGL"},
    {MPGL_CAP_GL_LEGACY,        "Legacy OpenGL"},
    {MPGL_CAP_GL2,              "OpenGL 2.0"},
    {MPGL_CAP_GL21,             "OpenGL 2.1"},
    {MPGL_CAP_GL3,              "OpenGL 3.0"},
    {MPGL_CAP_FB,               "Framebuffers"},
    {MPGL_CAP_VAO,              "VAOs"},
    {MPGL_CAP_SRGB_TEX,         "sRGB textures"},
    {MPGL_CAP_SRGB_FB,          "sRGB framebuffers"},
    {MPGL_CAP_FLOAT_TEX,        "Float textures"},
    {MPGL_CAP_TEX_RG,           "RG textures"},
    {MPGL_CAP_NO_SW,            "NO_SW"},
    {0},
};

static void list_features(int set, int msgl, bool invert)
{
    for (const struct feature *f = &features[0]; f->id; f++) {
        if (invert == !(f->id & set))
            mp_msg(MSGT_VO, msgl, " [%s]", f->name);
    }
    mp_msg(MSGT_VO, msgl, "\n");
}

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

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

void *mp_getdladdr(const char *s)
{
    void *ret = NULL;
#ifdef HAVE_LIBDL
    void *handle = dlopen(NULL, RTLD_LAZY);
    if (!handle)
        return NULL;
    ret = dlsym(handle, s);
    dlclose(handle);
#endif
    return ret;
}

#define FN_OFFS(name) offsetof(GL, name)

// Define the function with a "hard" reference to the function as fallback.
// (This requires linking with a compatible OpenGL library.)
#define DEF_FN_HARD(name)       {FN_OFFS(name), {"gl" # name}, gl ## name}

#define DEF_FN(name)            {FN_OFFS(name), {"gl" # name}}
#define DEF_FN_NAMES(name, ...) {FN_OFFS(name), {__VA_ARGS__}}

struct gl_function {
    ptrdiff_t offset;
    char *funcnames[7];
    void *fallback;
};

struct gl_functions {
    const char *extension;      // introduced with this extension in any version
    int provides;               // bitfield of MPGL_CAP_* constants
    int ver_core;               // introduced as required function
    int ver_removed;            // removed as required function (no replacement)
    bool partial_ok;            // loading only some functions is ok
    struct gl_function *functions;
};

#define MAX_FN_COUNT 50         // max functions per gl_functions section

struct gl_functions gl_functions[] = {
    // GL functions which are always available anywhere at least since 1.1
    {
        .ver_core = MPGL_VER(1, 1),
        .provides = MPGL_CAP_GL,
        .functions = (struct gl_function[]) {
            DEF_FN_HARD(Viewport),
            DEF_FN_HARD(Clear),
            DEF_FN_HARD(GenTextures),
            DEF_FN_HARD(DeleteTextures),
            DEF_FN_HARD(TexEnvi),
            DEF_FN_HARD(ClearColor),
            DEF_FN_HARD(Enable),
            DEF_FN_HARD(Disable),
            DEF_FN_HARD(DrawBuffer),
            DEF_FN_HARD(DepthMask),
            DEF_FN_HARD(BlendFunc),
            DEF_FN_HARD(Flush),
            DEF_FN_HARD(Finish),
            DEF_FN_HARD(PixelStorei),
            DEF_FN_HARD(TexImage1D),
            DEF_FN_HARD(TexImage2D),
            DEF_FN_HARD(TexSubImage2D),
            DEF_FN_HARD(GetTexImage),
            DEF_FN_HARD(TexParameteri),
            DEF_FN_HARD(TexParameterf),
            DEF_FN_HARD(TexParameterfv),
            DEF_FN_HARD(GetIntegerv),
            DEF_FN_HARD(GetBooleanv),
            DEF_FN_HARD(ColorMask),
            DEF_FN_HARD(ReadPixels),
            DEF_FN_HARD(ReadBuffer),
            DEF_FN_HARD(DrawArrays),
            DEF_FN_HARD(GetString),
            DEF_FN_HARD(GetError),
            DEF_FN_HARD(GetTexLevelParameteriv),
            {0}
        },
    },
    // GL 2.0-3.x functions
    {
        .ver_core = MPGL_VER(2, 0),
        .provides = MPGL_CAP_GL2,
        .functions = (struct gl_function[]) {
            DEF_FN(GenBuffers),
            DEF_FN(DeleteBuffers),
            DEF_FN(BindBuffer),
            DEF_FN(MapBuffer),
            DEF_FN(UnmapBuffer),
            DEF_FN(BufferData),
            DEF_FN(ActiveTexture),
            DEF_FN(BindTexture),
            DEF_FN(GetAttribLocation),
            DEF_FN(EnableVertexAttribArray),
            DEF_FN(DisableVertexAttribArray),
            DEF_FN(VertexAttribPointer),
            DEF_FN(UseProgram),
            DEF_FN(GetUniformLocation),
            DEF_FN(CompileShader),
            DEF_FN(CreateProgram),
            DEF_FN(CreateShader),
            DEF_FN(ShaderSource),
            DEF_FN(LinkProgram),
            DEF_FN(AttachShader),
            DEF_FN(DeleteShader),
            DEF_FN(DeleteProgram),
            DEF_FN(GetShaderInfoLog),
            DEF_FN(GetShaderiv),
            DEF_FN(GetProgramInfoLog),
            DEF_FN(GetProgramiv),
            DEF_FN(BindAttribLocation),
            DEF_FN(Uniform1f),
            DEF_FN(Uniform2f),
            DEF_FN(Uniform3f),
            DEF_FN(Uniform1i),
            DEF_FN(UniformMatrix3fv),
            DEF_FN(TexImage3D),
            {0},
        },
    },
    // GL 2.1-3.x functions (also: GLSL 120 shaders)
    {
        .ver_core = MPGL_VER(2, 1),
        .provides = MPGL_CAP_GL21,
        .functions = (struct gl_function[]) {
            DEF_FN(UniformMatrix4x3fv),
            {0}
        },
    },
    // GL 3.x core only functions.
    {
        .ver_core = MPGL_VER(3, 0),
        .provides = MPGL_CAP_GL3 | MPGL_CAP_SRGB_TEX | MPGL_CAP_SRGB_FB,
        .functions = (struct gl_function[]) {
            DEF_FN(GetStringi),
            {0}
        },
    },
    // Framebuffers, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = MPGL_VER(3, 0),
        .extension = "GL_ARB_framebuffer_object",
        .provides = MPGL_CAP_FB,
        .functions = (struct gl_function[]) {
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
        .ver_removed = MPGL_VER(3, 0), // don't touch these fn names in 3.x
        .extension = "GL_EXT_framebuffer_object",
        .provides = MPGL_CAP_FB,
        .functions = (struct gl_function[]) {
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
        .ver_core = MPGL_VER(3, 0),
        .extension = "GL_ARB_vertex_array_object",
        .provides = MPGL_CAP_VAO,
        .functions = (struct gl_function[]) {
            DEF_FN(GenVertexArrays),
            DEF_FN(BindVertexArray),
            DEF_FN(DeleteVertexArrays),
            {0}
        }
    },
    // sRGB textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = MPGL_VER(3, 0),
        .extension = "GL_EXT_texture_sRGB",
        .provides = MPGL_CAP_SRGB_TEX,
        .functions = (struct gl_function[]) {{0}},
    },
    // sRGB framebuffers, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = MPGL_VER(3, 0),
        .extension = "GL_EXT_framebuffer_sRGB",
        .provides = MPGL_CAP_SRGB_FB,
        .functions = (struct gl_function[]) {{0}},
    },
    // Float textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = MPGL_VER(3, 0),
        .extension = "GL_ARB_texture_float",
        .provides = MPGL_CAP_FLOAT_TEX,
        .functions = (struct gl_function[]) {{0}},
    },
    // GL_RED / GL_RG textures, extension in GL 2.x, core in GL 3.x core.
    {
        .ver_core = MPGL_VER(3, 0),
        .extension = "GL_ARB_texture_rg",
        .provides = MPGL_CAP_TEX_RG,
        .functions = (struct gl_function[]) {{0}},
    },
    // Swap control, always an OS specific extension
    {
        .extension = "_swap_control",
        .functions = (struct gl_function[]) {
            DEF_FN_NAMES(SwapInterval, "glXSwapIntervalSGI", "glXSwapInterval",
                         "wglSwapIntervalSGI", "wglSwapInterval",
                         "wglSwapIntervalEXT"),
            {0}
        },
    },
    // GL legacy functions in GL 1.x - 2.x, removed from GL 3.x
    {
        .ver_core = MPGL_VER(1, 1),
        .ver_removed = MPGL_VER(3, 0),
        .provides = MPGL_CAP_GL_LEGACY,
        .functions = (struct gl_function[]) {
            DEF_FN_HARD(Begin),
            DEF_FN_HARD(End),
            DEF_FN_HARD(MatrixMode),
            DEF_FN_HARD(LoadIdentity),
            DEF_FN_HARD(Translated),
            DEF_FN_HARD(Scaled),
            DEF_FN_HARD(Ortho),
            DEF_FN_HARD(PushMatrix),
            DEF_FN_HARD(PopMatrix),
            DEF_FN_HARD(GenLists),
            DEF_FN_HARD(DeleteLists),
            DEF_FN_HARD(NewList),
            DEF_FN_HARD(EndList),
            DEF_FN_HARD(CallList),
            DEF_FN_HARD(CallLists),
            DEF_FN_HARD(Color4ub),
            DEF_FN_HARD(Color4f),
            DEF_FN_HARD(TexCoord2f),
            DEF_FN_HARD(TexCoord2fv),
            DEF_FN_HARD(Vertex2f),
            DEF_FN_HARD(VertexPointer),
            DEF_FN_HARD(ColorPointer),
            DEF_FN_HARD(TexCoordPointer),
            DEF_FN_HARD(EnableClientState),
            DEF_FN_HARD(DisableClientState),
            {0}
        },
    },
    // Loading of old extensions, which are later added to GL 2.0.
    // NOTE: actually we should be checking the extension strings: the OpenGL
    //       library could provide an entry point, but not implement it.
    //       But the previous code didn't do that, and nobody ever complained.
    {
        .ver_removed = MPGL_VER(2, 1),
        .partial_ok = true,
        .functions = (struct gl_function[]) {
            DEF_FN_NAMES(GenBuffers, "glGenBuffers", "glGenBuffersARB"),
            DEF_FN_NAMES(DeleteBuffers, "glDeleteBuffers", "glDeleteBuffersARB"),
            DEF_FN_NAMES(BindBuffer, "glBindBuffer", "glBindBufferARB"),
            DEF_FN_NAMES(MapBuffer, "glMapBuffer", "glMapBufferARB"),
            DEF_FN_NAMES(UnmapBuffer, "glUnmapBuffer", "glUnmapBufferARB"),
            DEF_FN_NAMES(BufferData, "glBufferData", "glBufferDataARB"),
            DEF_FN_NAMES(ActiveTexture, "glActiveTexture", "glActiveTextureARB"),
            DEF_FN_NAMES(BindTexture, "glBindTexture", "glBindTextureARB", "glBindTextureEXT"),
            DEF_FN_NAMES(MultiTexCoord2f, "glMultiTexCoord2f", "glMultiTexCoord2fARB"),
            DEF_FN_NAMES(TexImage3D, "glTexImage3D"),
            {0}
        },
    },
    // Ancient ARB shaders.
    {
        .extension = "_program",
        .ver_removed = MPGL_VER(3, 0),
        .functions = (struct gl_function[]) {
            DEF_FN_NAMES(GenPrograms, "glGenProgramsARB"),
            DEF_FN_NAMES(DeletePrograms, "glDeleteProgramsARB"),
            DEF_FN_NAMES(BindProgram, "glBindProgramARB"),
            DEF_FN_NAMES(ProgramString, "glProgramStringARB"),
            DEF_FN_NAMES(GetProgramivARB, "glGetProgramivARB"),
            DEF_FN_NAMES(ProgramEnvParameter4f, "glProgramEnvParameter4fARB"),
            {0}
        },
    },
    // Ancient ATI extensions.
    {
        .extension = "ATI_fragment_shader",
        .ver_removed = MPGL_VER(3, 0),
        .functions = (struct gl_function[]) {
            DEF_FN_NAMES(BeginFragmentShader, "glBeginFragmentShaderATI"),
            DEF_FN_NAMES(EndFragmentShader, "glEndFragmentShaderATI"),
            DEF_FN_NAMES(SampleMap, "glSampleMapATI"),
            DEF_FN_NAMES(ColorFragmentOp2, "glColorFragmentOp2ATI"),
            DEF_FN_NAMES(ColorFragmentOp3, "glColorFragmentOp3ATI"),
            DEF_FN_NAMES(SetFragmentShaderConstant, "glSetFragmentShaderConstantATI"),
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
// Note: if you create a CONTEXT_FORWARD_COMPATIBLE_BIT_ARB with OpenGL 3.0,
//       you must append "GL_ARB_compatibility" to ext2.
void mpgl_load_functions(GL *gl, void *(*getProcAddress)(const GLubyte *),
                         const char *ext2)
{
    talloc_free_children(gl);
    *gl = (GL) {
        .extensions = talloc_strdup(gl, ext2 ? ext2 : ""),
    };

    if (!getProcAddress)
        getProcAddress = (void *)mp_getdladdr;

    gl->GetString = getProcAddress("glGetString");
    if (!gl->GetString)
        gl->GetString = glGetString;

    GLint major = 0, minor = 0;
    const char *version = gl->GetString(GL_VERSION);
    sscanf(version, "%d.%d", &major, &minor);
    gl->version = MPGL_VER(major, minor);
    mp_msg(MSGT_VO, MSGL_V, "[gl] Detected OpenGL %d.%d.\n", major, minor);

    mp_msg(MSGT_VO, MSGL_V, "GL_VENDOR='%s'\n",   gl->GetString(GL_VENDOR));
    mp_msg(MSGT_VO, MSGL_V, "GL_RENDERER='%s'\n", gl->GetString(GL_RENDERER));
    mp_msg(MSGT_VO, MSGL_V, "GL_VERSION='%s'\n",  gl->GetString(GL_VERSION));
    mp_msg(MSGT_VO, MSGL_V, "GL_SHADING_LANGUAGE_VERSION='%s'\n",
                            gl->GetString(GL_SHADING_LANGUAGE_VERSION));

    // Note: This code doesn't handle CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
    //       on OpenGL 3.0 correctly. Apparently there's no way to detect this
    //       situation, because GL_ARB_compatibility is specified only for 3.1
    //       and above.

    bool has_legacy = false;
    if (gl->version >= MPGL_VER(3, 0)) {
        gl->GetStringi = getProcAddress("glGetStringi");
        gl->GetIntegerv = getProcAddress("glGetIntegerv");

        if (!(gl->GetStringi && gl->GetIntegerv))
            return;

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
        if (gl->version == MPGL_VER(3, 0))
            has_legacy = true;
    } else {
        const char *ext = (char*)gl->GetString(GL_EXTENSIONS);
        gl->extensions = talloc_asprintf_append(gl->extensions, " %s", ext);

        has_legacy = true;
    }

    if (has_legacy)
        mp_msg(MSGT_VO, MSGL_V, "[gl] OpenGL legacy compat. found.\n");
    mp_msg(MSGT_VO, MSGL_DBG2, "[gl] Combined OpenGL extensions string:\n%s\n",
           gl->extensions);

    for (int n = 0; n < sizeof(gl_functions) / sizeof(gl_functions[0]); n++) {
        struct gl_functions *section = &gl_functions[n];

        // With has_legacy, the legacy functions are still available, and
        // functions are never actually removed. (E.g. the context could be at
        // version >= 3.0, but functions like glBegin still exist and work.)
        if (!has_legacy && section->ver_removed &&
            gl->version >= section->ver_removed)
            continue;

        // NOTE: Function entrypoints can exist, even if they do not work.
        //       We must always check extension strings and versions.

        bool exists = false;
        if (section->ver_core)
            exists = gl->version >= section->ver_core;

        if (section->extension && strstr(gl->extensions, section->extension))
            exists = true;

        if (section->partial_ok)
            exists = true; // possibly

        if (!exists)
            continue;

        void *loaded[MAX_FN_COUNT] = {0};
        bool all_loaded = true;

        for (int i = 0; section->functions[i].funcnames[0]; i++) {
            struct gl_function *fn = &section->functions[i];
            void *ptr = NULL;
            for (int x = 0; fn->funcnames[x]; x++) {
                ptr = getProcAddress((const GLubyte *)fn->funcnames[x]);
                if (ptr)
                    break;
            }
            if (!ptr)
                ptr = fn->fallback;
            if (!ptr) {
                all_loaded = false;
                if (!section->partial_ok) {
                    mp_msg(MSGT_VO, MSGL_V, "[gl] Required function '%s' not "
                           "found for %s/%d.%d.\n", fn->funcnames[0],
                           section->extension ? section->extension : "native",
                           MPGL_VER_GET_MAJOR(section->ver_core),
                           MPGL_VER_GET_MINOR(section->ver_core));
                    break;
                }
            }
            assert(i < MAX_FN_COUNT);
            loaded[i] = ptr;
        }

        if (all_loaded || section->partial_ok) {
            gl->mpgl_caps |= section->provides;
            for (int i = 0; section->functions[i].funcnames[0]; i++) {
                struct gl_function *fn = &section->functions[i];
                void **funcptr = (void**)(((char*)gl) + fn->offset);
                if (loaded[i])
                    *funcptr = loaded[i];
            }
        }
    }

    gl->glsl_version = 0;
    if (gl->version >= MPGL_VER(2, 0))
        gl->glsl_version = 110;
    if (gl->version >= MPGL_VER(2, 1))
        gl->glsl_version = 120;
    if (gl->version >= MPGL_VER(3, 0))
        gl->glsl_version = 130;
    // Specifically needed for OSX (normally we request 3.0 contexts only, but
    // OSX always creates 3.2 contexts when requesting a core context).
    if (gl->version >= MPGL_VER(3, 2))
        gl->glsl_version = 150;

    if (!is_software_gl(gl))
        gl->mpgl_caps |= MPGL_CAP_NO_SW;

    mp_msg(MSGT_VO, MSGL_V, "[gl] Detected OpenGL features:");
    list_features(gl->mpgl_caps, MSGL_V, false);
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
    case GL_YCBCR_MESA:
        return 2;
    case GL_RGB:
    case GL_BGR:
        return 3 * component_size;
    case GL_RGBA:
    case GL_BGRA:
        return 4 * component_size;
    case GL_RED:
        return component_size;
    case GL_RG:
    case GL_LUMINANCE_ALPHA:
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
    // this is not always correct, but should work for MPlayer
    glAdjustAlignment(gl, stride);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, stride / glFmt2bpp(format, type));
    for (; y + slice <= y_max; y += slice) {
        gl->TexSubImage2D(target, 0, x, y, w, slice, format, type, data);
        data += stride * slice;
    }
    if (y < y_max)
        gl->TexSubImage2D(target, 0, x, y, w, y_max - y, format, type, data);
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
    glAdjustAlignment(gl, stride);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, w);
    gl->TexSubImage2D(target, 0, x, y, w, h, format, type, data);
    if (scratch) {
        *scratch = data;
    } else {
        talloc_free(data);
    }
}

/**
 * \brief download a texture, handling things like stride and slices
 * \param target texture target, usually GL_TEXTURE_2D
 * \param format OpenGL format of data
 * \param type OpenGL type of data
 * \param dataptr destination memory for download
 * \param stride data stride (must be positive)
 * \ingroup gltexture
 */
void glDownloadTex(GL *gl, GLenum target, GLenum format, GLenum type,
                   void *dataptr, int stride)
{
    // this is not always correct, but should work for MPlayer
    glAdjustAlignment(gl, stride);
    gl->PixelStorei(GL_PACK_ROW_LENGTH, stride / glFmt2bpp(format, type));
    gl->GetTexImage(target, 0, format, type, dataptr);
}

void glEnable3DLeft(GL *gl, int type)
{
    GLint buffer;
    switch (type) {
    case GL_3D_RED_CYAN:
        gl->ColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        break;
    case GL_3D_GREEN_MAGENTA:
        gl->ColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
        break;
    case GL_3D_QUADBUFFER:
        gl->GetIntegerv(GL_DRAW_BUFFER, &buffer);
        switch (buffer) {
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
            buffer = GL_FRONT_LEFT;
            break;
        case GL_BACK:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
            buffer = GL_BACK_LEFT;
            break;
        }
        gl->DrawBuffer(buffer);
        break;
    }
}

void glEnable3DRight(GL *gl, int type)
{
    GLint buffer;
    switch (type) {
    case GL_3D_RED_CYAN:
        gl->ColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
        break;
    case GL_3D_GREEN_MAGENTA:
        gl->ColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
        break;
    case GL_3D_QUADBUFFER:
        gl->GetIntegerv(GL_DRAW_BUFFER, &buffer);
        switch (buffer) {
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
            buffer = GL_FRONT_RIGHT;
            break;
        case GL_BACK:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
            buffer = GL_BACK_RIGHT;
            break;
        }
        gl->DrawBuffer(buffer);
        break;
    }
}

void glDisable3D(GL *gl, int type)
{
    GLint buffer;
    switch (type) {
    case GL_3D_RED_CYAN:
    case GL_3D_GREEN_MAGENTA:
        gl->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        break;
    case GL_3D_QUADBUFFER:
        gl->DrawBuffer(GL_BACK);
        gl->GetIntegerv(GL_DRAW_BUFFER, &buffer);
        switch (buffer) {
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
            buffer = GL_FRONT;
            break;
        case GL_BACK:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
            buffer = GL_BACK;
            break;
        }
        gl->DrawBuffer(buffer);
        break;
    }
}

mp_image_t *glGetWindowScreenshot(GL *gl)
{
    GLint vp[4]; //x, y, w, h
    gl->GetIntegerv(GL_VIEWPORT, vp);
    mp_image_t *image = mp_image_alloc(IMGFMT_RGB24, vp[2], vp[3]);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    gl->PixelStorei(GL_PACK_ALIGNMENT, 0);
    gl->PixelStorei(GL_PACK_ROW_LENGTH, 0);
    gl->ReadBuffer(GL_FRONT);
    //flip image while reading
    for (int y = 0; y < vp[3]; y++) {
        gl->ReadPixels(vp[0], vp[1] + vp[3] - y - 1, vp[2], 1,
                       GL_RGB, GL_UNSIGNED_BYTE,
                       image->planes[0] + y * image->stride[0]);
    }
    return image;
}

typedef void (*MPGLSetBackendFn)(MPGLContext *ctx);

struct backend {
    const char *name;
    MPGLSetBackendFn init;
};

static struct backend backends[] = {
#ifdef CONFIG_GL_COCOA
    {"cocoa", mpgl_set_backend_cocoa},
#endif
#ifdef CONFIG_GL_WIN32
    {"win", mpgl_set_backend_w32},
#endif
#ifdef CONFIG_GL_X11
    {"x11", mpgl_set_backend_x11},
#endif
#ifdef CONFIG_GL_WAYLAND
    {"wayland", mpgl_set_backend_wayland},
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

static MPGLContext *init_backend(struct vo *vo, MPGLSetBackendFn set_backend)
{
    MPGLContext *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (MPGLContext) {
        .gl = talloc_zero(ctx, GL),
        .vo = vo,
    };
    set_backend(ctx);
    if (!ctx->vo_init(vo)) {
        talloc_free(ctx);
        ctx = NULL;
    }
    return ctx;
}

MPGLContext *mpgl_init(struct vo *vo, const char *backend_name)
{
    MPGLContext *ctx = NULL;
    int index = mpgl_find_backend(backend_name);
    if (index == -1) {
        for (const struct backend *entry = backends; entry->name; entry++) {
            ctx = init_backend(vo, entry->init);
            if (ctx)
                return ctx;
        }
    } else if (index >= 0) {
        ctx = init_backend(vo, backends[index].init);
    }
    return ctx;
}

bool mpgl_config_window(struct MPGLContext *ctx, int gl_caps, uint32_t d_width,
                        uint32_t d_height, uint32_t flags)
{
    gl_caps |= MPGL_CAP_GL;

    ctx->requested_gl_version = (gl_caps & MPGL_CAP_GL_LEGACY)
                                ? MPGL_VER(2, 1) : MPGL_VER(3, 0);

    if (ctx->config_window(ctx, d_width, d_height, flags)) {
        int missing = (ctx->gl->mpgl_caps & gl_caps) ^ gl_caps;
        if (!missing)
            return true;

        mp_msg(MSGT_VO, MSGL_WARN, "[gl] Missing OpenGL features:");
        list_features(missing, MSGL_WARN, false);
        if (missing & MPGL_CAP_NO_SW) {
            mp_msg(MSGT_VO, MSGL_WARN, "[gl] Rejecting suspected software "
                    "OpenGL renderer.\n");
        }
    }

    mp_msg(MSGT_VO, MSGL_ERR, "[gl] OpenGL context creation failed!\n");
    return false;
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

void mp_log_source(int mod, int lev, const char *src)
{
    int line = 1;
    if (!src)
        return;
    while (*src) {
        const char *end = strchr(src, '\n');
        const char *next = end + 1;
        if (!end)
            next = end = src + strlen(src);
        mp_msg(mod, lev, "[%3d] %.*s\n", line, (int)(end - src), src);
        line++;
        src = next;
    }
}
