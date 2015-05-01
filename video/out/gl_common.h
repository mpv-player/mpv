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

#ifndef MPLAYER_GL_COMMON_H
#define MPLAYER_GL_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "common/msg.h"
#include "misc/bstr.h"

#include "vo.h"
#include "video/csputils.h"

#include "video/mp_image.h"

#if HAVE_GL_COCOA
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED 1
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#define MP_GET_GL_WORKAROUNDS
#include "video/out/gl_header_fixes.h"

struct GL;
typedef struct GL GL;

enum {
    MPGL_CAP_ROW_LENGTH         = (1 << 4),     // GL_[UN]PACK_ROW_LENGTH
    MPGL_CAP_FB                 = (1 << 5),
    MPGL_CAP_VAO                = (1 << 6),
    MPGL_CAP_FLOAT_TEX          = (1 << 9),
    MPGL_CAP_TEX_RG             = (1 << 10),    // GL_ARB_texture_rg / GL 3.x
    MPGL_CAP_VDPAU              = (1 << 11),    // GL_NV_vdpau_interop
    MPGL_CAP_APPLE_RGB_422      = (1 << 12),    // GL_APPLE_rgb_422
    MPGL_CAP_1D_TEX             = (1 << 14),
    MPGL_CAP_3D_TEX             = (1 << 15),
    MPGL_CAP_DEBUG              = (1 << 16),
    MPGL_CAP_SW                 = (1 << 30),    // indirect or sw renderer
};

// E.g. 310 means 3.1
// Code doesn't have to use the macros; they are for convenience only.
#define MPGL_VER(major, minor) (((major) * 100) + (minor) * 10)
#define MPGL_VER_GET_MAJOR(ver) ((unsigned)(ver) / 100)
#define MPGL_VER_GET_MINOR(ver) ((unsigned)(ver) % 100 / 10)

#define MPGL_VER_P(ver) MPGL_VER_GET_MAJOR(ver), MPGL_VER_GET_MINOR(ver)

typedef struct MPGLContext {
    GL *gl;
    struct vo *vo;

    // Bit size of each component in the created framebuffer. 0 if unknown.
    int depth_r, depth_g, depth_b;

    // GL version requested from config_window_gl3 backend (MPGL_VER mangled).
    // (Might be different from the actual version in gl->version.)
    int requested_gl_version;

    void (*swapGlBuffers)(struct MPGLContext *);
    int (*vo_init)(struct vo *vo);
    void (*vo_uninit)(struct vo *vo);
    int (*vo_control)(struct vo *vo, int *events, int request, void *arg);
    void (*releaseGlContext)(struct MPGLContext *);
    void (*set_current)(struct MPGLContext *, bool current);

    // Used on windows only, tries to vsync with the DWM, and modifies SwapInterval
    // when it does so. Returns the possibly modified swapinterval value.
    int (*DwmFlush)(struct MPGLContext *, int opt_dwmflush,
                    int opt_swapinterval, int current_swapinterval);


    // Resize the window, or create a new window if there isn't one yet.
    // On the first call, it creates a GL context according to what's specified
    // in MPGLContext.requested_gl_version. This is just a hint, and if the
    // requested version is not available, it may return a completely different
    // GL context. (The caller must check if the created GL version is ok. The
    // callee must try to fall back to an older version if the requested
    // version is not available, and newer versions are incompatible.)
    bool (*config_window)(struct MPGLContext *ctx, int flags);

    // An optional function to register a resize callback in the backend that
    // can be called on separate thread to handle resize events immediately
    // (without waiting for vo_check_events, which will come later for the
    // proper resize)
    void (*register_resize_callback)(struct vo *vo,
                                     void (*cb)(struct vo *vo, int w, int h));

    // Optional callback on the beginning of a frame. The frame will be finished
    // with swapGlBuffers(). This returns false if use of the OpenGL context
    // should be avoided.
    bool (*start_frame)(struct MPGLContext *);

    // For free use by the backend.
    void *priv;
} MPGLContext;

void mpgl_lock(MPGLContext *ctx);
void mpgl_unlock(MPGLContext *ctx);

MPGLContext *mpgl_init(struct vo *vo, const char *backend_name, int vo_flags);
void mpgl_uninit(MPGLContext *ctx);
bool mpgl_reconfig_window(struct MPGLContext *ctx, int vo_flags);

