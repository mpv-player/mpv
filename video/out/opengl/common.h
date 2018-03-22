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

#ifndef MPLAYER_GL_COMMON_H
#define MPLAYER_GL_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "common/msg.h"
#include "misc/bstr.h"

#include "video/csputils.h"
#include "video/mp_image.h"
#include "video/out/vo.h"
#include "video/out/gpu/ra.h"

#include "gl_headers.h"

#if HAVE_GL_WIN32
#include <windows.h>
#endif

struct GL;
typedef struct GL GL;

enum {
    MPGL_CAP_ROW_LENGTH         = (1 << 4),     // GL_[UN]PACK_ROW_LENGTH
    MPGL_CAP_FB                 = (1 << 5),
    MPGL_CAP_VAO                = (1 << 6),
    MPGL_CAP_TEX_RG             = (1 << 10),    // GL_ARB_texture_rg / GL 3.x
    MPGL_CAP_VDPAU              = (1 << 11),    // GL_NV_vdpau_interop
    MPGL_CAP_APPLE_RGB_422      = (1 << 12),    // GL_APPLE_rgb_422
    MPGL_CAP_1D_TEX             = (1 << 14),
    MPGL_CAP_3D_TEX             = (1 << 15),
    MPGL_CAP_DEBUG              = (1 << 16),
    MPGL_CAP_DXINTEROP          = (1 << 17),    // WGL_NV_DX_interop
    MPGL_CAP_EXT16              = (1 << 18),    // GL_EXT_texture_norm16
    MPGL_CAP_ARB_FLOAT          = (1 << 19),    // GL_ARB_texture_float
    MPGL_CAP_EXT_CR_HFLOAT      = (1 << 20),    // GL_EXT_color_buffer_half_float
    MPGL_CAP_UBO                = (1 << 21),    // GL_ARB_uniform_buffer_object
    MPGL_CAP_SSBO               = (1 << 22),    // GL_ARB_shader_storage_buffer_object
    MPGL_CAP_COMPUTE_SHADER     = (1 << 23),    // GL_ARB_compute_shader & GL_ARB_shader_image_load_store
    MPGL_CAP_NESTED_ARRAY       = (1 << 24),    // GL_ARB_arrays_of_arrays

    MPGL_CAP_SW                 = (1 << 30),    // indirect or sw renderer
};

// E.g. 310 means 3.1
// Code doesn't have to use the macros; they are for convenience only.
#define MPGL_VER(major, minor) (((major) * 100) + (minor) * 10)
#define MPGL_VER_GET_MAJOR(ver) ((unsigned)(ver) / 100)
#define MPGL_VER_GET_MINOR(ver) ((unsigned)(ver) % 100 / 10)

#define MPGL_VER_P(ver) MPGL_VER_GET_MAJOR(ver), MPGL_VER_GET_MINOR(ver)

void mpgl_load_functions(GL *gl, void *(*getProcAddress)(const GLubyte *),
                         const char *ext2, struct mp_log *log);
void mpgl_load_functions2(GL *gl, void *(*get_fn)(void *ctx, const char *n),
                          void *fn_ctx, const char *ext2, struct mp_log *log);

typedef void (GLAPIENTRY *MP_GLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum,
                                          GLsizei, const GLchar *,const void *);

//function pointers loaded from the OpenGL library
struct GL {
    int version;                // MPGL_VER() mangled (e.g. 210 for 2.1)
    int es;                     // es version (e.g. 300), 0 for desktop GL
    int glsl_version;           // e.g. 130 for GLSL 1.30
    char *extensions;           // Equivalent to GL_EXTENSIONS
    int mpgl_caps;              // Bitfield of MPGL_CAP_* constants
    bool debug_context;         // use of e.g. GLX_CONTEXT_DEBUG_BIT_ARB

