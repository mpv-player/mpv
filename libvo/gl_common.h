/*
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

#ifndef MPLAYER_GL_COMMON_H
#define MPLAYER_GL_COMMON_H

#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "mp_msg.h"

#include "video_out.h"
#include "csputils.h"

#include <GL/gl.h>
#include <GL/glext.h>

#include "libvo/gl_header_fixes.h"

struct GL;
typedef struct GL GL;

void glAdjustAlignment(GL *gl, int stride);

int glFindFormat(uint32_t format, int have_texture_rg, int *bpp,
                 GLint *gl_texfmt, GLenum *gl_format, GLenum *gl_type);
int glFmt2bpp(GLenum format, GLenum type);
void glCreateClearTex(GL *gl, GLenum target, GLenum fmt, GLenum format,
                      GLenum type, GLint filter, int w, int h,
                      unsigned char val);
int glCreatePPMTex(GL *gl, GLenum target, GLenum fmt, GLint filter,
                   FILE *f, int *width, int *height, int *maxval);
void glUploadTex(GL *gl, GLenum target, GLenum format, GLenum type,
                 const void *dataptr, int stride,
                 int x, int y, int w, int h, int slice);
void glDownloadTex(GL *gl, GLenum target, GLenum format, GLenum type,
                   void *dataptr, int stride);
void glDrawTex(GL *gl, GLfloat x, GLfloat y, GLfloat w, GLfloat h,
               GLfloat tx, GLfloat ty, GLfloat tw, GLfloat th,
               int sx, int sy, int rect_tex, int is_yv12, int flip);
int loadGPUProgram(GL *gl, GLenum target, char *prog);
void glCheckError(GL *gl, const char *info);

/** \addtogroup glconversion
 * \{ */
//! do not use YUV conversion, this should always stay 0
#define YUV_CONVERSION_NONE 0
//! use nVidia specific register combiners for YUV conversion
//! implementation has been removed
#define YUV_CONVERSION_COMBINERS 1
//! use a fragment program for YUV conversion
#define YUV_CONVERSION_FRAGMENT 2
//! use a fragment program for YUV conversion with gamma using POW
#define YUV_CONVERSION_FRAGMENT_POW 3
//! use a fragment program with additional table lookup for YUV conversion
#define YUV_CONVERSION_FRAGMENT_LOOKUP 4
//! use ATI specific register combiners ("fragment program")
#define YUV_CONVERSION_COMBINERS_ATI 5
//! use a fragment program with 3D table lookup for YUV conversion
#define YUV_CONVERSION_FRAGMENT_LOOKUP3D 6
//! use ATI specific "text" register combiners ("fragment program")
#define YUV_CONVERSION_TEXT_FRAGMENT 7
//! use normal bilinear scaling for textures
#define YUV_SCALER_BILIN 0
//! use higher quality bicubic scaling for textures
#define YUV_SCALER_BICUB 1
//! use cubic scaling in X and normal linear scaling in Y direction
#define YUV_SCALER_BICUB_X 2
//! use cubic scaling without additional lookup texture
#define YUV_SCALER_BICUB_NOTEX 3
#define YUV_SCALER_UNSHARP 4
#define YUV_SCALER_UNSHARP2 5
//! mask for conversion type
#define YUV_CONVERSION_MASK 0xF
//! mask for scaler type
#define YUV_SCALER_MASK 0xF
//! shift value for luminance scaler type
#define YUV_LUM_SCALER_SHIFT 8
//! shift value for chrominance scaler type
#define YUV_CHROM_SCALER_SHIFT 12
//! extract conversion out of type
#define YUV_CONVERSION(t) ((t) & YUV_CONVERSION_MASK)
//! extract luminance scaler out of type
#define YUV_LUM_SCALER(t) (((t) >> YUV_LUM_SCALER_SHIFT) & YUV_SCALER_MASK)
//! extract chrominance scaler out of type
#define YUV_CHROM_SCALER(t) (((t) >> YUV_CHROM_SCALER_SHIFT) & YUV_SCALER_MASK)
#define SET_YUV_CONVERSION(c)   ((c) & YUV_CONVERSION_MASK)
#define SET_YUV_LUM_SCALER(s)   (((s) & YUV_SCALER_MASK) << YUV_LUM_SCALER_SHIFT)
#define SET_YUV_CHROM_SCALER(s) (((s) & YUV_SCALER_MASK) << YUV_CHROM_SCALER_SHIFT)
//! returns whether the yuv conversion supports large brightness range etc.
static inline int glYUVLargeRange(int conv)
{
    switch (conv) {
    case YUV_CONVERSION_NONE:
    case YUV_CONVERSION_COMBINERS_ATI:
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_TEXT_FRAGMENT:
        return 0;
    }
    return 1;
}
/** \} */

