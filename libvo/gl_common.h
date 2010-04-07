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
#include <GL/gl.h>
#include "w32_common.h"
#endif
#ifdef CONFIG_GL_X11
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "x11_common.h"
#endif

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
#ifndef GL_REGISTER_COMBINERS_NV
#define GL_REGISTER_COMBINERS_NV 0x8522
#endif
#ifndef GL_MAX_GENERAL_COMBINERS_NV
#define GL_MAX_GENERAL_COMBINERS_NV 0x854D
#endif
#ifndef GL_NUM_GENERAL_COMBINERS_NV
#define GL_NUM_GENERAL_COMBINERS_NV 0x854E
#endif
#ifndef GL_CONSTANT_COLOR0_NV
#define GL_CONSTANT_COLOR0_NV 0x852A
#endif
#ifndef GL_CONSTANT_COLOR1_NV
#define GL_CONSTANT_COLOR1_NV 0x852B
#endif
#ifndef GL_COMBINER0_NV
#define GL_COMBINER0_NV 0x8550
#endif
#ifndef GL_COMBINER1_NV
#define GL_COMBINER1_NV 0x8551
#endif
#ifndef GL_VARIABLE_A_NV
#define GL_VARIABLE_A_NV 0x8523
#endif
#ifndef GL_VARIABLE_B_NV
#define GL_VARIABLE_B_NV 0x8524
#endif
#ifndef GL_VARIABLE_C_NV
#define GL_VARIABLE_C_NV 0x8525
#endif
#ifndef GL_VARIABLE_D_NV
#define GL_VARIABLE_D_NV 0x8526
#endif
#ifndef GL_UNSIGNED_INVERT_NV
#define GL_UNSIGNED_INVERT_NV 0x8537
#endif
#ifndef GL_HALF_BIAS_NORMAL_NV
#define GL_HALF_BIAS_NORMAL_NV 0x853A
#endif
#ifndef GL_SIGNED_IDENTITY_NV
#define GL_SIGNED_IDENTITY_NV 0x853C
#endif
#ifndef GL_SCALE_BY_FOUR_NV
#define GL_SCALE_BY_FOUR_NV 0x853F
#endif
#ifndef GL_DISCARD_NV
#define GL_DISCARD_NV 0x8530
#endif
#ifndef GL_SPARE0_NV
#define GL_SPARE0_NV 0x852E
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

void glAdjustAlignment(int stride);

const char *glValName(GLint value);

int glFindFormat(uint32_t format, int *bpp, GLint *gl_texfmt,
                  GLenum *gl_format, GLenum *gl_type);
int glFmt2bpp(GLenum format, GLenum type);
void glCreateClearTex(GLenum target, GLenum fmt, GLenum format, GLenum type, GLint filter,
                      int w, int h, unsigned char val);
int glCreatePPMTex(GLenum target, GLenum fmt, GLint filter,
                   FILE *f, int *width, int *height, int *maxval);
void glUploadTex(GLenum target, GLenum format, GLenum type,
                 const void *dataptr, int stride,
                 int x, int y, int w, int h, int slice);
void glDrawTex(GLfloat x, GLfloat y, GLfloat w, GLfloat h,
               GLfloat tx, GLfloat ty, GLfloat tw, GLfloat th,
               int sx, int sy, int rect_tex, int is_yv12, int flip);
int loadGPUProgram(GLenum target, char *prog);

/** \addtogroup glconversion
  * \{ */
//! do not use YUV conversion, this should always stay 0
#define YUV_CONVERSION_NONE 0
//! use nVidia specific register combiners for YUV conversion
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
#define YUV_CONVERSION(t) (t & YUV_CONVERSION_MASK)
//! extract luminance scaler out of type
#define YUV_LUM_SCALER(t) ((t >> YUV_LUM_SCALER_SHIFT) & YUV_SCALER_MASK)
//! extract chrominance scaler out of type
#define YUV_CHROM_SCALER(t) ((t >> YUV_CHROM_SCALER_SHIFT) & YUV_SCALER_MASK)
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

