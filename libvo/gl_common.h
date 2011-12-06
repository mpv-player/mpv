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

#ifdef CONFIG_GL_WIN32
#include <windows.h>
#include "w32_common.h"
#endif
#ifdef CONFIG_GL_X11
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "x11_common.h"
// This old-vo wrapper macro would conflict with the struct member
#undef update_xinerama_info
#endif
#include <GL/gl.h>

// workaround for some gl.h headers
#ifndef GLAPIENTRY
#ifdef APIENTRY
#define GLAPIENTRY APIENTRY
#elif defined(CONFIG_GL_WIN32)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
#endif

/**
 * \defgroup glextdefines OpenGL extension defines
 *
 * conditionally define all extension defines used.
 * vendor specific extensions should be marked as such
 * (e.g. _NV), _ARB is not used to ease readability.
 * \{
 */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif
#ifndef GL_TEXT_FRAGMENT_SHADER_ATI
#define GL_TEXT_FRAGMENT_SHADER_ATI 0x8200
#endif
#ifndef GL_FRAGMENT_SHADER_ATI
#define GL_FRAGMENT_SHADER_ATI 0x8920
#endif
#ifndef GL_NUM_FRAGMENT_REGISTERS_ATI
#define GL_NUM_FRAGMENT_REGISTERS_ATI 0x896E
#endif
#ifndef GL_REG_0_ATI
#define GL_REG_0_ATI 0x8921
#endif
#ifndef GL_REG_1_ATI
#define GL_REG_1_ATI 0x8922
#endif
#ifndef GL_REG_2_ATI
#define GL_REG_2_ATI 0x8923
#endif
#ifndef GL_CON_0_ATI
#define GL_CON_0_ATI 0x8941
#endif
#ifndef GL_CON_1_ATI
#define GL_CON_1_ATI 0x8942
#endif
#ifndef GL_CON_2_ATI
#define GL_CON_2_ATI 0x8943
#endif
#ifndef GL_CON_3_ATI
#define GL_CON_3_ATI 0x8944
#endif
#ifndef GL_ADD_ATI
#define GL_ADD_ATI 0x8963
#endif
#ifndef GL_MUL_ATI
#define GL_MUL_ATI 0x8964
#endif
#ifndef GL_MAD_ATI
#define GL_MAD_ATI 0x8968
#endif
#ifndef GL_SWIZZLE_STR_ATI
#define GL_SWIZZLE_STR_ATI 0x8976
#endif
#ifndef GL_4X_BIT_ATI
#define GL_4X_BIT_ATI 2
#endif
#ifndef GL_8X_BIT_ATI
#define GL_8X_BIT_ATI 4
#endif
#ifndef GL_BIAS_BIT_ATI
#define GL_BIAS_BIT_ATI 8
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_BYTE_3_3_2
#define GL_UNSIGNED_BYTE_3_3_2 0x8032
#endif
#ifndef GL_UNSIGNED_BYTE_2_3_3_REV
#define GL_UNSIGNED_BYTE_2_3_3_REV 0x8362
#endif
#ifndef GL_UNSIGNED_SHORT_4_4_4_4
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#endif
#ifndef GL_UNSIGNED_SHORT_4_4_4_4_REV
#define GL_UNSIGNED_SHORT_4_4_4_4_REV 0x8365
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#endif
#ifndef GL_UNSIGNED_INT_10_10_10_2
#define GL_UNSIGNED_INT_10_10_10_2 0x8036
#endif
#ifndef GL_UNSIGNED_INT_2_10_10_10_REV
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#endif
#ifndef GL_UNSIGNED_SHORT_5_5_5_1
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#endif
#ifndef GL_UNSIGNED_SHORT_8_8
#define GL_UNSIGNED_SHORT_8_8 0x85BA
#endif
#ifndef GL_UNSIGNED_SHORT_8_8_REV
#define GL_UNSIGNED_SHORT_8_8_REV 0x85BB
#endif
#ifndef GL_YCBCR_MESA
#define GL_YCBCR_MESA 0x8757
#endif
#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif
#ifndef GL_FLOAT_RGB32_NV
#define GL_FLOAT_RGB32_NV 0x8889
#endif
#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16 0x8042
#endif
#ifndef GL_R16
#define GL_R16 0x822A
#endif
#ifndef GL_UNPACK_CLIENT_STORAGE_APPLE
#define GL_UNPACK_CLIENT_STORAGE_APPLE 0x85B2
#endif
#ifndef GL_FRAGMENT_PROGRAM
#define GL_FRAGMENT_PROGRAM 0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII
#define GL_PROGRAM_FORMAT_ASCII 0x8875
#endif
#ifndef GL_PROGRAM_ERROR_POSITION
#define GL_PROGRAM_ERROR_POSITION 0x864B
#endif
#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#endif
#ifndef GL_PROGRAM_ERROR_STRING
#define GL_PROGRAM_ERROR_STRING 0x8874
#endif
/** \} */ // end of glextdefines group