typedef struct {
    GLenum target;
    int type;
    struct mp_csp_params csp_params;
    int texw;
    int texh;
    int chrom_texw;
    int chrom_texh;
    float filter_strength;
    float noise_strength;
} gl_conversion_params_t;

int glAutodetectYUVConversion(GL *gl);
void glSetupYUVConversion(GL *gl, gl_conversion_params_t *params);
void glEnableYUVConversion(GL *gl, GLenum target, int type);
void glDisableYUVConversion(GL *gl, GLenum target, int type);

#define GL_3D_RED_CYAN        1
#define GL_3D_GREEN_MAGENTA   2
#define GL_3D_QUADBUFFER      3

void glEnable3DLeft(GL *gl, int type);
void glEnable3DRight(GL *gl, int type);
void glDisable3D(GL *gl, int type);

/** \addtogroup glcontext
 * \{ */
//! could not set new window, will continue drawing into the old one.
#define SET_WINDOW_FAILED -1
//! new window is set, could even transfer the OpenGL context.
#define SET_WINDOW_OK 0
//! new window is set, but the OpenGL context needs to be reinitialized.
#define SET_WINDOW_REINIT 1
/** \} */

enum MPGLType {
    GLTYPE_AUTO,
    GLTYPE_COCOA,
    GLTYPE_W32,
    GLTYPE_X11,
    GLTYPE_SDL,
};

enum {
    MPGLFLAG_DEBUG = 1,
};

#define MPGL_VER(major, minor) (((major) << 16) | (minor))
#define MPGL_VER_GET_MAJOR(ver) ((ver) >> 16)
#define MPGL_VER_GET_MINOR(ver) ((ver) & ((1 << 16) - 1))

typedef struct MPGLContext {
    GL *gl;
    enum MPGLType type;
    struct vo *vo;
    void *priv;
    // Bit size of each component in the created framebuffer. 0 if unknown.
    int depth_r, depth_g, depth_b;
    int (*create_window)(struct MPGLContext *ctx, uint32_t d_width,
                         uint32_t d_height, uint32_t flags);
    int (*setGlWindow)(struct MPGLContext *);
    void (*releaseGlContext)(struct MPGLContext *);
    void (*swapGlBuffers)(struct MPGLContext *);
    int (*check_events)(struct vo *vo);
    void (*fullscreen)(struct vo *vo);
    void (*vo_uninit)(struct vo *vo);
    // only available if GL3 context creation is supported
    // gl_flags: bitfield of MPGLFLAG_* constants
    // gl_version: requested OpenGL version number (use MPGL_VER())
    // return value is one of the SET_WINDOW_* constants
    int (*create_window_gl3)(struct MPGLContext *ctx, int gl_flags,
                             int gl_version, uint32_t d_width,
                             uint32_t d_height, uint32_t flags);
    // optional
    void (*ontop)(struct vo *vo);
    void (*border)(struct vo *vo);
    void (*update_xinerama_info)(struct vo *vo);
} MPGLContext;

int mpgl_find_backend(const char *name);

MPGLContext *init_mpglcontext(enum MPGLType type, struct vo *vo);
void uninit_mpglcontext(MPGLContext *ctx);

// calls create_window_gl3 or create_window+setGlWindow
int create_mpglcontext(struct MPGLContext *ctx, int gl_flags, int gl_version,
                       uint32_t d_width, uint32_t d_height, uint32_t flags);

// print a multi line string with line numbers (e.g. for shader sources)
// mod, lev: module and log level, as in mp_msg()
void mp_log_source(int mod, int lev, const char *src);