    void (GLAPIENTRY *Viewport)(GLint, GLint, GLsizei, GLsizei);
    void (GLAPIENTRY *Clear)(GLbitfield);
    void (GLAPIENTRY *GenTextures)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteTextures)(GLsizei, const GLuint *);
    void (GLAPIENTRY *ClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
    void (GLAPIENTRY *Enable)(GLenum);
    void (GLAPIENTRY *Disable)(GLenum);
    const GLubyte *(GLAPIENTRY * GetString)(GLenum);
    void (GLAPIENTRY *BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (GLAPIENTRY *Flush)(void);
    void (GLAPIENTRY *Finish)(void);
    void (GLAPIENTRY *PixelStorei)(GLenum, GLint);
    void (GLAPIENTRY *TexImage1D)(GLenum, GLint, GLint, GLsizei, GLint,
                                  GLenum, GLenum, const GLvoid *);
    void (GLAPIENTRY *TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei,
                                  GLint, GLenum, GLenum, const GLvoid *);
    void (GLAPIENTRY *TexSubImage2D)(GLenum, GLint, GLint, GLint,
                                     GLsizei, GLsizei, GLenum, GLenum,
                                     const GLvoid *);
    void (GLAPIENTRY *TexParameteri)(GLenum, GLenum, GLint);
    void (GLAPIENTRY *GetIntegerv)(GLenum, GLint *);
    void (GLAPIENTRY *ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum,
                                  GLenum, GLvoid *);
    void (GLAPIENTRY *ReadBuffer)(GLenum);
    void (GLAPIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    GLenum (GLAPIENTRY *GetError)(void);
    void (GLAPIENTRY *GetTexLevelParameteriv)(GLenum, GLint, GLenum, GLint *);
    void (GLAPIENTRY *Scissor)(GLint, GLint, GLsizei, GLsizei);

    void (GLAPIENTRY *GenBuffers)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteBuffers)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BindBuffer)(GLenum, GLuint);
    void (GLAPIENTRY *BindBufferBase)(GLenum, GLuint, GLuint);
    GLvoid * (GLAPIENTRY *MapBufferRange)(GLenum, GLintptr, GLsizeiptr,
                                          GLbitfield);
    GLboolean (GLAPIENTRY *UnmapBuffer)(GLenum);
    void (GLAPIENTRY *BufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
    void (GLAPIENTRY *BufferSubData)(GLenum, GLintptr, GLsizeiptr, const GLvoid *);
    void (GLAPIENTRY *ActiveTexture)(GLenum);
    void (GLAPIENTRY *BindTexture)(GLenum, GLuint);
    int (GLAPIENTRY *SwapInterval)(int);
    void (GLAPIENTRY *TexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei,
                                  GLsizei, GLint, GLenum, GLenum,
                                  const GLvoid *);

    void (GLAPIENTRY *GenVertexArrays)(GLsizei, GLuint *);
    void (GLAPIENTRY *BindVertexArray)(GLuint);
    GLint (GLAPIENTRY *GetAttribLocation)(GLuint, const GLchar *);
    void (GLAPIENTRY *EnableVertexAttribArray)(GLuint);
    void (GLAPIENTRY *DisableVertexAttribArray)(GLuint);
    void (GLAPIENTRY *VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean,
                                           GLsizei, const GLvoid *);
    void (GLAPIENTRY *DeleteVertexArrays)(GLsizei, const GLuint *);
    void (GLAPIENTRY *UseProgram)(GLuint);
    GLint (GLAPIENTRY *GetUniformLocation)(GLuint, const GLchar *);
    void (GLAPIENTRY *CompileShader)(GLuint);
    GLuint (GLAPIENTRY *CreateProgram)(void);
    GLuint (GLAPIENTRY *CreateShader)(GLenum);
    void (GLAPIENTRY *ShaderSource)(GLuint, GLsizei, const GLchar **,
                                    const GLint *);
    void (GLAPIENTRY *LinkProgram)(GLuint);
    void (GLAPIENTRY *AttachShader)(GLuint, GLuint);
    void (GLAPIENTRY *DeleteShader)(GLuint);
    void (GLAPIENTRY *DeleteProgram)(GLuint);
    void (GLAPIENTRY *GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (GLAPIENTRY *GetShaderiv)(GLuint, GLenum, GLint *);
    void (GLAPIENTRY *GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (GLAPIENTRY *GetProgramiv)(GLenum, GLenum, GLint *);
    void (GLAPIENTRY *GetProgramBinary)(GLuint, GLsizei, GLsizei *, GLenum *,
                                        void *);
    void (GLAPIENTRY *ProgramBinary)(GLuint, GLenum, const void *, GLsizei);

    void (GLAPIENTRY *DispatchCompute)(GLuint, GLuint, GLuint);
    void (GLAPIENTRY *BindImageTexture)(GLuint, GLuint, GLint, GLboolean,
                                        GLint, GLenum, GLenum);
    void (GLAPIENTRY *MemoryBarrier)(GLbitfield);

    const GLubyte* (GLAPIENTRY *GetStringi)(GLenum, GLuint);
    void (GLAPIENTRY *BindAttribLocation)(GLuint, GLuint, const GLchar *);
    void (GLAPIENTRY *BindFramebuffer)(GLenum, GLuint);
    void (GLAPIENTRY *GenFramebuffers)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteFramebuffers)(GLsizei, const GLuint *);
    GLenum (GLAPIENTRY *CheckFramebufferStatus)(GLenum);
    void (GLAPIENTRY *FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint,
                                            GLint);
    void (GLAPIENTRY *BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint,
                                       GLint, GLint, GLbitfield, GLenum);
    void (GLAPIENTRY *GetFramebufferAttachmentParameteriv)(GLenum, GLenum,
                                                           GLenum, GLint *);

    void (GLAPIENTRY *Uniform1f)(GLint, GLfloat);
    void (GLAPIENTRY *Uniform2f)(GLint, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform1i)(GLint, GLint);
    void (GLAPIENTRY *UniformMatrix2fv)(GLint, GLsizei, GLboolean,
                                        const GLfloat *);
    void (GLAPIENTRY *UniformMatrix3fv)(GLint, GLsizei, GLboolean,
                                        const GLfloat *);

    void (GLAPIENTRY *InvalidateTexImage)(GLuint, GLint);
    void (GLAPIENTRY *InvalidateFramebuffer)(GLenum, GLsizei, const GLenum *);

    GLsync (GLAPIENTRY *FenceSync)(GLenum, GLbitfield);
    GLenum (GLAPIENTRY *ClientWaitSync)(GLsync, GLbitfield, GLuint64);
    void (GLAPIENTRY *DeleteSync)(GLsync sync);

    void (GLAPIENTRY *BufferStorage)(GLenum, intptr_t, const GLvoid *, GLenum);

    void (GLAPIENTRY *GenQueries)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteQueries)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BeginQuery)(GLenum,  GLuint);
    void (GLAPIENTRY *EndQuery)(GLenum);
    void (GLAPIENTRY *QueryCounter)(GLuint, GLenum);
    GLboolean (GLAPIENTRY *IsQuery)(GLuint);
    void (GLAPIENTRY *GetQueryObjectiv)(GLuint, GLenum, GLint *);
    void (GLAPIENTRY *GetQueryObjecti64v)(GLuint, GLenum, GLint64 *);
    void (GLAPIENTRY *GetQueryObjectuiv)(GLuint, GLenum, GLuint *);
    void (GLAPIENTRY *GetQueryObjectui64v)(GLuint, GLenum, GLuint64 *);

    void (GLAPIENTRY *VDPAUInitNV)(const GLvoid *, const GLvoid *);
    void (GLAPIENTRY *VDPAUFiniNV)(void);
    GLvdpauSurfaceNV (GLAPIENTRY *VDPAURegisterOutputSurfaceNV)
        (GLvoid *, GLenum, GLsizei, const GLuint *);
    GLvdpauSurfaceNV (GLAPIENTRY *VDPAURegisterVideoSurfaceNV)
        (GLvoid *, GLenum, GLsizei, const GLuint *);
    void (GLAPIENTRY *VDPAUUnregisterSurfaceNV)(GLvdpauSurfaceNV);
    void (GLAPIENTRY *VDPAUSurfaceAccessNV)(GLvdpauSurfaceNV, GLenum);
    void (GLAPIENTRY *VDPAUMapSurfacesNV)(GLsizei, const GLvdpauSurfaceNV *);
    void (GLAPIENTRY *VDPAUUnmapSurfacesNV)(GLsizei, const GLvdpauSurfaceNV *);

#if HAVE_GL_WIN32
    // The HANDLE type might not be present on non-Win32
    BOOL (GLAPIENTRY *DXSetResourceShareHandleNV)(void *dxObject,
        HANDLE shareHandle);
    HANDLE (GLAPIENTRY *DXOpenDeviceNV)(void *dxDevice);
    BOOL (GLAPIENTRY *DXCloseDeviceNV)(HANDLE hDevice);
    HANDLE (GLAPIENTRY *DXRegisterObjectNV)(HANDLE hDevice, void *dxObject,
        GLuint name, GLenum type, GLenum access);
    BOOL (GLAPIENTRY *DXUnregisterObjectNV)(HANDLE hDevice, HANDLE hObject);
    BOOL (GLAPIENTRY *DXLockObjectsNV)(HANDLE hDevice, GLint count,
        HANDLE *hObjects);
    BOOL (GLAPIENTRY *DXUnlockObjectsNV)(HANDLE hDevice, GLint count,
        HANDLE *hObjects);
#endif

    GLint (GLAPIENTRY *GetVideoSync)(GLuint *);
    GLint (GLAPIENTRY *WaitVideoSync)(GLint, GLint, unsigned int *);

    void (GLAPIENTRY *GetTranslatedShaderSourceANGLE)(GLuint, GLsizei,
                                                      GLsizei*, GLchar* source);

    void (GLAPIENTRY *DebugMessageCallback)(MP_GLDEBUGPROC callback,
                                            const void *userParam);

    void *(GLAPIENTRY *MPGetNativeDisplay)(const char *name);
};

#endif /* MPLAYER_GL_COMMON_H */
