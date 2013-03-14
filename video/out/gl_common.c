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
/**
 * \brief find address of a linked function
 * \param s name of function to find
 * \return address of function or NULL if not found
 */
static void *getdladdr(const char *s)
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
// GL context.
// getProcAddress: function to resolve function names, may be NULL
// ext2: an extra extension string
// Note: if you create a CONTEXT_FORWARD_COMPATIBLE_BIT_ARB with OpenGL 3.0,
//       you must append "GL_ARB_compatibility" to ext2.
static void getFunctions(GL *gl, void *(*getProcAddress)(const GLubyte *),
                         const char *ext2)
{
    talloc_free_children(gl);
    *gl = (GL) {
        .extensions = talloc_strdup(gl, ext2 ? ext2 : ""),
    };

    if (!getProcAddress)
        getProcAddress = (void *)getdladdr;

    gl->GetString = getProcAddress("glGetString");
    if (!gl->GetString)
        gl->GetString = glGetString;

    GLint major = 0, minor = 0;
    const char *version = gl->GetString(GL_VERSION);
    sscanf(version, "%d.%d", &major, &minor);
    gl->version = MPGL_VER(major, minor);
    mp_msg(MSGT_VO, MSGL_V, "[gl] Detected OpenGL %d.%d.\n", major, minor);

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

#ifdef CONFIG_GL_COCOA
#include "cocoa_common.h"

static bool config_window_cocoa(struct MPGLContext *ctx, uint32_t d_width,
                                uint32_t d_height, uint32_t flags)
{
    int rv = vo_cocoa_config_window(ctx->vo, d_width, d_height, flags,
                                    ctx->requested_gl_version >= MPGL_VER(3, 0));
    if (rv != 0)
        return false;

    getFunctions(ctx->gl, (void *)vo_cocoa_glgetaddr, NULL);

    ctx->depth_r = vo_cocoa_cgl_color_size(ctx->vo);
    ctx->depth_g = vo_cocoa_cgl_color_size(ctx->vo);
    ctx->depth_b = vo_cocoa_cgl_color_size(ctx->vo);

    if (!ctx->gl->SwapInterval)
        ctx->gl->SwapInterval = vo_cocoa_swap_interval;

    return true;
}

static void releaseGlContext_cocoa(MPGLContext *ctx)
{
}

static void swapGlBuffers_cocoa(MPGLContext *ctx)
{
    vo_cocoa_swap_buffers(ctx->vo);
}
#endif

#ifdef CONFIG_GL_WIN32
#include <windows.h>
#include "w32_common.h"

struct w32_context {
    HGLRC context;
};

static void *w32gpa(const GLubyte *procName)
{
    HMODULE oglmod;
    void *res = wglGetProcAddress(procName);
    if (res)
        return res;
    oglmod = GetModuleHandle("opengl32.dll");
    return GetProcAddress(oglmod, procName);
}

static bool create_context_w32_old(struct MPGLContext *ctx)
{
    GL *gl = ctx->gl;

    struct w32_context *w32_ctx = ctx->priv;
    HGLRC *context = &w32_ctx->context;

    if (*context) {
        gl->Finish();   // supposedly to prevent flickering
        return true;
    }

    HWND win = ctx->vo->w32->window;
    HDC windc = GetDC(win);
    bool res = false;

    HGLRC new_context = wglCreateContext(windc);
    if (!new_context) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GL context!\n");
        goto out;
    }

    if (!wglMakeCurrent(windc, new_context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL context!\n");
        wglDeleteContext(new_context);
        goto out;
    }

    *context = new_context;

    getFunctions(ctx->gl, w32gpa, NULL);
    res = true;

out:
    ReleaseDC(win, windc);
    return res;
}

static bool create_context_w32_gl3(struct MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;
    HGLRC *context = &w32_ctx->context;

    if (*context) // reuse existing context
        return true; // not reusing it breaks gl3!

    HWND win = ctx->vo->w32->window;
    HDC windc = GetDC(win);
    HGLRC new_context = 0;

    new_context = wglCreateContext(windc);
    if (!new_context) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GL context!\n");
        return false;
    }

    // set context
    if (!wglMakeCurrent(windc, new_context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL context!\n");
        goto out;
    }

    const char *(GLAPIENTRY *wglGetExtensionsStringARB)(HDC hdc)
        = w32gpa((const GLubyte*)"wglGetExtensionsStringARB");

    if (!wglGetExtensionsStringARB)
        goto unsupported;

    const char *wgl_exts = wglGetExtensionsStringARB(windc);
    if (!strstr(wgl_exts, "WGL_ARB_create_context"))
        goto unsupported;

    HGLRC (GLAPIENTRY *wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext,
                                                   const int *attribList)
        = w32gpa((const GLubyte*)"wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB)
        goto unsupported;

    int gl_version = ctx->requested_gl_version;
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, MPGL_VER_GET_MAJOR(gl_version),
        WGL_CONTEXT_MINOR_VERSION_ARB, MPGL_VER_GET_MINOR(gl_version),
        WGL_CONTEXT_FLAGS_ARB, 0,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    *context = wglCreateContextAttribsARB(windc, 0, attribs);
    if (! *context) {
        // NVidia, instead of ignoring WGL_CONTEXT_FLAGS_ARB, will error out if
        // it's present on pre-3.2 contexts.
        // Remove it from attribs and retry the context creation.
        attribs[6] = attribs[7] = 0;
        *context = wglCreateContextAttribsARB(windc, 0, attribs);
    }
    if (! *context) {
        int err = GetLastError();
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create an OpenGL 3.x"
                                    " context: error 0x%x\n", err);
        goto out;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(new_context);

    if (!wglMakeCurrent(windc, *context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL3 context!\n");
        wglDeleteContext(*context);
        return false;
    }

    /* update function pointers */
    getFunctions(ctx->gl, w32gpa, NULL);

    int pfmt = GetPixelFormat(windc);
    PIXELFORMATDESCRIPTOR pfd;
    if (DescribePixelFormat(windc, pfmt, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
        ctx->depth_r = pfd.cRedBits;
        ctx->depth_g = pfd.cGreenBits;
        ctx->depth_b = pfd.cBlueBits;
    }

    return true;

unsupported:
    mp_msg(MSGT_VO, MSGL_ERR, "[gl] The current OpenGL implementation does"
                              " not support OpenGL 3.x \n");
out:
    wglDeleteContext(new_context);
    return false;
}

static bool config_window_w32(struct MPGLContext *ctx, uint32_t d_width,
                              uint32_t d_height, uint32_t flags)
{
    if (!vo_w32_config(ctx->vo, d_width, d_height, flags))
        return false;

    bool success = false;
    if (ctx->requested_gl_version >= MPGL_VER(3, 0))
        success = create_context_w32_gl3(ctx);
    if (!success)
        success = create_context_w32_old(ctx);
    return success;
}

static void releaseGlContext_w32(MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;
    HGLRC *context = &w32_ctx->context;
    if (*context) {
        wglMakeCurrent(0, 0);
        wglDeleteContext(*context);
    }
    *context = 0;
}

static void swapGlBuffers_w32(MPGLContext *ctx)
{
    HDC vo_hdc = GetDC(ctx->vo->w32->window);
    SwapBuffers(vo_hdc);
    ReleaseDC(ctx->vo->w32->window, vo_hdc);
}
#endif

#ifdef CONFIG_GL_X11
#include <X11/Xlib.h>
#include <GL/glx.h>

#include "x11_common.h"

#define MP_GET_GLX_WORKAROUNDS
#include "gl_header_fixes.h"

struct glx_context {
    XVisualInfo *vinfo;
    GLXContext context;
    GLXFBConfig fbc;
};

static bool create_context_x11_old(struct MPGLContext *ctx)
{
    struct glx_context *glx_ctx = ctx->priv;
    Display *display = ctx->vo->x11->display;
    struct vo *vo = ctx->vo;
    GL *gl = ctx->gl;

    if (glx_ctx->context)
        return true;

    GLXContext new_context = glXCreateContext(display, glx_ctx->vinfo, NULL,
                                              True);
    if (!new_context) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GLX context!\n");
        return false;
    }

    if (!glXMakeCurrent(display, ctx->vo->x11->window, new_context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GLX context!\n");
        glXDestroyContext(display, new_context);
        return false;
    }

    void *(*getProcAddress)(const GLubyte *);
    getProcAddress = getdladdr("glXGetProcAddress");
    if (!getProcAddress)
        getProcAddress = getdladdr("glXGetProcAddressARB");

    const char *glxstr = "";
    const char *(*glXExtStr)(Display *, int)
        = getdladdr("glXQueryExtensionsString");
    if (glXExtStr)
        glxstr = glXExtStr(display, ctx->vo->x11->screen);

    getFunctions(gl, getProcAddress, glxstr);
    if (!gl->GenPrograms && gl->GetString &&
        gl->version < MPGL_VER(3, 0) &&
        getProcAddress &&
        strstr(gl->GetString(GL_EXTENSIONS), "GL_ARB_vertex_program"))
    {
        mp_msg(MSGT_VO, MSGL_WARN,
                "Broken glXGetProcAddress detected, trying workaround\n");
        getFunctions(gl, NULL, glxstr);
    }

    glx_ctx->context = new_context;

    if (!glXIsDirect(vo->x11->display, new_context))
        ctx->gl->mpgl_caps &= ~MPGL_CAP_NO_SW;

    return true;
}

typedef GLXContext (*glXCreateContextAttribsARBProc)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

static bool create_context_x11_gl3(struct MPGLContext *ctx, bool debug)
{
    struct glx_context *glx_ctx = ctx->priv;
    struct vo *vo = ctx->vo;

    if (glx_ctx->context)
        return true;

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)
            glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    const char *glxstr = "";
    const char *(*glXExtStr)(Display *, int)
        = getdladdr("glXQueryExtensionsString");
    if (glXExtStr)
        glxstr = glXExtStr(vo->x11->display, vo->x11->screen);
    bool have_ctx_ext = glxstr && !!strstr(glxstr, "GLX_ARB_create_context");

    if (!(have_ctx_ext && glXCreateContextAttribsARB)) {
        return false;
    }

    int gl_version = ctx->requested_gl_version;
    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, MPGL_VER_GET_MAJOR(gl_version),
        GLX_CONTEXT_MINOR_VERSION_ARB, MPGL_VER_GET_MINOR(gl_version),
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB, debug ? GLX_CONTEXT_DEBUG_BIT_ARB : 0,
        None
    };
    GLXContext context = glXCreateContextAttribsARB(vo->x11->display,
                                                    glx_ctx->fbc, 0, True,
                                                    context_attribs);
    if (!context) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GLX context!\n");
        return false;
    }

    // set context
    if (!glXMakeCurrent(vo->x11->display, vo->x11->window, context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GLX context!\n");
        glXDestroyContext(vo->x11->display, context);
        return false;
    }

    glx_ctx->context = context;

    getFunctions(ctx->gl, (void *)glXGetProcAddress, glxstr);

    if (!glXIsDirect(vo->x11->display, context))
        ctx->gl->mpgl_caps &= ~MPGL_CAP_NO_SW;

    return true;
}

// The GL3/FBC initialization code roughly follows/copies from:
//  http://www.opengl.org/wiki/Tutorial:_OpenGL_3.0_Context_Creation_(GLX)
// but also uses some of the old code.

static GLXFBConfig select_fb_config(struct vo *vo, const int *attribs)
{
    int fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(vo->x11->display, vo->x11->screen,
                                         attribs, &fbcount);
    if (!fbc)
        return NULL;

    // The list in fbc is sorted (so that the first element is the best).
    GLXFBConfig fbconfig = fbc[0];

    XFree(fbc);

    return fbconfig;
}

static bool config_window_x11(struct MPGLContext *ctx, uint32_t d_width,
                              uint32_t d_height, uint32_t flags)
{
    struct vo *vo = ctx->vo;
    struct glx_context *glx_ctx = ctx->priv;

    if (glx_ctx->context) {
        // GL context and window already exist.
        // Only update window geometry etc.
        vo_x11_config_vo_window(vo, glx_ctx->vinfo, vo->dx, vo->dy, d_width,
                                d_height, flags, "gl");
        return true;
    }

    int glx_major, glx_minor;

    // FBConfigs were added in GLX version 1.3.
    if (!glXQueryVersion(vo->x11->display, &glx_major, &glx_minor) ||
        (MPGL_VER(glx_major, glx_minor) <  MPGL_VER(1, 3)))
    {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] GLX version older than 1.3.\n");
        return false;
    }

    const int glx_attribs_stereo_value_idx = 1; // index of GLX_STEREO + 1
    int glx_attribs[] = {
        GLX_STEREO, False,
        GLX_X_RENDERABLE, True,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DOUBLEBUFFER, True,
        None
    };
    GLXFBConfig fbc = NULL;
    if (flags & VOFLAG_STEREO) {
        glx_attribs[glx_attribs_stereo_value_idx] = True;
        fbc = select_fb_config(vo, glx_attribs);
        if (!fbc) {
            mp_msg(MSGT_VO, MSGL_ERR, "[gl] Could not find a stereo visual,"
                   " 3D will probably not work!\n");
            glx_attribs[glx_attribs_stereo_value_idx] = False;
        }
    }
    if (!fbc)
        fbc = select_fb_config(vo, glx_attribs);
    if (!fbc) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] no GLX support present\n");
        return false;
    }

    glx_ctx->fbc = fbc;
    glx_ctx->vinfo = glXGetVisualFromFBConfig(vo->x11->display, fbc);

    mp_msg(MSGT_VO, MSGL_V, "[gl] GLX chose visual with ID 0x%x\n",
            (int)glx_ctx->vinfo->visualid);

    glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_RED_SIZE, &ctx->depth_r);
    glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_GREEN_SIZE, &ctx->depth_g);
    glXGetFBConfigAttrib(vo->x11->display, fbc, GLX_BLUE_SIZE, &ctx->depth_b);

    vo_x11_config_vo_window(vo, glx_ctx->vinfo, vo->dx, vo->dy, d_width,
                            d_height, flags, "gl");

    bool success = false;
    if (ctx->requested_gl_version >= MPGL_VER(3, 0))
        success = create_context_x11_gl3(ctx, flags & VOFLAG_GL_DEBUG);
    if (!success)
        success = create_context_x11_old(ctx);
    return success;
}