struct GL;
typedef struct GL GL;

void glAdjustAlignment(GL *gl, int stride);

const char *glValName(GLint value);

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

typedef struct MPGLContext {
    GL *gl;
    enum MPGLType type;
    struct vo *vo;
    union {
        int w32;
#ifdef CONFIG_GL_X11
        XVisualInfo *x11;
#endif
    } vinfo;
    union {
#ifdef CONFIG_GL_WIN32
        HGLRC w32;
#endif
#ifdef CONFIG_GL_X11
        GLXContext x11;
#endif
    } context;
    int (*create_window)(struct MPGLContext *ctx, uint32_t d_width,
                         uint32_t d_height, uint32_t flags);
    int (*setGlWindow)(struct MPGLContext *);
    void (*releaseGlContext)(struct MPGLContext *);
    void (*swapGlBuffers)(struct MPGLContext *);
    void (*update_xinerama_info)(struct vo *vo);
    void (*border)(struct vo *vo);
    int (*check_events)(struct vo *vo);
    void (*fullscreen)(struct vo *vo);
    void (*ontop)(struct vo *vo);
} MPGLContext;

MPGLContext *init_mpglcontext(enum MPGLType type, struct vo *vo);
void uninit_mpglcontext(MPGLContext *ctx);

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
    void (GLAPIENTRY *Frustum)(double, double, double, double, double, double);
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
    void (GLAPIENTRY *TexEnvf)(GLenum, GLenum, GLfloat);
    void (GLAPIENTRY *TexEnvi)(GLenum, GLenum, GLint);
    void (GLAPIENTRY *Color4ub)(GLubyte, GLubyte, GLubyte, GLubyte);
    void (GLAPIENTRY *Color3f)(GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Color4f)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *ClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
    void (GLAPIENTRY *ClearDepth)(GLclampd);
    void (GLAPIENTRY *DepthFunc)(GLenum);
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
    void (GLAPIENTRY *Vertex3f)(GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Normal3f)(GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Lightfv)(GLenum, GLenum, const GLfloat *);
    void (GLAPIENTRY *ColorMaterial)(GLenum, GLenum);
    void (GLAPIENTRY *ShadeModel)(GLenum);
    void (GLAPIENTRY *GetIntegerv)(GLenum, GLint *);
    void (GLAPIENTRY *ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (GLAPIENTRY *ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum,
                                  GLenum, GLvoid *);
    void (GLAPIENTRY *ReadBuffer)(GLenum);

    // OpenGL extension functions
    void (GLAPIENTRY *GenBuffers)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeleteBuffers)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BindBuffer)(GLenum, GLuint);
    GLvoid * (GLAPIENTRY * MapBuffer)(GLenum, GLenum);
    GLboolean (GLAPIENTRY *UnmapBuffer)(GLenum);
    void (GLAPIENTRY *BufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
    void (GLAPIENTRY *BeginFragmentShader)(void);
    void (GLAPIENTRY *EndFragmentShader)(void);
    void (GLAPIENTRY *SampleMap)(GLuint, GLuint, GLenum);
    void (GLAPIENTRY *ColorFragmentOp2)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                        GLuint, GLuint, GLuint, GLuint, GLuint);
    void (GLAPIENTRY *ColorFragmentOp3)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                        GLuint, GLuint, GLuint, GLuint, GLuint,
                                        GLuint, GLuint, GLuint);
    void (GLAPIENTRY *SetFragmentShaderConstant)(GLuint, const GLfloat *);
    void (GLAPIENTRY *ActiveTexture)(GLenum);
    void (GLAPIENTRY *BindTexture)(GLenum, GLuint);
    void (GLAPIENTRY *MultiTexCoord2f)(GLenum, GLfloat, GLfloat);
    void (GLAPIENTRY *GenPrograms)(GLsizei, GLuint *);
    void (GLAPIENTRY *DeletePrograms)(GLsizei, const GLuint *);
    void (GLAPIENTRY *BindProgram)(GLenum, GLuint);
    void (GLAPIENTRY *ProgramString)(GLenum, GLenum, GLsizei, const GLvoid *);
    void (GLAPIENTRY *GetProgramiv)(GLenum, GLenum, GLint *);
    void (GLAPIENTRY *ProgramEnvParameter4f)(GLenum, GLuint, GLfloat, GLfloat,
                                             GLfloat, GLfloat);
    int (GLAPIENTRY *SwapInterval)(int);
    void (GLAPIENTRY *TexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei,
                                  GLsizei, GLint, GLenum, GLenum,
                                  const GLvoid *);
};

#endif /* MPLAYER_GL_COMMON_H */