int mpgl_find_backend(const char *name);

struct m_option;
int mpgl_validate_backend_opt(struct mp_log *log, const struct m_option *opt,
                              struct bstr name, struct bstr param);

void mpgl_set_backend_cocoa(MPGLContext *ctx);
void mpgl_set_backend_w32(MPGLContext *ctx);
void mpgl_set_backend_x11(MPGLContext *ctx);
void mpgl_set_backend_x11es(MPGLContext *ctx);
void mpgl_set_backend_x11egl(MPGLContext *ctx);
void mpgl_set_backend_x11egles(MPGLContext *ctx);
void mpgl_set_backend_wayland(MPGLContext *ctx);
void mpgl_set_backend_rpi(MPGLContext *ctx);

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
    void (GLAPIENTRY *Color4ub)(GLubyte, GLubyte, GLubyte, GLubyte);
    void (GLAPIENTRY *Color4f)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *ClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
    void (GLAPIENTRY *Enable)(GLenum);
    void (GLAPIENTRY *Disable)(GLenum);
    const GLubyte *(GLAPIENTRY * GetString)(GLenum);
    void (GLAPIENTRY *DrawBuffer)(GLenum);
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
    void (GLAPIENTRY *GetBooleanv)(GLenum, GLboolean *);
    void (GLAPIENTRY *ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum,
                                  GLenum, GLvoid *);
    void (GLAPIENTRY *ReadBuffer)(GLenum);
    void (GLAPIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    GLenum (GLAPIENTRY *GetError)(void);
    void (GLAPIENTRY *GetTexLevelParameteriv)(GLenum, GLint, GLenum, GLint *);

    void (GLAPIENTRY *GenBuffers)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteBuffers)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BindBuffer)(GLenum, GLuint);
    GLvoid * (GLAPIENTRY * MapBuffer)(GLenum, GLenum);
    GLboolean (GLAPIENTRY *UnmapBuffer)(GLenum);
    void (GLAPIENTRY *BufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
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
    const GLubyte* (GLAPIENTRY *GetStringi)(GLenum, GLuint);
    void (GLAPIENTRY *BindAttribLocation)(GLuint, GLuint, const GLchar *);
    void (GLAPIENTRY *BindFramebuffer)(GLenum, GLuint);
    void (GLAPIENTRY *GenFramebuffers)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteFramebuffers)(GLsizei, const GLuint *);
    GLenum (GLAPIENTRY *CheckFramebufferStatus)(GLenum);
    void (GLAPIENTRY *FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint,
                                            GLint);

    void (GLAPIENTRY *Uniform1f)(GLint, GLfloat);
    void (GLAPIENTRY *Uniform2f)(GLint, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform1i)(GLint, GLint);
    void (GLAPIENTRY *UniformMatrix2fv)(GLint, GLsizei, GLboolean,
                                        const GLfloat *);
    void (GLAPIENTRY *UniformMatrix3fv)(GLint, GLsizei, GLboolean,
                                        const GLfloat *);

    void (GLAPIENTRY *VDPAUInitNV)(const GLvoid *, const GLvoid *);
    void (GLAPIENTRY *VDPAUFiniNV)(void);
    GLvdpauSurfaceNV (GLAPIENTRY *VDPAURegisterOutputSurfaceNV)
        (GLvoid *, GLenum, GLsizei, const GLuint *);
    void (GLAPIENTRY *VDPAUUnregisterSurfaceNV)(GLvdpauSurfaceNV);
    void (GLAPIENTRY *VDPAUSurfaceAccessNV)(GLvdpauSurfaceNV, GLenum);
    void (GLAPIENTRY *VDPAUMapSurfacesNV)(GLsizei, const GLvdpauSurfaceNV *);
    void (GLAPIENTRY *VDPAUUnmapSurfacesNV)(GLsizei, const GLvdpauSurfaceNV *);

    GLint (GLAPIENTRY *GetVideoSync)(GLuint *);
    GLint (GLAPIENTRY *WaitVideoSync)(GLint, GLint, unsigned int *);

    void (GLAPIENTRY *DebugMessageCallback)(MP_GLDEBUGPROC callback,
                                            const void *userParam);
};

#endif /* MPLAYER_GL_COMMON_H */