int glAutodetectYUVConversion(void);
void glSetupYUVConversion(gl_conversion_params_t *params);
void glEnableYUVConversion(GLenum target, int type);
void glDisableYUVConversion(GLenum target, int type);

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
  GLTYPE_W32,
  GLTYPE_X11,
};

typedef struct MPGLContext {
  enum MPGLType type;
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
  int (*setGlWindow)(struct MPGLContext *);
  void (*releaseGlContext)(struct MPGLContext *);
  void (*swapGlBuffers)(struct MPGLContext *);
  void (*update_xinerama_info)(void);
  void (*border)(void);
  int (*check_events)(void);
  void (*fullscreen)(void);
  void (*ontop)(void);
} MPGLContext;

int init_mpglcontext(MPGLContext *ctx, enum MPGLType type);
void uninit_mpglcontext(MPGLContext *ctx);

extern void (GLAPIENTRY *mpglBegin)(GLenum);
extern void (GLAPIENTRY *mpglEnd)(void);
extern void (GLAPIENTRY *mpglViewport)(GLint, GLint, GLsizei, GLsizei);
extern void (GLAPIENTRY *mpglMatrixMode)(GLenum);
extern void (GLAPIENTRY *mpglLoadIdentity)(void);
extern void (GLAPIENTRY *mpglTranslated)(double, double, double);
extern void (GLAPIENTRY *mpglScaled)(double, double, double);
extern void (GLAPIENTRY *mpglOrtho)(double, double, double, double, double, double);
extern void (GLAPIENTRY *mpglFrustum)(double, double, double, double, double, double);
extern void (GLAPIENTRY *mpglPushMatrix)(void);
extern void (GLAPIENTRY *mpglPopMatrix)(void);
extern void (GLAPIENTRY *mpglClear)(GLbitfield);
extern GLuint (GLAPIENTRY *mpglGenLists)(GLsizei);
extern void (GLAPIENTRY *mpglDeleteLists)(GLuint, GLsizei);
extern void (GLAPIENTRY *mpglNewList)(GLuint, GLenum);
extern void (GLAPIENTRY *mpglEndList)(void);
extern void (GLAPIENTRY *mpglCallList)(GLuint);
extern void (GLAPIENTRY *mpglCallLists)(GLsizei, GLenum, const GLvoid *);
extern void (GLAPIENTRY *mpglGenTextures)(GLsizei, GLuint *);
extern void (GLAPIENTRY *mpglDeleteTextures)(GLsizei, const GLuint *);
extern void (GLAPIENTRY *mpglTexEnvf)(GLenum, GLenum, GLfloat);
extern void (GLAPIENTRY *mpglTexEnvi)(GLenum, GLenum, GLint);
extern void (GLAPIENTRY *mpglColor4ub)(GLubyte, GLubyte, GLubyte, GLubyte);
extern void (GLAPIENTRY *mpglColor3f)(GLfloat, GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglColor4f)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
extern void (GLAPIENTRY *mpglClearDepth)(GLclampd);
extern void (GLAPIENTRY *mpglDepthFunc)(GLenum);
extern void (GLAPIENTRY *mpglEnable)(GLenum);
extern void (GLAPIENTRY *mpglDisable)(GLenum);
extern const GLubyte *(GLAPIENTRY *mpglGetString)(GLenum);
extern void (GLAPIENTRY *mpglDrawBuffer)(GLenum);
extern void (GLAPIENTRY *mpglDepthMask)(GLboolean);
extern void (GLAPIENTRY *mpglBlendFunc)(GLenum, GLenum);
extern void (GLAPIENTRY *mpglFlush)(void);
extern void (GLAPIENTRY *mpglFinish)(void);
extern void (GLAPIENTRY *mpglPixelStorei)(GLenum, GLint);
extern void (GLAPIENTRY *mpglTexImage1D)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
extern void (GLAPIENTRY *mpglTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
extern void (GLAPIENTRY *mpglTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *);
extern void (GLAPIENTRY *mpglTexParameteri)(GLenum, GLenum, GLint);
extern void (GLAPIENTRY *mpglTexParameterf)(GLenum, GLenum, GLfloat);
extern void (GLAPIENTRY *mpglTexParameterfv)(GLenum, GLenum, const GLfloat *);
extern void (GLAPIENTRY *mpglTexCoord2f)(GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglVertex2f)(GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglVertex3f)(GLfloat, GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglNormal3f)(GLfloat, GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglLightfv)(GLenum, GLenum, const GLfloat *);
extern void (GLAPIENTRY *mpglColorMaterial)(GLenum, GLenum);
extern void (GLAPIENTRY *mpglShadeModel)(GLenum);
extern void (GLAPIENTRY *mpglGetIntegerv)(GLenum, GLint *);

extern void (GLAPIENTRY *mpglGenBuffers)(GLsizei, GLuint *);
extern void (GLAPIENTRY *mpglDeleteBuffers)(GLsizei, const GLuint *);
extern void (GLAPIENTRY *mpglBindBuffer)(GLenum, GLuint);
extern GLvoid* (GLAPIENTRY *mpglMapBuffer)(GLenum, GLenum);
extern GLboolean (GLAPIENTRY *mpglUnmapBuffer)(GLenum);
extern void (GLAPIENTRY *mpglBufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
extern void (GLAPIENTRY *mpglCombinerParameterfv)(GLenum, const GLfloat *);
extern void (GLAPIENTRY *mpglCombinerParameteri)(GLenum, GLint);
extern void (GLAPIENTRY *mpglCombinerInput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                                      GLenum);
extern void (GLAPIENTRY *mpglCombinerOutput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                                       GLenum, GLenum, GLboolean, GLboolean,
                                       GLboolean);
extern void (GLAPIENTRY *mpglBeginFragmentShader)(void);
extern void (GLAPIENTRY *mpglEndFragmentShader)(void);
extern void (GLAPIENTRY *mpglSampleMap)(GLuint, GLuint, GLenum);
extern void (GLAPIENTRY *mpglColorFragmentOp2)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                         GLuint, GLuint, GLuint, GLuint, GLuint);
extern void (GLAPIENTRY *mpglColorFragmentOp3)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                         GLuint, GLuint, GLuint, GLuint, GLuint,
                                         GLuint, GLuint, GLuint);