/**
 * \brief free the VisualInfo and GLXContext of an OpenGL context.
 * \ingroup glcontext
 */
static void releaseGlContext_x11(MPGLContext *ctx)
{
    struct glx_context *glx_ctx = ctx->priv;
    XVisualInfo **vinfo = &glx_ctx->vinfo;
    GLXContext *context = &glx_ctx->context;
    Display *display = ctx->vo->x11->display;
    GL *gl = ctx->gl;
    if (*vinfo)
        XFree(*vinfo);
    *vinfo = NULL;
    if (*context) {
        if (gl->Finish)
            gl->Finish();
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, *context);
    }
    *context = 0;
}

static void swapGlBuffers_x11(MPGLContext *ctx)
{
    glXSwapBuffers(ctx->vo->x11->display, ctx->vo->x11->window);
}
#endif

#ifdef CONFIG_GL_WAYLAND

#include "wayland_common.h"
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct egl_context {
    EGLSurface egl_surface;

    struct wl_egl_window *egl_window;

    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    } egl;
};

static void egl_resize_func(struct vo_wayland_state *wl,
                            struct egl_context *ctx)
{
    int32_t x, y, scaled_height;
    double ratio;
    int minimum_size = 50;

    if (wl->window->pending_width < minimum_size)
        wl->window->pending_width = minimum_size;
    if (wl->window->pending_height < minimum_size)
        wl->window->pending_height = minimum_size;