//function pointers loaded from the OpenGL library
struct GL {
    void (GLAPIENTRY *Begin)(GLenum);
    void (GLAPIENTRY *End)(void);
    void (GLAPIENTRY *Viewport)(GLint, GLint, GLsizei, GLsizei);
    void (GLAPIENTRY *MatrixMode)(GLenum);
    void (GLAPIENTRY *LoadIdentity)(void);
    void (GLAPIENTRY *Translated)(double, double, double);
    void (GLAPIENTRY *Scaled)(double, double, double);
    void (GLAPIENTRY *Ortho)(double, double, double, double, double,double);
    void (GLAPIENTRY *PushMatrix)(void);
    void (GLAPIENTRY *PopMatrix)(void);
    void (GLAPIENTRY *Clear)(GLbitfield);
    GLuint (GLAPIENTRY *GenLists)(GLsizei);
    void (GLAPIENTRY *DeleteLists)(GLuint, GLsizei);
    void (GLAPIENTRY *NewList)(GLuint, GLenum);
    void (GLAPIENTRY *EndList)(void);
    void (GLAPIENTRY *CallList)(GLuint);
    void (GLAPIENTRY *CallLists)(GLsizei, GLenum, const GLvoid *);
    void (GLAPIENTRY *GenTextures)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteTextures)(GLsizei, const GLuint *);
    void (GLAPIENTRY *TexEnvi)(GLenum, GLenum, GLint);
    void (GLAPIENTRY *Color4ub)(GLubyte, GLubyte, GLubyte, GLubyte);
    void (GLAPIENTRY *Color4f)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *ClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
    void (GLAPIENTRY *Enable)(GLenum);
    void (GLAPIENTRY *Disable)(GLenum);
    const GLubyte *(GLAPIENTRY * GetString)(GLenum);
    void (GLAPIENTRY *DrawBuffer)(GLenum);
    void (GLAPIENTRY *DepthMask)(GLboolean);
    void (GLAPIENTRY *BlendFunc)(GLenum, GLenum);
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
    void (GLAPIENTRY *GetTexImage)(GLenum, GLint, GLenum, GLenum, GLvoid *);
    void (GLAPIENTRY *TexParameteri)(GLenum, GLenum, GLint);
    void (GLAPIENTRY *TexParameterf)(GLenum, GLenum, GLfloat);
    void (GLAPIENTRY *TexParameterfv)(GLenum, GLenum, const GLfloat *);
    void (GLAPIENTRY *TexCoord2f)(GLfloat, GLfloat);
    void (GLAPIENTRY *Vertex2f)(GLfloat, GLfloat);
    void (GLAPIENTRY *GetIntegerv)(GLenum, GLint *);
    void (GLAPIENTRY *GetBooleanv)(GLenum, GLboolean *);
    void (GLAPIENTRY *ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (GLAPIENTRY *ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum,
                                  GLenum, GLvoid *);
    void (GLAPIENTRY *ReadBuffer)(GLenum);
    void (GLAPIENTRY *VertexPointer)(GLint, GLenum, GLsizei, const GLvoid *);
    void (GLAPIENTRY *ColorPointer)(GLint, GLenum, GLsizei, const GLvoid *);
    void (GLAPIENTRY *TexCoordPointer)(GLint, GLenum, GLsizei, const GLvoid *);
    void (GLAPIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    void (GLAPIENTRY *EnableClientState)(GLenum);
    void (GLAPIENTRY *DisableClientState)(GLenum);
    GLenum (GLAPIENTRY *GetError)(void);


    // OpenGL extension functions
    void (GLAPIENTRY *GenBuffers)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteBuffers)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BindBuffer)(GLenum, GLuint);
    GLvoid * (GLAPIENTRY * MapBuffer)(GLenum, GLenum);
    GLboolean (GLAPIENTRY *UnmapBuffer)(GLenum);
    void (GLAPIENTRY *BufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
    void (GLAPIENTRY *ActiveTexture)(GLenum);
    void (GLAPIENTRY *BindTexture)(GLenum, GLuint);
    void (GLAPIENTRY *MultiTexCoord2f)(GLenum, GLfloat, GLfloat);
    void (GLAPIENTRY *GenPrograms)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeletePrograms)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BindProgram)(GLenum, GLuint);
    void (GLAPIENTRY *ProgramString)(GLenum, GLenum, GLsizei, const GLvoid *);
    void (GLAPIENTRY *GetProgramivARB)(GLenum, GLenum, GLint *);
    void (GLAPIENTRY *ProgramEnvParameter4f)(GLenum, GLuint, GLfloat, GLfloat,
                                             GLfloat, GLfloat);
    int (GLAPIENTRY *SwapInterval)(int);
    void (GLAPIENTRY *TexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei,
                                  GLsizei, GLint, GLenum, GLenum,
                                  const GLvoid *);

    // ancient ATI extensions
    void (GLAPIENTRY *BeginFragmentShader)(void);
    void (GLAPIENTRY *EndFragmentShader)(void);
    void (GLAPIENTRY *SampleMap)(GLuint, GLuint, GLenum);
    void (GLAPIENTRY *ColorFragmentOp2)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                        GLuint, GLuint, GLuint, GLuint, GLuint);
    void (GLAPIENTRY *ColorFragmentOp3)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                        GLuint, GLuint, GLuint, GLuint, GLuint,
                                        GLuint, GLuint, GLuint);
    void (GLAPIENTRY *SetFragmentShaderConstant)(GLuint, const GLfloat *);


    // GL 3, possibly in GL 2.x as well in form of extensions
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
    void (GLAPIENTRY *Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Uniform1i)(GLint, GLint);
    void (GLAPIENTRY *UniformMatrix3fv)(GLint, GLsizei, GLboolean,
                                        const GLfloat *);
    void (GLAPIENTRY *UniformMatrix4x3fv)(GLint, GLsizei, GLboolean,
                                          const GLfloat *);
};

#endif /* MPLAYER_GL_COMMON_H */