extern void (GLAPIENTRY *mpglSetFragmentShaderConstant)(GLuint, const GLfloat *);
extern void (GLAPIENTRY *mpglActiveTexture)(GLenum);
extern void (GLAPIENTRY *mpglBindTexture)(GLenum, GLuint);
extern void (GLAPIENTRY *mpglMultiTexCoord2f)(GLenum, GLfloat, GLfloat);
extern void (GLAPIENTRY *mpglGenPrograms)(GLsizei, GLuint *);
extern void (GLAPIENTRY *mpglDeletePrograms)(GLsizei, const GLuint *);
extern void (GLAPIENTRY *mpglBindProgram)(GLenum, GLuint);
extern void (GLAPIENTRY *mpglProgramString)(GLenum, GLenum, GLsizei, const GLvoid *);
extern void (GLAPIENTRY *mpglProgramEnvParameter4f)(GLenum, GLuint, GLfloat, GLfloat,
                                              GLfloat, GLfloat);
extern int (GLAPIENTRY *mpglSwapInterval)(int);
extern void (GLAPIENTRY *mpglTexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei,
                             GLsizei, GLint, GLenum, GLenum, const GLvoid *);
extern void* (GLAPIENTRY *mpglAllocateMemoryMESA)(void *, int, size_t, float, float, float);
extern void (GLAPIENTRY *mpglFreeMemoryMESA)(void *, int, void *);

#endif /* MPLAYER_GL_COMMON_H */