    ratio = (double) wl->vo->aspdat.orgw / wl->vo->aspdat.orgh;
    scaled_height = wl->window->pending_height * ratio;
    if (wl->window->pending_width > scaled_height) {
        wl->window->pending_height = wl->window->pending_width / ratio;
    } else {
        wl->window->pending_width = scaled_height;
    }

    if (wl->window->edges & WL_SHELL_SURFACE_RESIZE_LEFT)
        x = wl->window->width - wl->window->pending_width;
    else
        x = 0;

    if (wl->window->edges & WL_SHELL_SURFACE_RESIZE_TOP)
        y = wl->window->height - wl->window->pending_height;
    else
        y = 0;

    wl_egl_window_resize(ctx->egl_window,
            wl->window->pending_width,
            wl->window->pending_height,
            x, y);

    wl->window->width = wl->window->pending_width;
    wl->window->height = wl->window->pending_height;

    /* set size for mplayer */
    wl->vo->dwidth = wl->window->pending_width;
    wl->vo->dheight = wl->window->pending_height;
    wl->window->events |= VO_EVENT_RESIZE;
    wl->window->edges = 0;
    wl->window->resize_needed = 0;
}

static bool egl_create_context(struct vo_wayland_state *wl,
                               struct egl_context *egl_ctx,
                               MPGLContext *ctx)
{
    EGLint major, minor, n;

    GL *gl = ctx->gl;
    const char *eglstr = "";

    if (!(egl_ctx->egl.dpy = eglGetDisplay(wl->display->display)))
        return false;

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    /* major and minor here returns the supported EGL version (e.g.: 1.4) */
    if (eglInitialize(egl_ctx->egl.dpy, &major, &minor) != EGL_TRUE)
        return false;

    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        MPGL_VER_GET_MAJOR(ctx->requested_gl_version),
        /* EGL_CONTEXT_MINOR_VERSION_KHR, */
        /* MPGL_VER_GET_MINOR(ctx->requested_gl_version), */
        /* EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR, 0, */
        /* Segfaults on anything else than the major version */
        EGL_NONE
    };

    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE)
        return false;

    eglChooseConfig(egl_ctx->egl.dpy, config_attribs,
                    &egl_ctx->egl.conf, 1, &n);

    egl_ctx->egl.ctx = eglCreateContext(egl_ctx->egl.dpy,
                                        egl_ctx->egl.conf,
                                        EGL_NO_CONTEXT,
                                        context_attribs);
    if (!egl_ctx->egl.ctx)
        return false;

    eglMakeCurrent(egl_ctx->egl.dpy, NULL, NULL, egl_ctx->egl.ctx);

    eglstr = eglQueryString(egl_ctx->egl.dpy, EGL_EXTENSIONS);

    getFunctions(gl, (void*(*)(const GLubyte*))eglGetProcAddress, eglstr);
    if (!gl->BindProgram)
        getFunctions(gl, NULL, eglstr);

    return true;
}

