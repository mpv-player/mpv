/*
 * common OpenGL routines
 *
 * copyleft (C) 2005-2010 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 * Special thanks go to the xine team and Matthias Hopf, whose video_out_opengl.c
 * gave me lots of good ideas.
 *
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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include "common.h"
#include "common/common.h"
#include "utils.h"

// This guesses if the current GL context is a suspected software renderer.
static bool is_software_gl(GL *gl)
{
    const char *renderer = gl->GetString(GL_RENDERER);
    const char *vendor = gl->GetString(GL_VENDOR);
    return !(renderer && vendor) ||
           strcmp(renderer, "Software Rasterizer") == 0 ||
           strstr(renderer, "llvmpipe") ||
           strstr(renderer, "softpipe") ||
           strcmp(vendor, "Microsoft Corporation") == 0 ||
           strcmp(renderer, "Mesa X11") == 0 ||
           strcmp(renderer, "Apple Software Renderer") == 0;
}

static void GLAPIENTRY dummy_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    assert(framebuffer == 0);
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
    int ver_exclude;            // not applicable to versions >= ver_exclude
    int ver_es_exclude;         // same for GLES
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
            DEF_FN(BufferSubData),
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
            DEF_FN(Scissor),
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
            DEF_FN(GetTexLevelParameteriv),
            DEF_FN(ReadBuffer),
            DEF_FN(TexImage1D),
            DEF_FN(UnmapBuffer),
            {0}
        },
    },
    // GL 2.1 has this as extension only.
    {
        .ver_exclude = 300,
        .ver_es_exclude = 300,
        .extension = "GL_ARB_map_buffer_range",
        .functions = (const struct gl_function[]) {
            DEF_FN(MapBufferRange),
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
            DEF_FN(MapBufferRange),
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
            DEF_FN(GetFramebufferAttachmentParameteriv),
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
        .ver_core = 300,
        .ver_es_core = 300,
        .extension = "GL_EXT_texture_rg",
        .provides = MPGL_CAP_TEX_RG,
    },
    // GL_R16 etc.
    {
        .extension = "GL_EXT_texture_norm16",
        .provides = MPGL_CAP_EXT16,
        .ver_exclude = 1, // never in desktop GL
    },
    // Float texture support for GL 2.x
    {
        .extension = "GL_ARB_texture_float",
        .provides = MPGL_CAP_ARB_FLOAT,
        .ver_exclude = 300,
        .ver_es_exclude = 1,
    },
    // 16 bit float textures that can be rendered to in GLES
    {
        .extension = "GL_EXT_color_buffer_half_float",
        .provides = MPGL_CAP_EXT_CR_HFLOAT,
        .ver_exclude = 1,
        .ver_es_exclude = 320,
    },
    {
        .ver_core = 320,
        .ver_es_core = 300,
        .extension = "GL_ARB_sync",
        .functions = (const struct gl_function[]) {
            DEF_FN(FenceSync),
            DEF_FN(ClientWaitSync),
            DEF_FN(DeleteSync),
            {0}
        },
    },
    {
        .ver_core = 330,
        .extension = "GL_ARB_timer_query",
        .functions = (const struct gl_function[]) {
            DEF_FN(GenQueries),
            DEF_FN(DeleteQueries),
            DEF_FN(BeginQuery),
            DEF_FN(EndQuery),
            DEF_FN(QueryCounter),
            DEF_FN(IsQuery),
            DEF_FN(GetQueryObjectiv),
            DEF_FN(GetQueryObjecti64v),
            DEF_FN(GetQueryObjectuiv),
            DEF_FN(GetQueryObjectui64v),
            {0}
        },
    },
    {
        .extension = "GL_EXT_disjoint_timer_query",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(GenQueries, "glGenQueriesEXT"),
            DEF_FN_NAME(DeleteQueries, "glDeleteQueriesEXT"),
            DEF_FN_NAME(BeginQuery, "glBeginQueryEXT"),
            DEF_FN_NAME(EndQuery, "glEndQueryEXT"),
            DEF_FN_NAME(QueryCounter, "glQueryCounterEXT"),
            DEF_FN_NAME(IsQuery, "glIsQueryEXT"),
            DEF_FN_NAME(GetQueryObjectiv, "glGetQueryObjectivEXT"),
            DEF_FN_NAME(GetQueryObjecti64v, "glGetQueryObjecti64vEXT"),
            DEF_FN_NAME(GetQueryObjectuiv, "glGetQueryObjectuivEXT"),
            DEF_FN_NAME(GetQueryObjectui64v, "glGetQueryObjectui64vEXT"),
            {0}
        },
    },
    {
        .ver_core = 430,
        .functions = (const struct gl_function[]) {
            DEF_FN(InvalidateTexImage),
            {0}
        },
    },
    {
        .ver_core = 430,
        .ver_es_core = 300,
        .functions = (const struct gl_function[]) {
            DEF_FN(InvalidateFramebuffer),
            {0}
        },
    },
    {
        .ver_core = 410,
        .ver_es_core = 300,
        .extension = "GL_ARB_get_program_binary",
        .functions = (const struct gl_function[]) {
            DEF_FN(GetProgramBinary),
            DEF_FN(ProgramBinary),
            {0}
        },
    },
    {
        .ver_core = 440,
        .extension = "GL_ARB_buffer_storage",
        .functions = (const struct gl_function[]) {
            DEF_FN(BufferStorage),
            {0}
        },
    },
    {
        .ver_core = 420,
        .extension = "GL_ARB_shader_image_load_store",
        .functions = (const struct gl_function[]) {
            DEF_FN(BindImageTexture),
            DEF_FN(MemoryBarrier),
            {0}
        },
    },
    {
        .ver_core = 310,
        .extension = "GL_ARB_uniform_buffer_object",
        .provides = MPGL_CAP_UBO,
    },
    {
        .ver_core = 430,
        .extension = "GL_ARB_shader_storage_buffer_object",
        .provides = MPGL_CAP_SSBO,
    },
    {
        .ver_core = 430,
        .extension = "GL_ARB_compute_shader",
        .functions = (const struct gl_function[]) {
            DEF_FN(DispatchCompute),
            {0},
        },
    },
    {
        .ver_core = 430,
        .extension = "GL_ARB_arrays_of_arrays",
        .provides = MPGL_CAP_NESTED_ARRAY,
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
    // This one overrides GLX_SGI_swap_control on platforms using mesa. The
    // only difference is that it supports glXSwapInterval(0).
    {
        .extension = "GLX_MESA_swap_control",
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(SwapInterval, "glXSwapIntervalMESA"),
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
            DEF_FN(VDPAURegisterVideoSurfaceNV),
            DEF_FN(VDPAUUnregisterSurfaceNV),
            DEF_FN(VDPAUSurfaceAccessNV),
            DEF_FN(VDPAUMapSurfacesNV),
            DEF_FN(VDPAUUnmapSurfacesNV),
            {0}
        },
    },
#if HAVE_GL_DXINTEROP
    {
        .extension = "WGL_NV_DX_interop",
        .provides = MPGL_CAP_DXINTEROP,
        .functions = (const struct gl_function[]) {
            DEF_FN_NAME(DXSetResourceShareHandleNV, "wglDXSetResourceShareHandleNV"),
            DEF_FN_NAME(DXOpenDeviceNV, "wglDXOpenDeviceNV"),
            DEF_FN_NAME(DXCloseDeviceNV, "wglDXCloseDeviceNV"),
            DEF_FN_NAME(DXRegisterObjectNV, "wglDXRegisterObjectNV"),
            DEF_FN_NAME(DXUnregisterObjectNV, "wglDXUnregisterObjectNV"),
            DEF_FN_NAME(DXLockObjectsNV, "wglDXLockObjectsNV"),
            DEF_FN_NAME(DXUnlockObjectsNV, "wglDXUnlockObjectsNV"),
            {0}
        },
    },
#endif
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
    // This is not used by the render API, only the deprecated opengl-cb API.
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
    {
        .extension = "GL_ANGLE_translated_shader_source",
        .functions = (const struct gl_function[]) {
            DEF_FN(GetTranslatedShaderSourceANGLE),
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
// get_fn: function to resolve function names
// ext2: an extra extension string
// log: used to output messages
void mpgl_load_functions2(GL *gl, void *(*get_fn)(void *ctx, const char *n),
                          void *fn_ctx, const char *ext2, struct mp_log *log)
{
    talloc_free(gl->extensions);
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
    if (!version_string) {
        mp_fatal(log, "glGetString(GL_VERSION) returned NULL.\n");
        goto error;
    }
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

        if (gl->version && section->ver_exclude &&
            gl->version >= section->ver_exclude)
            continue;
        if (gl->es && section->ver_es_exclude &&
            gl->es >= section->ver_es_exclude)
            continue;

        bool exists = false, must_exist = false;
        if (ver_core)
            must_exist = version >= ver_core;

        if (section->extension)
            exists = gl_check_extension(gl->extensions, section->extension);

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
                if (must_exist) {
                    mp_err(log, "GL %d.%d function %s not found.\n",
                           MPGL_VER_GET_MAJOR(ver_core),
                           MPGL_VER_GET_MINOR(ver_core), fn->name);
                    goto error;
                } else {
                    mp_warn(log, "Function %s from extension %s not found.\n",
                            fn->name, section->extension);
                }
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
            if (!must_exist && section->extension)
                mp_verbose(log, "Loaded extension %s.\n", section->extension);
        }
    }

    gl->glsl_version = 0;
    if (gl->es) {
        if (gl->es >= 200)
            gl->glsl_version = 100;
        if (gl->es >= 300)
            gl->glsl_version = 300;
    } else {
        gl->glsl_version = 120;
        int glsl_major = 0, glsl_minor = 0;
        if (shader && sscanf(shader, "%d.%d", &glsl_major, &glsl_minor) == 2)
            gl->glsl_version = glsl_major * 100 + glsl_minor;
        // restrict GLSL version to be forwards compatible
        gl->glsl_version = MPMIN(gl->glsl_version, 440);
    }

    if (is_software_gl(gl)) {
        gl->mpgl_caps |= MPGL_CAP_SW;
        mp_verbose(log, "Detected suspected software renderer.\n");
    }

    // GL_ARB_compute_shader & GL_ARB_shader_image_load_store
    if (gl->DispatchCompute && gl->BindImageTexture)
        gl->mpgl_caps |= MPGL_CAP_COMPUTE_SHADER;

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