static void egl_create_window(struct vo_wayland_state *wl,
                              struct egl_context *egl_ctx,
                              uint32_t width,
                              uint32_t height)
{
    egl_ctx->egl_window = wl_egl_window_create(wl->window->surface,
                                               wl->window->width,
                                               wl->window->height);

    egl_ctx->egl_surface = eglCreateWindowSurface(egl_ctx->egl.dpy,
                                                  egl_ctx->egl.conf,
                                                  egl_ctx->egl_window,
                                                  NULL);

    eglMakeCurrent(egl_ctx->egl.dpy,
                   egl_ctx->egl_surface,
                   egl_ctx->egl_surface,
                   egl_ctx->egl.ctx);

    wl_display_dispatch_pending(wl->display->display);
}

static bool config_window_wayland(struct MPGLContext *ctx,
                                  uint32_t d_width,
                                  uint32_t d_height,
                                  uint32_t flags)
{
    struct egl_context * egl_ctx = ctx->priv;
    struct vo_wayland_state * wl = ctx->vo->wayland;
    bool ret = false;

    wl->window->pending_width = d_width;
    wl->window->pending_height = d_height;
    wl->window->width = d_width;
    wl->window->height = d_height;

    vo_wayland_update_window_title(ctx->vo);

    if ((VOFLAG_FULLSCREEN & flags) && wl->window->type != TYPE_FULLSCREEN)
        vo_wayland_fullscreen(ctx->vo);

    if (!egl_ctx->egl.ctx) {
        /* Create OpenGL context */
        ret = egl_create_context(wl, egl_ctx, ctx);

        /* If successfully created the context and we don't want to hide the
         * window than also create the window immediately */
        if (ret && !(VOFLAG_HIDDEN & flags))
            egl_create_window(wl, egl_ctx, d_width, d_height);

        return ret;
    }
    else {
        /* If the window exists just resize it */
        if (egl_ctx->egl_window)
            egl_resize_func(wl, egl_ctx);

        else {
            /* If the context exists and the hidden flag is unset then
             * create the window */
            if (!(VOFLAG_HIDDEN & flags))
                egl_create_window(wl, egl_ctx, d_width, d_height);
        }
        return true;
    }
}

static void releaseGlContext_wayland(MPGLContext *ctx)
{
    GL *gl = ctx->gl;
    struct egl_context * egl_ctx = ctx->priv;

    gl->Finish();
    eglMakeCurrent(egl_ctx->egl.dpy, NULL, NULL, EGL_NO_CONTEXT);
    eglDestroyContext(egl_ctx->egl.dpy, egl_ctx->egl.ctx);
    eglTerminate(egl_ctx->egl.dpy);
    eglReleaseThread();
    wl_egl_window_destroy(egl_ctx->egl_window);
    egl_ctx->egl.ctx = NULL;
}

static void swapGlBuffers_wayland(MPGLContext *ctx)
{
    struct egl_context * egl_ctx = ctx->priv;
    struct vo_wayland_state *wl = ctx->vo->wayland;

    eglSwapBuffers(egl_ctx->egl.dpy, egl_ctx->egl_surface);

    /* resize window after the buffers have swapped
     * makes resizing more fluid */
    if (wl->window->resize_needed) {
        wl_egl_window_get_attached_size(egl_ctx->egl_window,
            &wl->window->width,
            &wl->window->height);
        egl_resize_func(wl, egl_ctx);
    }
}

#endif

struct backend {
    const char *name;
    enum MPGLType type;
};

static struct backend backends[] = {
    {"auto", GLTYPE_AUTO},
    {"cocoa", GLTYPE_COCOA},
    {"win", GLTYPE_W32},
    {"x11", GLTYPE_X11},
    {"wayland", GLTYPE_WAYLAND},
    {0}
};

int mpgl_find_backend(const char *name)
{
    for (const struct backend *entry = backends; entry->name; entry++) {
        if (strcmp(entry->name, name) == 0)
            return entry->type;
    }
    return -1;
}

MPGLContext *mpgl_init(enum MPGLType type, struct vo *vo)
{
    MPGLContext *ctx;
    if (type == GLTYPE_AUTO) {
        ctx = mpgl_init(GLTYPE_COCOA, vo);
        if (ctx)
            return ctx;
        ctx = mpgl_init(GLTYPE_W32, vo);
        if (ctx)
            return ctx;
        ctx = mpgl_init(GLTYPE_X11, vo);
        if (ctx)
            return ctx;
        return mpgl_init(GLTYPE_WAYLAND, vo);
    }
    ctx = talloc_zero(NULL, MPGLContext);
    *ctx = (MPGLContext) {
        .gl = talloc_zero(ctx, GL),
        .type = type,
        .vo = vo,
    };
    switch (ctx->type) {
#ifdef CONFIG_GL_COCOA
    case GLTYPE_COCOA:
        ctx->config_window = config_window_cocoa;
        ctx->releaseGlContext = releaseGlContext_cocoa;
        ctx->swapGlBuffers = swapGlBuffers_cocoa;
        ctx->check_events = vo_cocoa_check_events;
        ctx->update_xinerama_info = vo_cocoa_update_xinerama_info;
        ctx->fullscreen = vo_cocoa_fullscreen;
        ctx->ontop = vo_cocoa_ontop;
        ctx->vo_init = vo_cocoa_init;
        ctx->pause = vo_cocoa_pause;
        ctx->resume = vo_cocoa_resume;
        ctx->vo_uninit = vo_cocoa_uninit;
        break;
#endif
#ifdef CONFIG_GL_WIN32
    case GLTYPE_W32:
        ctx->priv = talloc_zero(ctx, struct w32_context);
        ctx->config_window = config_window_w32;
        ctx->releaseGlContext = releaseGlContext_w32;
        ctx->swapGlBuffers = swapGlBuffers_w32;
        ctx->update_xinerama_info = w32_update_xinerama_info;
        ctx->border = vo_w32_border;
        ctx->check_events = vo_w32_check_events;
        ctx->fullscreen = vo_w32_fullscreen;
        ctx->ontop = vo_w32_ontop;
        ctx->vo_init = vo_w32_init;
        ctx->vo_uninit = vo_w32_uninit;
        break;
#endif
#ifdef CONFIG_GL_X11
    case GLTYPE_X11:
        ctx->priv = talloc_zero(ctx, struct glx_context);
        ctx->config_window = config_window_x11;
        ctx->releaseGlContext = releaseGlContext_x11;
        ctx->swapGlBuffers = swapGlBuffers_x11;
        ctx->update_xinerama_info = vo_x11_update_screeninfo;
        ctx->border = vo_x11_border;
        ctx->check_events = vo_x11_check_events;
        ctx->fullscreen = vo_x11_fullscreen;
        ctx->ontop = vo_x11_ontop;
        ctx->vo_init = vo_x11_init;
        ctx->vo_uninit = vo_x11_uninit;
        break;
#endif
#ifdef CONFIG_GL_WAYLAND
    case GLTYPE_WAYLAND:
        ctx->priv = talloc_zero(ctx, struct egl_context);
        ctx->config_window = config_window_wayland;
        ctx->releaseGlContext = releaseGlContext_wayland;
        ctx->swapGlBuffers = swapGlBuffers_wayland;
        ctx->update_xinerama_info = vo_wayland_update_screeninfo;
        ctx->border = vo_wayland_border;
        ctx->check_events = vo_wayland_check_events;
        ctx->fullscreen = vo_wayland_fullscreen;
        ctx->ontop = vo_wayland_ontop;
        ctx->vo_init = vo_wayland_init;
        ctx->vo_uninit = vo_wayland_uninit;
        break;
#endif
    }
    if (ctx->vo_init && ctx->vo_init(vo))
        return ctx;
    talloc_free(ctx);
    return NULL;
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
