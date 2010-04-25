/*
 * common OpenGL routines
 *
 * copyleft (C) 2005 Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>
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
 */

/**
 * \file gl_common.c
 * \brief OpenGL helper functions used by vo_gl.c and vo_gl2.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "gl_common.h"
#include "csputils.h"

void (GLAPIENTRY *mpglBegin)(GLenum);
void (GLAPIENTRY *mpglEnd)(void);
void (GLAPIENTRY *mpglViewport)(GLint, GLint, GLsizei, GLsizei);
void (GLAPIENTRY *mpglMatrixMode)(GLenum);
void (GLAPIENTRY *mpglLoadIdentity)(void);
void (GLAPIENTRY *mpglTranslated)(double, double, double);
void (GLAPIENTRY *mpglScaled)(double, double, double);
void (GLAPIENTRY *mpglOrtho)(double, double, double, double, double, double);
void (GLAPIENTRY *mpglFrustum)(double, double, double, double, double, double);
void (GLAPIENTRY *mpglPushMatrix)(void);
void (GLAPIENTRY *mpglPopMatrix)(void);
void (GLAPIENTRY *mpglClear)(GLbitfield);
GLuint (GLAPIENTRY *mpglGenLists)(GLsizei);
void (GLAPIENTRY *mpglDeleteLists)(GLuint, GLsizei);
void (GLAPIENTRY *mpglNewList)(GLuint, GLenum);
void (GLAPIENTRY *mpglEndList)(void);
void (GLAPIENTRY *mpglCallList)(GLuint);
void (GLAPIENTRY *mpglCallLists)(GLsizei, GLenum, const GLvoid *);
void (GLAPIENTRY *mpglGenTextures)(GLsizei, GLuint *);
void (GLAPIENTRY *mpglDeleteTextures)(GLsizei, const GLuint *);
void (GLAPIENTRY *mpglTexEnvf)(GLenum, GLenum, GLfloat);
void (GLAPIENTRY *mpglTexEnvi)(GLenum, GLenum, GLint);
void (GLAPIENTRY *mpglColor4ub)(GLubyte, GLubyte, GLubyte, GLubyte);
void (GLAPIENTRY *mpglColor3f)(GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *mpglColor4f)(GLfloat, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *mpglClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
void (GLAPIENTRY *mpglClearDepth)(GLclampd);
void (GLAPIENTRY *mpglDepthFunc)(GLenum);
void (GLAPIENTRY *mpglEnable)(GLenum);
void (GLAPIENTRY *mpglDisable)(GLenum);
const GLubyte *(GLAPIENTRY *mpglGetString)(GLenum);
void (GLAPIENTRY *mpglDrawBuffer)(GLenum);
void (GLAPIENTRY *mpglDepthMask)(GLboolean);
void (GLAPIENTRY *mpglBlendFunc)(GLenum, GLenum);
void (GLAPIENTRY *mpglFlush)(void);
void (GLAPIENTRY *mpglFinish)(void);
void (GLAPIENTRY *mpglPixelStorei)(GLenum, GLint);
void (GLAPIENTRY *mpglTexImage1D)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
void (GLAPIENTRY *mpglTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
void (GLAPIENTRY *mpglTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *);
void (GLAPIENTRY *mpglTexParameteri)(GLenum, GLenum, GLint);
void (GLAPIENTRY *mpglTexParameterf)(GLenum, GLenum, GLfloat);
void (GLAPIENTRY *mpglTexParameterfv)(GLenum, GLenum, const GLfloat *);
void (GLAPIENTRY *mpglTexCoord2f)(GLfloat, GLfloat);
void (GLAPIENTRY *mpglVertex2f)(GLfloat, GLfloat);
void (GLAPIENTRY *mpglVertex3f)(GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *mpglNormal3f)(GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *mpglLightfv)(GLenum, GLenum, const GLfloat *);
void (GLAPIENTRY *mpglColorMaterial)(GLenum, GLenum);
void (GLAPIENTRY *mpglShadeModel)(GLenum);
void (GLAPIENTRY *mpglGetIntegerv)(GLenum, GLint *);

/**
 * \defgroup glextfunctions OpenGL extension functions
 *
 * the pointers to these functions are acquired when the OpenGL
 * context is created
 * \{
 */
void (GLAPIENTRY *mpglGenBuffers)(GLsizei, GLuint *);
void (GLAPIENTRY *mpglDeleteBuffers)(GLsizei, const GLuint *);
void (GLAPIENTRY *mpglBindBuffer)(GLenum, GLuint);
GLvoid* (GLAPIENTRY *mpglMapBuffer)(GLenum, GLenum);
GLboolean (GLAPIENTRY *mpglUnmapBuffer)(GLenum);
void (GLAPIENTRY *mpglBufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
void (GLAPIENTRY *mpglCombinerParameterfv)(GLenum, const GLfloat *);
void (GLAPIENTRY *mpglCombinerParameteri)(GLenum, GLint);
void (GLAPIENTRY *mpglCombinerInput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                               GLenum);
void (GLAPIENTRY *mpglCombinerOutput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                                GLenum, GLenum, GLboolean, GLboolean,
                                GLboolean);
void (GLAPIENTRY *mpglBeginFragmentShader)(void);
void (GLAPIENTRY *mpglEndFragmentShader)(void);
void (GLAPIENTRY *mpglSampleMap)(GLuint, GLuint, GLenum);
void (GLAPIENTRY *mpglColorFragmentOp2)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                  GLuint, GLuint, GLuint, GLuint, GLuint);
void (GLAPIENTRY *mpglColorFragmentOp3)(GLenum, GLuint, GLuint, GLuint, GLuint,
                                  GLuint, GLuint, GLuint, GLuint, GLuint,
                                  GLuint, GLuint, GLuint);
void (GLAPIENTRY *mpglSetFragmentShaderConstant)(GLuint, const GLfloat *);
void (GLAPIENTRY *mpglActiveTexture)(GLenum);
void (GLAPIENTRY *mpglBindTexture)(GLenum, GLuint);
void (GLAPIENTRY *mpglMultiTexCoord2f)(GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *mpglGenPrograms)(GLsizei, GLuint *);
void (GLAPIENTRY *mpglDeletePrograms)(GLsizei, const GLuint *);
void (GLAPIENTRY *mpglBindProgram)(GLenum, GLuint);
void (GLAPIENTRY *mpglProgramString)(GLenum, GLenum, GLsizei, const GLvoid *);
void (GLAPIENTRY *mpglGetProgramiv)(GLenum, GLenum, GLint *);
void (GLAPIENTRY *mpglProgramEnvParameter4f)(GLenum, GLuint, GLfloat, GLfloat,
                                       GLfloat, GLfloat);
int (GLAPIENTRY *mpglSwapInterval)(int);
void (GLAPIENTRY *mpglTexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei,
                            GLint, GLenum, GLenum, const GLvoid *);
void* (GLAPIENTRY *mpglAllocateMemoryMESA)(void *, int, size_t, float, float, float);
void (GLAPIENTRY *mpglFreeMemoryMESA)(void *, int, void *);
/** \} */ // end of glextfunctions group

//! \defgroup glgeneral OpenGL general helper functions

//! \defgroup glcontext OpenGL context management helper functions

//! \defgroup gltexture OpenGL texture handling helper functions

//! \defgroup glconversion OpenGL conversion helper functions

static GLint hqtexfmt;

/**
 * \brief adjusts the GL_UNPACK_ALIGNMENT to fit the stride.
 * \param stride number of bytes per line for which alignment should fit.
 * \ingroup glgeneral
 */
void glAdjustAlignment(int stride) {
  GLint gl_alignment;
  if (stride % 8 == 0)
    gl_alignment=8;
  else if (stride % 4 == 0)
    gl_alignment=4;
  else if (stride % 2 == 0)
    gl_alignment=2;
  else
    gl_alignment=1;
  mpglPixelStorei(GL_UNPACK_ALIGNMENT, gl_alignment);
}

struct gl_name_map_struct {
  GLint value;
  const char *name;
};

#undef MAP
#define MAP(a) {a, #a}
//! mapping table for the glValName function
static const struct gl_name_map_struct gl_name_map[] = {
  // internal format
  MAP(GL_R3_G3_B2), MAP(GL_RGB4), MAP(GL_RGB5), MAP(GL_RGB8),
  MAP(GL_RGB10), MAP(GL_RGB12), MAP(GL_RGB16), MAP(GL_RGBA2),
  MAP(GL_RGBA4), MAP(GL_RGB5_A1), MAP(GL_RGBA8), MAP(GL_RGB10_A2),
  MAP(GL_RGBA12), MAP(GL_RGBA16), MAP(GL_LUMINANCE8),

  // format
  MAP(GL_RGB), MAP(GL_RGBA), MAP(GL_RED), MAP(GL_GREEN), MAP(GL_BLUE),
  MAP(GL_ALPHA), MAP(GL_LUMINANCE), MAP(GL_LUMINANCE_ALPHA),
  MAP(GL_COLOR_INDEX),
  // rest 1.2 only
  MAP(GL_BGR), MAP(GL_BGRA),

  //type
  MAP(GL_BYTE), MAP(GL_UNSIGNED_BYTE), MAP(GL_SHORT), MAP(GL_UNSIGNED_SHORT),
  MAP(GL_INT), MAP(GL_UNSIGNED_INT), MAP(GL_FLOAT), MAP(GL_DOUBLE),
  MAP(GL_2_BYTES), MAP(GL_3_BYTES), MAP(GL_4_BYTES),
  // rest 1.2 only
  MAP(GL_UNSIGNED_BYTE_3_3_2), MAP(GL_UNSIGNED_BYTE_2_3_3_REV),
  MAP(GL_UNSIGNED_SHORT_5_6_5), MAP(GL_UNSIGNED_SHORT_5_6_5_REV),
  MAP(GL_UNSIGNED_SHORT_4_4_4_4), MAP(GL_UNSIGNED_SHORT_4_4_4_4_REV),
  MAP(GL_UNSIGNED_SHORT_5_5_5_1), MAP(GL_UNSIGNED_SHORT_1_5_5_5_REV),
  MAP(GL_UNSIGNED_INT_8_8_8_8), MAP(GL_UNSIGNED_INT_8_8_8_8_REV),
  MAP(GL_UNSIGNED_INT_10_10_10_2), MAP(GL_UNSIGNED_INT_2_10_10_10_REV),
  {0, 0}
};
#undef MAP

/**
 * \brief return the name of an OpenGL constant
 * \param value the constant
 * \return name of the constant or "Unknown format!"
 * \ingroup glgeneral
 */
const char *glValName(GLint value)
{
  int i = 0;

  while (gl_name_map[i].name) {
    if (gl_name_map[i].value == value)
      return gl_name_map[i].name;
    i++;
  }
  return "Unknown format!";
}

//! always return this format as internal texture format in glFindFormat
#define TEXTUREFORMAT_ALWAYS GL_RGB8
#undef TEXTUREFORMAT_ALWAYS

/**
 * \brief find the OpenGL settings coresponding to format.
 *
 * All parameters may be NULL.
 * \param fmt MPlayer format to analyze.
 * \param bpp [OUT] bits per pixel of that format.
 * \param gl_texfmt [OUT] internal texture format that fits the
 * image format, not necessarily the best for performance.
 * \param gl_format [OUT] OpenGL format for this image format.
 * \param gl_type [OUT] OpenGL type for this image format.
 * \return 1 if format is supported by OpenGL, 0 if not.
 * \ingroup gltexture
 */
int glFindFormat(uint32_t fmt, int *bpp, GLint *gl_texfmt,
                  GLenum *gl_format, GLenum *gl_type)
{
  int supported = 1;
  int dummy1;
  GLenum dummy2;
  GLint dummy3;
  if (!bpp) bpp = &dummy1;
  if (!gl_texfmt) gl_texfmt = &dummy3;
  if (!gl_format) gl_format = &dummy2;
  if (!gl_type) gl_type = &dummy2;

  if (mp_get_chroma_shift(fmt, NULL, NULL)) {
    // reduce the possible cases a bit
    if (IMGFMT_IS_YUVP16_LE(fmt))
      fmt = IMGFMT_420P16_LE;
    else if (IMGFMT_IS_YUVP16_BE(fmt))
      fmt = IMGFMT_420P16_BE;
    else
      fmt = IMGFMT_YV12;
  }

  *bpp = IMGFMT_IS_BGR(fmt)?IMGFMT_BGR_DEPTH(fmt):IMGFMT_RGB_DEPTH(fmt);
  *gl_texfmt = 3;
  switch (fmt) {
    case IMGFMT_RGB48NE:
      *gl_format = GL_RGB;
      *gl_type = GL_UNSIGNED_SHORT;
      break;
    case IMGFMT_RGB24:
      *gl_format = GL_RGB;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case IMGFMT_RGBA:
      *gl_texfmt = 4;
      *gl_format = GL_RGBA;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case IMGFMT_420P16:
      supported = 0; // no native YUV support
      *gl_texfmt = 1;
      *bpp = 16;
      *gl_format = GL_LUMINANCE;
      *gl_type = GL_UNSIGNED_SHORT;
      break;
    case IMGFMT_YV12:
      supported = 0; // no native YV12 support
    case IMGFMT_Y800:
    case IMGFMT_Y8:
      *gl_texfmt = 1;
      *bpp = 8;
      *gl_format = GL_LUMINANCE;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case IMGFMT_UYVY:
    case IMGFMT_YUY2:
      *gl_texfmt = GL_YCBCR_MESA;
      *bpp = 16;
      *gl_format = GL_YCBCR_MESA;
      *gl_type = fmt == IMGFMT_UYVY ? GL_UNSIGNED_SHORT_8_8 : GL_UNSIGNED_SHORT_8_8_REV;
      break;
#if 0
    // we do not support palettized formats, although the format the
    // swscale produces works
    case IMGFMT_RGB8:
      gl_format = GL_RGB;
      gl_type = GL_UNSIGNED_BYTE_2_3_3_REV;
      break;
#endif
    case IMGFMT_RGB15:
      *gl_format = GL_RGBA;
      *gl_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
      break;
    case IMGFMT_RGB16:
      *gl_format = GL_RGB;
      *gl_type = GL_UNSIGNED_SHORT_5_6_5_REV;
      break;
#if 0
    case IMGFMT_BGR8:
      // special case as red and blue have a differen number of bits.
      // GL_BGR and GL_UNSIGNED_BYTE_3_3_2 isn't supported at least
      // by nVidia drivers, and in addition would give more bits to
      // blue than to red, which isn't wanted
      gl_format = GL_RGB;
      gl_type = GL_UNSIGNED_BYTE_3_3_2;
      break;
#endif
    case IMGFMT_BGR15:
      *gl_format = GL_BGRA;
      *gl_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
      break;
    case IMGFMT_BGR16:
      *gl_format = GL_RGB;
      *gl_type = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case IMGFMT_BGR24:
      *gl_format = GL_BGR;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case IMGFMT_BGRA:
      *gl_texfmt = 4;
      *gl_format = GL_BGRA;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    default:
      *gl_texfmt = 4;
      *gl_format = GL_RGBA;
      *gl_type = GL_UNSIGNED_BYTE;
      supported = 0;
  }
#ifdef TEXTUREFORMAT_ALWAYS
  *gl_texfmt = TEXTUREFORMAT_ALWAYS;
#endif
  return supported;
}

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif
/**
 * \brief find address of a linked function
 * \param s name of function to find
 * \return address of function or NULL if not found
 */
static void *getdladdr(const char *s) {
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

typedef struct {
  void *funcptr;
  const char *extstr;
  const char *funcnames[7];
  void *fallback;
} extfunc_desc_t;

#define DEF_FUNC_DESC(name) {&mpgl##name, NULL, {"gl"#name, NULL}, gl ##name}
static const extfunc_desc_t extfuncs[] = {
  // these aren't extension functions but we query them anyway to allow
  // different "backends" with one binary
  DEF_FUNC_DESC(Begin),
  DEF_FUNC_DESC(End),
  DEF_FUNC_DESC(Viewport),
  DEF_FUNC_DESC(MatrixMode),
  DEF_FUNC_DESC(LoadIdentity),
  DEF_FUNC_DESC(Translated),
  DEF_FUNC_DESC(Scaled),
  DEF_FUNC_DESC(Ortho),
  DEF_FUNC_DESC(Frustum),
  DEF_FUNC_DESC(PushMatrix),
  DEF_FUNC_DESC(PopMatrix),
  DEF_FUNC_DESC(Clear),
  DEF_FUNC_DESC(GenLists),
  DEF_FUNC_DESC(DeleteLists),
  DEF_FUNC_DESC(NewList),
  DEF_FUNC_DESC(EndList),
  DEF_FUNC_DESC(CallList),
  DEF_FUNC_DESC(CallLists),
  DEF_FUNC_DESC(GenTextures),
  DEF_FUNC_DESC(DeleteTextures),
  DEF_FUNC_DESC(TexEnvf),
  DEF_FUNC_DESC(TexEnvi),
  DEF_FUNC_DESC(Color4ub),
  DEF_FUNC_DESC(Color3f),
  DEF_FUNC_DESC(Color4f),
  DEF_FUNC_DESC(ClearColor),
  DEF_FUNC_DESC(ClearDepth),
  DEF_FUNC_DESC(DepthFunc),
  DEF_FUNC_DESC(Enable),
  DEF_FUNC_DESC(Disable),
  DEF_FUNC_DESC(DrawBuffer),
  DEF_FUNC_DESC(DepthMask),
  DEF_FUNC_DESC(BlendFunc),
  DEF_FUNC_DESC(Flush),
  DEF_FUNC_DESC(Finish),
  DEF_FUNC_DESC(PixelStorei),
  DEF_FUNC_DESC(TexImage1D),
  DEF_FUNC_DESC(TexImage2D),
  DEF_FUNC_DESC(TexSubImage2D),
  DEF_FUNC_DESC(TexParameteri),
  DEF_FUNC_DESC(TexParameterf),
  DEF_FUNC_DESC(TexParameterfv),
  DEF_FUNC_DESC(TexCoord2f),
  DEF_FUNC_DESC(Vertex2f),
  DEF_FUNC_DESC(Vertex3f),
  DEF_FUNC_DESC(Normal3f),
  DEF_FUNC_DESC(Lightfv),
  DEF_FUNC_DESC(ColorMaterial),
  DEF_FUNC_DESC(ShadeModel),
  DEF_FUNC_DESC(GetIntegerv),

  // here start the real extensions
  {&mpglGenBuffers, NULL, {"glGenBuffers", "glGenBuffersARB", NULL}},
  {&mpglDeleteBuffers, NULL, {"glDeleteBuffers", "glDeleteBuffersARB", NULL}},
  {&mpglBindBuffer, NULL, {"glBindBuffer", "glBindBufferARB", NULL}},
  {&mpglMapBuffer, NULL, {"glMapBuffer", "glMapBufferARB", NULL}},
  {&mpglUnmapBuffer, NULL, {"glUnmapBuffer", "glUnmapBufferARB", NULL}},
  {&mpglBufferData, NULL, {"glBufferData", "glBufferDataARB", NULL}},
  {&mpglCombinerParameterfv, "NV_register_combiners", {"glCombinerParameterfv", "glCombinerParameterfvNV", NULL}},
  {&mpglCombinerParameteri, "NV_register_combiners", {"glCombinerParameteri", "glCombinerParameteriNV", NULL}},
  {&mpglCombinerInput, "NV_register_combiners", {"glCombinerInput", "glCombinerInputNV", NULL}},
  {&mpglCombinerOutput, "NV_register_combiners", {"glCombinerOutput", "glCombinerOutputNV", NULL}},
  {&mpglBeginFragmentShader, "ATI_fragment_shader", {"glBeginFragmentShaderATI", NULL}},
  {&mpglEndFragmentShader, "ATI_fragment_shader", {"glEndFragmentShaderATI", NULL}},
  {&mpglSampleMap, "ATI_fragment_shader", {"glSampleMapATI", NULL}},
  {&mpglColorFragmentOp2, "ATI_fragment_shader", {"glColorFragmentOp2ATI", NULL}},
  {&mpglColorFragmentOp3, "ATI_fragment_shader", {"glColorFragmentOp3ATI", NULL}},
  {&mpglSetFragmentShaderConstant, "ATI_fragment_shader", {"glSetFragmentShaderConstantATI", NULL}},
  {&mpglActiveTexture, NULL, {"glActiveTexture", "glActiveTextureARB", NULL}},
  {&mpglBindTexture, NULL, {"glBindTexture", "glBindTextureARB", "glBindTextureEXT", NULL}},
  {&mpglMultiTexCoord2f, NULL, {"glMultiTexCoord2f", "glMultiTexCoord2fARB", NULL}},
  {&mpglGenPrograms, "_program", {"glGenProgramsARB", NULL}},
  {&mpglDeletePrograms, "_program", {"glDeleteProgramsARB", NULL}},
  {&mpglBindProgram, "_program", {"glBindProgramARB", NULL}},
  {&mpglProgramString, "_program", {"glProgramStringARB", NULL}},
  {&mpglGetProgramiv, "_program", {"glGetProgramivARB", NULL}},
  {&mpglProgramEnvParameter4f, "_program", {"glProgramEnvParameter4fARB", NULL}},
  {&mpglSwapInterval, "_swap_control", {"glXSwapIntervalSGI", "glXSwapInterval", "wglSwapIntervalSGI", "wglSwapInterval", "wglSwapIntervalEXT", NULL}},
  {&mpglTexImage3D, NULL, {"glTexImage3D", NULL}},
  {&mpglAllocateMemoryMESA, "GLX_MESA_allocate_memory", {"glXAllocateMemoryMESA", NULL}},
  {&mpglFreeMemoryMESA, "GLX_MESA_allocate_memory", {"glXFreeMemoryMESA", NULL}},
  {NULL}
};

/**
 * \brief find the function pointers of some useful OpenGL extensions
 * \param getProcAddress function to resolve function names, may be NULL
 * \param ext2 an extra extension string
 */
static void getFunctions(void *(*getProcAddress)(const GLubyte *),
                         const char *ext2) {
  const extfunc_desc_t *dsc;
  const char *extensions;
  char *allexts;

  if (!getProcAddress)
    getProcAddress = (void *)getdladdr;

  // special case, we need glGetString before starting to find the other functions
  mpglGetString = getProcAddress("glGetString");
  if (!mpglGetString)
      mpglGetString = glGetString;

  extensions = (const char *)mpglGetString(GL_EXTENSIONS);
  if (!extensions) extensions = "";
  if (!ext2) ext2 = "";
  allexts = malloc(strlen(extensions) + strlen(ext2) + 2);
  strcpy(allexts, extensions);
  strcat(allexts, " ");
  strcat(allexts, ext2);
  mp_msg(MSGT_VO, MSGL_DBG2, "OpenGL extensions string:\n%s\n", allexts);
  for (dsc = extfuncs; dsc->funcptr; dsc++) {
    void *ptr = NULL;
    int i;
    if (!dsc->extstr || strstr(allexts, dsc->extstr)) {
      for (i = 0; !ptr && dsc->funcnames[i]; i++)
        ptr = getProcAddress((const GLubyte *)dsc->funcnames[i]);
    }
    if (!ptr)
        ptr = dsc->fallback;
    *(void **)dsc->funcptr = ptr;
  }
  if (strstr(allexts, "_texture_float"))
    hqtexfmt = GL_RGB32F;
  else if (strstr(allexts, "NV_float_buffer"))
    hqtexfmt = GL_FLOAT_RGB32_NV;
  else
    hqtexfmt = GL_RGB16;
  free(allexts);
}

/**
 * \brief create a texture and set some defaults
 * \param target texture taget, usually GL_TEXTURE_2D
 * \param fmt internal texture format
 * \param format texture host data format
 * \param type texture host data type
 * \param filter filter used for scaling, e.g. GL_LINEAR
 * \param w texture width
 * \param h texture height
 * \param val luminance value to fill texture with
 * \ingroup gltexture
 */
void glCreateClearTex(GLenum target, GLenum fmt, GLenum format, GLenum type, GLint filter,
                      int w, int h, unsigned char val) {
  GLfloat fval = (GLfloat)val / 255.0;
  GLfloat border[4] = {fval, fval, fval, fval};
  int stride = w * glFmt2bpp(format, type);
  char *init;
  if (!stride) return;
  init = malloc(stride * h);
  memset(init, val, stride * h);
  glAdjustAlignment(stride);
  mpglPixelStorei(GL_UNPACK_ROW_LENGTH, w);
  mpglTexImage2D(target, 0, fmt, w, h, 0, format, type, init);
  mpglTexParameterf(target, GL_TEXTURE_PRIORITY, 1.0);
  mpglTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
  mpglTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
  mpglTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  mpglTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Border texels should not be used with CLAMP_TO_EDGE
  // We set a sane default anyway.
  mpglTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, border);
  free(init);
}

/**
 * \brief skips whitespace and comments
 * \param f file to read from
 */
static void ppm_skip(FILE *f) {
  int c, comment = 0;
  do {
    c = fgetc(f);
    if (c == '#')
      comment = 1;
    if (c == '\n')
      comment = 0;
  } while (c != EOF && (isspace(c) || comment));
  if (c != EOF)
    ungetc(c, f);
}

#define MAXDIM (16 * 1024)

/**
 * \brief creates a texture from a PPM file
 * \param target texture taget, usually GL_TEXTURE_2D
 * \param fmt internal texture format, 0 for default
 * \param filter filter used for scaling, e.g. GL_LINEAR
 * \param f file to read PPM from
 * \param width [out] width of texture
 * \param height [out] height of texture
 * \param maxval [out] maxval value from PPM file
 * \return 0 on error, 1 otherwise
 * \ingroup gltexture
 */
int glCreatePPMTex(GLenum target, GLenum fmt, GLint filter,
                   FILE *f, int *width, int *height, int *maxval) {
  unsigned w, h, m, val, bpp;
  char *data;
  GLenum type;
  ppm_skip(f);
  if (fgetc(f) != 'P' || fgetc(f) != '6')
    return 0;
  ppm_skip(f);
  if (fscanf(f, "%u", &w) != 1)
    return 0;
  ppm_skip(f);
  if (fscanf(f, "%u", &h) != 1)
    return 0;
  ppm_skip(f);
  if (fscanf(f, "%u", &m) != 1)
    return 0;
  val = fgetc(f);
  if (!isspace(val))
    return 0;
  if (w > MAXDIM || h > MAXDIM)
    return 0;
  bpp = (m > 255) ? 6 : 3;
  data = malloc(w * h * bpp);
  if (fread(data, w * bpp, h, f) != h)
    return 0;
  if (!fmt) {
    fmt = (m > 255) ? hqtexfmt : 3;
    if (fmt == GL_FLOAT_RGB32_NV && target != GL_TEXTURE_RECTANGLE)
      fmt = GL_RGB16;
  }
  type = m > 255 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
  glCreateClearTex(target, fmt, GL_RGB, type, filter, w, h, 0);
  glUploadTex(target, GL_RGB, type,
              data, w * bpp, 0, 0, w, h, 0);
  free(data);
  if (width) *width = w;
  if (height) *height = h;
  if (maxval) *maxval = m;
  return 1;
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
int glFmt2bpp(GLenum format, GLenum type) {
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
  }
  return 0; // unknown
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
void glUploadTex(GLenum target, GLenum format, GLenum type,
                 const void *dataptr, int stride,
                 int x, int y, int w, int h, int slice) {
  const uint8_t *data = dataptr;
  int y_max = y + h;
  if (w <= 0 || h <= 0) return;
  if (slice <= 0)
    slice = h;
  if (stride < 0) {
    data += (h - 1) * stride;
    stride = -stride;
  }
  // this is not always correct, but should work for MPlayer
  glAdjustAlignment(stride);
  mpglPixelStorei(GL_UNPACK_ROW_LENGTH, stride / glFmt2bpp(format, type));
  for (; y + slice <= y_max; y += slice) {
    mpglTexSubImage2D(target, 0, x, y, w, slice, format, type, data);
    data += stride * slice;
  }
  if (y < y_max)
    mpglTexSubImage2D(target, 0, x, y, w, y_max - y, format, type, data);
}

static void fillUVcoeff(GLfloat *ucoef, GLfloat *vcoef,
                        float uvcos, float uvsin) {
  int i;
  ucoef[0] = 0 * uvcos + 1.403 * uvsin;
  vcoef[0] = 0 * uvsin + 1.403 * uvcos;
  ucoef[1] = -0.344 * uvcos + -0.714 * uvsin;
  vcoef[1] = -0.344 * uvsin + -0.714 * uvcos;
  ucoef[2] = 1.770 * uvcos + 0 * uvsin;
  vcoef[2] = 1.770 * uvsin + 0 * uvcos;
  ucoef[3] = 0;
  vcoef[3] = 0;
  // Coefficients (probably) must be in [0, 1] range, whereas they originally
  // are in [-2, 2] range, so here comes the trick:
  // First put them in the [-0.5, 0.5] range, then add 0.5.
  // This can be undone with the HALF_BIAS and SCALE_BY_FOUR arguments
  // for CombinerInput and CombinerOutput (or the respective ATI variants)
  for (i = 0; i < 4; i++) {
    ucoef[i] = ucoef[i] * 0.25 + 0.5;
    vcoef[i] = vcoef[i] * 0.25 + 0.5;
  }
}

/**
 * \brief Setup register combiners for YUV to RGB conversion.
 * \param uvcos used for saturation and hue adjustment
 * \param uvsin used for saturation and hue adjustment
 */
static void glSetupYUVCombiners(float uvcos, float uvsin) {
  GLfloat ucoef[4];
  GLfloat vcoef[4];
  GLint i;
  if (!mpglCombinerInput || !mpglCombinerOutput ||
      !mpglCombinerParameterfv || !mpglCombinerParameteri) {
    mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Combiner functions missing!\n");
    return;
  }
  mpglGetIntegerv(GL_MAX_GENERAL_COMBINERS_NV, &i);
  if (i < 2)
    mp_msg(MSGT_VO, MSGL_ERR,
           "[gl] 2 general combiners needed for YUV combiner support (found %i)\n", i);
  mpglGetIntegerv(GL_MAX_TEXTURE_UNITS, &i);
  if (i < 3)
    mp_msg(MSGT_VO, MSGL_ERR,
           "[gl] 3 texture units needed for YUV combiner support (found %i)\n", i);
  fillUVcoeff(ucoef, vcoef, uvcos, uvsin);
  mpglCombinerParameterfv(GL_CONSTANT_COLOR0_NV, ucoef);
  mpglCombinerParameterfv(GL_CONSTANT_COLOR1_NV, vcoef);

  // UV first, like this green component cannot overflow
  mpglCombinerInput(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV,
                    GL_TEXTURE1, GL_HALF_BIAS_NORMAL_NV, GL_RGB);
  mpglCombinerInput(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV,
                    GL_CONSTANT_COLOR0_NV, GL_HALF_BIAS_NORMAL_NV, GL_RGB);
  mpglCombinerInput(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV,
                    GL_TEXTURE2, GL_HALF_BIAS_NORMAL_NV, GL_RGB);
  mpglCombinerInput(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV,
                    GL_CONSTANT_COLOR1_NV, GL_HALF_BIAS_NORMAL_NV, GL_RGB);
  mpglCombinerOutput(GL_COMBINER0_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV,
                     GL_SPARE0_NV, GL_SCALE_BY_FOUR_NV, GL_NONE, GL_FALSE,
                     GL_FALSE, GL_FALSE);

  // stage 2
  mpglCombinerInput(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE0_NV,
                    GL_SIGNED_IDENTITY_NV, GL_RGB);
  mpglCombinerInput(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_ZERO,
                    GL_UNSIGNED_INVERT_NV, GL_RGB);
  mpglCombinerInput(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV,
                    GL_TEXTURE0, GL_SIGNED_IDENTITY_NV, GL_RGB);
  mpglCombinerInput(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO,
                    GL_UNSIGNED_INVERT_NV, GL_RGB);
  mpglCombinerOutput(GL_COMBINER1_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV,
                     GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE,
                     GL_FALSE, GL_FALSE);

  // leave final combiner stage in default mode
  mpglCombinerParameteri(GL_NUM_GENERAL_COMBINERS_NV, 2);
}

/**
 * \brief Setup ATI version of register combiners for YUV to RGB conversion.
 * \param csp_params parameters used for colorspace conversion
 * \param text if set use the GL_ATI_text_fragment_shader API as
 *             used on OS X.
 */
static void glSetupYUVFragmentATI(struct mp_csp_params *csp_params,
                                  int text) {
  GLint i;
  float yuv2rgb[3][4];

  mpglGetIntegerv (GL_MAX_TEXTURE_UNITS, &i);
  if (i < 3)
    mp_msg(MSGT_VO, MSGL_ERR,
           "[gl] 3 texture units needed for YUV combiner (ATI) support (found %i)\n", i);

  mp_get_yuv2rgb_coeffs(csp_params, yuv2rgb);
  for (i = 0; i < 3; i++) {
    int j;
    yuv2rgb[i][3] -= -0.5 * (yuv2rgb[i][1] + yuv2rgb[i][2]);
    for (j = 0; j < 4; j++) {
      yuv2rgb[i][j] *= 0.125;
      yuv2rgb[i][j] += 0.5;
      if (yuv2rgb[i][j] > 1)
        yuv2rgb[i][j] = 1;
      if (yuv2rgb[i][j] < 0)
        yuv2rgb[i][j] = 0;
    }
  }
  if (text == 0) {
    GLfloat c0[4] = {yuv2rgb[0][0], yuv2rgb[1][0], yuv2rgb[2][0]};
    GLfloat c1[4] = {yuv2rgb[0][1], yuv2rgb[1][1], yuv2rgb[2][1]};
    GLfloat c2[4] = {yuv2rgb[0][2], yuv2rgb[1][2], yuv2rgb[2][2]};
    GLfloat c3[4] = {yuv2rgb[0][3], yuv2rgb[1][3], yuv2rgb[2][3]};
    if (!mpglBeginFragmentShader || !mpglEndFragmentShader ||
        !mpglSetFragmentShaderConstant || !mpglSampleMap ||
        !mpglColorFragmentOp2 || !mpglColorFragmentOp3) {
      mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Combiner (ATI) functions missing!\n");
      return;
    }
    mpglGetIntegerv(GL_NUM_FRAGMENT_REGISTERS_ATI, &i);
    if (i < 3)
      mp_msg(MSGT_VO, MSGL_ERR,
             "[gl] 3 registers needed for YUV combiner (ATI) support (found %i)\n", i);
    mpglBeginFragmentShader();
    mpglSetFragmentShaderConstant(GL_CON_0_ATI, c0);
    mpglSetFragmentShaderConstant(GL_CON_1_ATI, c1);
    mpglSetFragmentShaderConstant(GL_CON_2_ATI, c2);
    mpglSetFragmentShaderConstant(GL_CON_3_ATI, c3);
    mpglSampleMap(GL_REG_0_ATI, GL_TEXTURE0, GL_SWIZZLE_STR_ATI);
    mpglSampleMap(GL_REG_1_ATI, GL_TEXTURE1, GL_SWIZZLE_STR_ATI);
    mpglSampleMap(GL_REG_2_ATI, GL_TEXTURE2, GL_SWIZZLE_STR_ATI);
    mpglColorFragmentOp2(GL_MUL_ATI, GL_REG_1_ATI, GL_NONE, GL_NONE,
                         GL_REG_1_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                         GL_CON_1_ATI, GL_NONE, GL_BIAS_BIT_ATI);
    mpglColorFragmentOp3(GL_MAD_ATI, GL_REG_2_ATI, GL_NONE, GL_NONE,
                         GL_REG_2_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                         GL_CON_2_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                         GL_REG_1_ATI, GL_NONE, GL_NONE);
    mpglColorFragmentOp3(GL_MAD_ATI, GL_REG_0_ATI, GL_NONE, GL_NONE,
                         GL_REG_0_ATI, GL_NONE, GL_NONE,
                         GL_CON_0_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                         GL_REG_2_ATI, GL_NONE, GL_NONE);
    mpglColorFragmentOp2(GL_ADD_ATI, GL_REG_0_ATI, GL_NONE, GL_8X_BIT_ATI,
                         GL_REG_0_ATI, GL_NONE, GL_NONE,
                         GL_CON_3_ATI, GL_NONE, GL_BIAS_BIT_ATI);
    mpglEndFragmentShader();
  } else {
    static const char template[] =
      "!!ATIfs1.0\n"
      "StartConstants;\n"
      "  CONSTANT c0 = {%e, %e, %e};\n"
      "  CONSTANT c1 = {%e, %e, %e};\n"
      "  CONSTANT c2 = {%e, %e, %e};\n"
      "  CONSTANT c3 = {%e, %e, %e};\n"
      "EndConstants;\n"
      "StartOutputPass;\n"
      "  SampleMap r0, t0.str;\n"
      "  SampleMap r1, t1.str;\n"
      "  SampleMap r2, t2.str;\n"
      "  MUL r1.rgb, r1.bias, c1.bias;\n"
      "  MAD r2.rgb, r2.bias, c2.bias, r1;\n"
      "  MAD r0.rgb, r0, c0.bias, r2;\n"
      "  ADD r0.rgb.8x, r0, c3.bias;\n"
      "EndPass;\n";
    char buffer[512];
    snprintf(buffer, sizeof(buffer), template,
             yuv2rgb[0][0], yuv2rgb[1][0], yuv2rgb[2][0],
             yuv2rgb[0][1], yuv2rgb[1][1], yuv2rgb[2][1],
             yuv2rgb[0][2], yuv2rgb[1][2], yuv2rgb[2][2],
             yuv2rgb[0][3], yuv2rgb[1][3], yuv2rgb[2][3]);
    mp_msg(MSGT_VO, MSGL_DBG2, "[gl] generated fragment program:\n%s\n", buffer);
    loadGPUProgram(GL_TEXT_FRAGMENT_SHADER_ATI, buffer);
  }
}

/**
 * \brief helper function for gen_spline_lookup_tex
 * \param x subpixel-position ((0,1) range) to calculate weights for
 * \param dst where to store transformed weights, must provide space for 4 GLfloats
 *
 * calculates the weights and stores them after appropriate transformation
 * for the scaler fragment program.
 */
static void store_weights(float x, GLfloat *dst) {
  float w0 = (((-1 * x + 3) * x - 3) * x + 1) / 6;
  float w1 = ((( 3 * x - 6) * x + 0) * x + 4) / 6;
  float w2 = (((-3 * x + 3) * x + 3) * x + 1) / 6;
  float w3 = ((( 1 * x + 0) * x + 0) * x + 0) / 6;
  *dst++ = 1 + x - w1 / (w0 + w1);
  *dst++ = 1 - x + w3 / (w2 + w3);
  *dst++ = w0 + w1;
  *dst++ = 0;
}

//! to avoid artefacts this should be rather large
#define LOOKUP_BSPLINE_RES (2 * 1024)
/**
 * \brief creates the 1D lookup texture needed for fast higher-order filtering
 * \param unit texture unit to attach texture to
 */
static void gen_spline_lookup_tex(GLenum unit) {
  GLfloat *tex = calloc(4 * LOOKUP_BSPLINE_RES, sizeof(*tex));
  GLfloat *tp = tex;
  int i;
  for (i = 0; i < LOOKUP_BSPLINE_RES; i++) {
    float x = (float)(i + 0.5) / LOOKUP_BSPLINE_RES;
    store_weights(x, tp);
    tp += 4;
  }
  store_weights(0, tex);
  store_weights(1, &tex[4 * (LOOKUP_BSPLINE_RES - 1)]);
  mpglActiveTexture(unit);
  mpglTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, LOOKUP_BSPLINE_RES, 0, GL_RGBA, GL_FLOAT, tex);
  mpglTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_PRIORITY, 1.0);
  mpglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  mpglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  mpglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  mpglActiveTexture(GL_TEXTURE0);
  free(tex);
}

static const char *bilin_filt_template =
  "TEX yuv.%c, fragment.texcoord[%c], texture[%c], %s;\n";

#define BICUB_FILT_MAIN(textype) \
  /* first y-interpolation */ \
  "ADD coord, fragment.texcoord[%c].xyxy, cdelta.xyxw;\n" \
  "ADD coord2, fragment.texcoord[%c].xyxy, cdelta.zyzw;\n" \
  "TEX a.r, coord.xyxy, texture[%c], "textype";\n" \
  "TEX a.g, coord.zwzw, texture[%c], "textype";\n" \
  /* second y-interpolation */ \
  "TEX b.r, coord2.xyxy, texture[%c], "textype";\n" \
  "TEX b.g, coord2.zwzw, texture[%c], "textype";\n" \
  "LRP a.b, parmy.b, a.rrrr, a.gggg;\n" \
  "LRP a.a, parmy.b, b.rrrr, b.gggg;\n" \
  /* x-interpolation */ \
  "LRP yuv.%c, parmx.b, a.bbbb, a.aaaa;\n"

static const char *bicub_filt_template_2D =
  "MAD coord.xy, fragment.texcoord[%c], {%e, %e}, {0.5, 0.5};\n"
  "TEX parmx, coord.x, texture[%c], 1D;\n"
  "MUL cdelta.xz, parmx.rrgg, {-%e, 0, %e, 0};\n"
  "TEX parmy, coord.y, texture[%c], 1D;\n"
  "MUL cdelta.yw, parmy.rrgg, {0, -%e, 0, %e};\n"
  BICUB_FILT_MAIN("2D");

static const char *bicub_filt_template_RECT =
  "ADD coord, fragment.texcoord[%c], {0.5, 0.5};\n"
  "TEX parmx, coord.x, texture[%c], 1D;\n"
  "MUL cdelta.xz, parmx.rrgg, {-1, 0, 1, 0};\n"
  "TEX parmy, coord.y, texture[%c], 1D;\n"
  "MUL cdelta.yw, parmy.rrgg, {0, -1, 0, 1};\n"
  BICUB_FILT_MAIN("RECT");

#define CALCWEIGHTS(t, s) \
  "MAD "t", {-0.5, 0.1666, 0.3333, -0.3333}, "s", {1, 0, -0.5, 0.5};\n" \
  "MAD "t", "t", "s", {0, 0, -0.5, 0.5};\n" \
  "MAD "t", "t", "s", {-0.6666, 0, 0.8333, 0.1666};\n" \
  "RCP a.x, "t".z;\n" \
  "RCP a.y, "t".w;\n" \
  "MAD "t".xy, "t".xyxy, a.xyxy, {1, 1, 0, 0};\n" \
  "ADD "t".x, "t".xxxx, "s";\n" \
  "SUB "t".y, "t".yyyy, "s";\n"

static const char *bicub_notex_filt_template_2D =
  "MAD coord.xy, fragment.texcoord[%c], {%e, %e}, {0.5, 0.5};\n"
  "FRC coord.xy, coord.xyxy;\n"
  CALCWEIGHTS("parmx", "coord.xxxx")
  "MUL cdelta.xz, parmx.rrgg, {-%e, 0, %e, 0};\n"
  CALCWEIGHTS("parmy", "coord.yyyy")
  "MUL cdelta.yw, parmy.rrgg, {0, -%e, 0, %e};\n"
  BICUB_FILT_MAIN("2D");

static const char *bicub_notex_filt_template_RECT =
  "ADD coord, fragment.texcoord[%c], {0.5, 0.5};\n"
  "FRC coord.xy, coord.xyxy;\n"
  CALCWEIGHTS("parmx", "coord.xxxx")
  "MUL cdelta.xz, parmx.rrgg, {-1, 0, 1, 0};\n"
  CALCWEIGHTS("parmy", "coord.yyyy")
  "MUL cdelta.yw, parmy.rrgg, {0, -1, 0, 1};\n"
  BICUB_FILT_MAIN("RECT");

#define BICUB_X_FILT_MAIN(textype) \
  "ADD coord.xy, fragment.texcoord[%c].xyxy, cdelta.xyxy;\n" \
  "ADD coord2.xy, fragment.texcoord[%c].xyxy, cdelta.zyzy;\n" \
  "TEX a.r, coord, texture[%c], "textype";\n" \
  "TEX b.r, coord2, texture[%c], "textype";\n" \
  /* x-interpolation */ \
  "LRP yuv.%c, parmx.b, a.rrrr, b.rrrr;\n"

static const char *bicub_x_filt_template_2D =
  "MAD coord.x, fragment.texcoord[%c], {%e}, {0.5};\n"
  "TEX parmx, coord, texture[%c], 1D;\n"
  "MUL cdelta.xyz, parmx.rrgg, {-%e, 0, %e};\n"
  BICUB_X_FILT_MAIN("2D");

static const char *bicub_x_filt_template_RECT =
  "ADD coord.x, fragment.texcoord[%c], {0.5};\n"
  "TEX parmx, coord, texture[%c], 1D;\n"
  "MUL cdelta.xyz, parmx.rrgg, {-1, 0, 1};\n"
  BICUB_X_FILT_MAIN("RECT");

static const char *unsharp_filt_template =
  "PARAM dcoord%c = {%e, %e, %e, %e};\n"
  "ADD coord, fragment.texcoord[%c].xyxy, dcoord%c;\n"
  "SUB coord2, fragment.texcoord[%c].xyxy, dcoord%c;\n"
  "TEX a.r, fragment.texcoord[%c], texture[%c], %s;\n"
  "TEX b.r, coord.xyxy, texture[%c], %s;\n"
  "TEX b.g, coord.zwzw, texture[%c], %s;\n"
  "ADD b.r, b.r, b.g;\n"
  "TEX b.b, coord2.xyxy, texture[%c], %s;\n"
  "TEX b.g, coord2.zwzw, texture[%c], %s;\n"
  "DP3 b, b, {0.25, 0.25, 0.25};\n"
  "SUB b.r, a.r, b.r;\n"
  "MAD yuv.%c, b.r, {%e}, a.r;\n";

static const char *unsharp_filt_template2 =
  "PARAM dcoord%c = {%e, %e, %e, %e};\n"
  "PARAM dcoord2%c = {%e, 0, 0, %e};\n"
  "ADD coord, fragment.texcoord[%c].xyxy, dcoord%c;\n"
  "SUB coord2, fragment.texcoord[%c].xyxy, dcoord%c;\n"
  "TEX a.r, fragment.texcoord[%c], texture[%c], %s;\n"
  "TEX b.r, coord.xyxy, texture[%c], %s;\n"
  "TEX b.g, coord.zwzw, texture[%c], %s;\n"
  "ADD b.r, b.r, b.g;\n"
  "TEX b.b, coord2.xyxy, texture[%c], %s;\n"
  "TEX b.g, coord2.zwzw, texture[%c], %s;\n"
  "ADD b.r, b.r, b.b;\n"
  "ADD b.a, b.r, b.g;\n"
  "ADD coord, fragment.texcoord[%c].xyxy, dcoord2%c;\n"
  "SUB coord2, fragment.texcoord[%c].xyxy, dcoord2%c;\n"
  "TEX b.r, coord.xyxy, texture[%c], %s;\n"
  "TEX b.g, coord.zwzw, texture[%c], %s;\n"
  "ADD b.r, b.r, b.g;\n"
  "TEX b.b, coord2.xyxy, texture[%c], %s;\n"
  "TEX b.g, coord2.zwzw, texture[%c], %s;\n"
  "DP4 b.r, b, {-0.1171875, -0.1171875, -0.1171875, -0.09765625};\n"
  "MAD b.r, a.r, {0.859375}, b.r;\n"
  "MAD yuv.%c, b.r, {%e}, a.r;\n";

static const char *yuv_prog_template =
  "PARAM ycoef = {%e, %e, %e};\n"
  "PARAM ucoef = {%e, %e, %e};\n"
  "PARAM vcoef = {%e, %e, %e};\n"
  "PARAM offsets = {%e, %e, %e};\n"
  "TEMP res;\n"
  "MAD res.rgb, yuv.rrrr, ycoef, offsets;\n"
  "MAD res.rgb, yuv.gggg, ucoef, res;\n"
  "MAD result.color.rgb, yuv.bbbb, vcoef, res;\n"
  "END";

static const char *yuv_pow_prog_template =
  "PARAM ycoef = {%e, %e, %e};\n"
  "PARAM ucoef = {%e, %e, %e};\n"
  "PARAM vcoef = {%e, %e, %e};\n"
  "PARAM offsets = {%e, %e, %e};\n"
  "PARAM gamma = {%e, %e, %e};\n"
  "TEMP res;\n"
  "MAD res.rgb, yuv.rrrr, ycoef, offsets;\n"
  "MAD res.rgb, yuv.gggg, ucoef, res;\n"
  "MAD_SAT res.rgb, yuv.bbbb, vcoef, res;\n"
  "POW result.color.r, res.r, gamma.r;\n"
  "POW result.color.g, res.g, gamma.g;\n"
  "POW result.color.b, res.b, gamma.b;\n"
  "END";

static const char *yuv_lookup_prog_template =
  "PARAM ycoef = {%e, %e, %e, 0};\n"
  "PARAM ucoef = {%e, %e, %e, 0};\n"
  "PARAM vcoef = {%e, %e, %e, 0};\n"
  "PARAM offsets = {%e, %e, %e, 0.125};\n"
  "TEMP res;\n"
  "MAD res, yuv.rrrr, ycoef, offsets;\n"
  "MAD res.rgb, yuv.gggg, ucoef, res;\n"
  "MAD res.rgb, yuv.bbbb, vcoef, res;\n"
  "TEX result.color.r, res.raaa, texture[%c], 2D;\n"
  "ADD res.a, res.a, 0.25;\n"
  "TEX result.color.g, res.gaaa, texture[%c], 2D;\n"
  "ADD res.a, res.a, 0.25;\n"
  "TEX result.color.b, res.baaa, texture[%c], 2D;\n"
  "END";

static const char *yuv_lookup3d_prog_template =
  "TEX result.color, yuv, texture[%c], 3D;\n"
  "END";

/**
 * \brief creates and initializes helper textures needed for scaling texture read
 * \param scaler scaler type to create texture for
 * \param texu contains next free texture unit number
 * \param texs texture unit ids for the scaler are stored in this array
 */
static void create_scaler_textures(int scaler, int *texu, char *texs) {
  switch (scaler) {
    case YUV_SCALER_BILIN:
    case YUV_SCALER_BICUB_NOTEX:
    case YUV_SCALER_UNSHARP:
    case YUV_SCALER_UNSHARP2:
      break;
    case YUV_SCALER_BICUB:
    case YUV_SCALER_BICUB_X:
      texs[0] = (*texu)++;
      gen_spline_lookup_tex(GL_TEXTURE0 + texs[0]);
      texs[0] += '0';
      break;
    default:
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown scaler type %i\n", scaler);
  }
}

//! resolution of texture for gamma lookup table
#define LOOKUP_RES 512
//! resolution for 3D yuv->rgb conversion lookup table
#define LOOKUP_3DRES 32
/**
 * \brief creates and initializes helper textures needed for yuv conversion
 * \param params struct containing parameters like brightness, gamma, ...
 * \param texu contains next free texture unit number
 * \param texs texture unit ids for the conversion are stored in this array
 */
static void create_conv_textures(gl_conversion_params_t *params, int *texu, char *texs) {
  unsigned char *lookup_data = NULL;
  int conv = YUV_CONVERSION(params->type);
  switch (conv) {
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_FRAGMENT_POW:
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
      texs[0] = (*texu)++;
      mpglActiveTexture(GL_TEXTURE0 + texs[0]);
      lookup_data = malloc(4 * LOOKUP_RES);
      mp_gen_gamma_map(lookup_data, LOOKUP_RES, params->csp_params.rgamma);
      mp_gen_gamma_map(&lookup_data[LOOKUP_RES], LOOKUP_RES, params->csp_params.ggamma);
      mp_gen_gamma_map(&lookup_data[2 * LOOKUP_RES], LOOKUP_RES, params->csp_params.bgamma);
      glCreateClearTex(GL_TEXTURE_2D, GL_LUMINANCE8, GL_LUMINANCE, GL_UNSIGNED_BYTE, GL_LINEAR,
                       LOOKUP_RES, 4, 0);
      glUploadTex(GL_TEXTURE_2D, GL_LUMINANCE, GL_UNSIGNED_BYTE, lookup_data,
                  LOOKUP_RES, 0, 0, LOOKUP_RES, 4, 0);
      mpglActiveTexture(GL_TEXTURE0);
      texs[0] += '0';
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
      {
        int sz = LOOKUP_3DRES + 2; // texture size including borders
        if (!mpglTexImage3D) {
          mp_msg(MSGT_VO, MSGL_ERR, "[gl] Missing 3D texture function!\n");
          break;
        }
        texs[0] = (*texu)++;
        mpglActiveTexture(GL_TEXTURE0 + texs[0]);
        lookup_data = malloc(3 * sz * sz * sz);
        mp_gen_yuv2rgb_map(&params->csp_params, lookup_data, LOOKUP_3DRES);
        glAdjustAlignment(sz);
        mpglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        mpglTexImage3D(GL_TEXTURE_3D, 0, 3, sz, sz, sz, 1,
                       GL_RGB, GL_UNSIGNED_BYTE, lookup_data);
        mpglTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_PRIORITY, 1.0);
        mpglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        mpglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        mpglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        mpglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        mpglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
        mpglActiveTexture(GL_TEXTURE0);
        texs[0] += '0';
      }
      break;
    default:
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown conversion type %i\n", conv);
  }
  if (lookup_data)
    free(lookup_data);
}

/**
 * \brief adds a scaling texture read at the current fragment program position
 * \param scaler type of scaler to insert
 * \param prog_pos current position in fragment program
 * \param remain how many bytes remain in the buffer given by prog_pos
 * \param texs array containing the texture unit identifiers for this scaler
 * \param in_tex texture unit the scaler should read from
 * \param out_comp component of the yuv variable the scaler stores the result in
 * \param rect if rectangular (pixel) adressing should be used for in_tex
 * \param texw width of the in_tex texture
 * \param texh height of the in_tex texture
 * \param strength strength of filter effect if the scaler does some kind of filtering
 */
static void add_scaler(int scaler, char **prog_pos, int *remain, char *texs,
                    char in_tex, char out_comp, int rect, int texw, int texh,
                    double strength) {
  const char *ttype = rect ? "RECT" : "2D";
  const float ptw = rect ? 1.0 : 1.0 / texw;
  const float pth = rect ? 1.0 : 1.0 / texh;
  switch (scaler) {
    case YUV_SCALER_BILIN:
      snprintf(*prog_pos, *remain, bilin_filt_template, out_comp, in_tex,
                 in_tex, ttype);
      break;
    case YUV_SCALER_BICUB:
      if (rect)
        snprintf(*prog_pos, *remain, bicub_filt_template_RECT,
                 in_tex, texs[0], texs[0],
                 in_tex, in_tex, in_tex, in_tex, in_tex, in_tex, out_comp);
      else
        snprintf(*prog_pos, *remain, bicub_filt_template_2D,
                 in_tex, (float)texw, (float)texh,
                 texs[0], ptw, ptw, texs[0], pth, pth,
                 in_tex, in_tex, in_tex, in_tex, in_tex, in_tex, out_comp);
      break;
    case YUV_SCALER_BICUB_X:
      if (rect)
        snprintf(*prog_pos, *remain, bicub_x_filt_template_RECT,
                 in_tex, texs[0],
                 in_tex, in_tex, in_tex, in_tex, out_comp);
      else
        snprintf(*prog_pos, *remain, bicub_x_filt_template_2D,
                 in_tex, (float)texw,
                 texs[0], ptw, ptw,
                 in_tex, in_tex, in_tex, in_tex, out_comp);
      break;
    case YUV_SCALER_BICUB_NOTEX:
      if (rect)
        snprintf(*prog_pos, *remain, bicub_notex_filt_template_RECT,
                 in_tex,
                 in_tex, in_tex, in_tex, in_tex, in_tex, in_tex, out_comp);
      else
        snprintf(*prog_pos, *remain, bicub_notex_filt_template_2D,
                 in_tex, (float)texw, (float)texh, ptw, ptw, pth, pth,
                 in_tex, in_tex, in_tex, in_tex, in_tex, in_tex, out_comp);
      break;
    case YUV_SCALER_UNSHARP:
      snprintf(*prog_pos, *remain, unsharp_filt_template,
               out_comp, 0.5 * ptw, 0.5 * pth, 0.5 * ptw, -0.5 * pth,
               in_tex, out_comp, in_tex, out_comp, in_tex,
               in_tex, ttype, in_tex, ttype, in_tex, ttype, in_tex, ttype,
               in_tex, ttype, out_comp, strength);
      break;
    case YUV_SCALER_UNSHARP2:
      snprintf(*prog_pos, *remain, unsharp_filt_template2,
               out_comp, 1.2 * ptw, 1.2 * pth, 1.2 * ptw, -1.2 * pth,
               out_comp, 1.5 * ptw, 1.5 * pth,
               in_tex, out_comp, in_tex, out_comp, in_tex,
               in_tex, ttype, in_tex, ttype, in_tex, ttype, in_tex, ttype,
               in_tex, ttype, in_tex, out_comp, in_tex, out_comp,
               in_tex, ttype, in_tex, ttype, in_tex, ttype,
               in_tex, ttype, out_comp, strength);
      break;
  }
  *remain -= strlen(*prog_pos);
  *prog_pos += strlen(*prog_pos);
}

static const struct {
  const char *name;
  GLenum cur;
  GLenum max;
} progstats[] = {
  {"instructions", 0x88A0, 0x88A1},
  {"native instructions", 0x88A2, 0x88A3},
  {"temporaries", 0x88A4, 0x88A5},
  {"native temporaries", 0x88A6, 0x88A7},
  {"parameters", 0x88A8, 0x88A9},
  {"native parameters", 0x88AA, 0x88AB},
  {"attribs", 0x88AC, 0x88AD},
  {"native attribs", 0x88AE, 0x88AF},
  {"ALU instructions", 0x8805, 0x880B},
  {"TEX instructions", 0x8806, 0x880C},
  {"TEX indirections", 0x8807, 0x880D},
  {"native ALU instructions", 0x8808, 0x880E},
  {"native TEX instructions", 0x8809, 0x880F},
  {"native TEX indirections", 0x880A, 0x8810},
  {NULL, 0, 0}
};

/**
 * \brief load the specified GPU Program
 * \param target program target to load into, only GL_FRAGMENT_PROGRAM is tested
 * \param prog program string
 * \return 1 on success, 0 otherwise
 */
int loadGPUProgram(GLenum target, char *prog) {
  int i;
  GLint cur = 0, max = 0, err = 0;
  if (!mpglProgramString) {
    mp_msg(MSGT_VO, MSGL_ERR, "[gl] Missing GPU program function\n");
    return 0;
  }
  mpglProgramString(target, GL_PROGRAM_FORMAT_ASCII, strlen(prog), prog);
  mpglGetIntegerv(GL_PROGRAM_ERROR_POSITION, &err);
  if (err != -1) {
    mp_msg(MSGT_VO, MSGL_ERR,
      "[gl] Error compiling fragment program, make sure your card supports\n"
      "[gl]   GL_ARB_fragment_program (use glxinfo to check).\n"
      "[gl]   Error message:\n  %s at %.10s\n",
      mpglGetString(GL_PROGRAM_ERROR_STRING), &prog[err]);
    return 0;
  }
  if (!mpglGetProgramiv || !mp_msg_test(MSGT_VO, MSGL_DBG2))
    return 1;
  mp_msg(MSGT_VO, MSGL_V, "[gl] Program statistics:\n");
  for (i = 0; progstats[i].name; i++) {
    mpglGetProgramiv(target, progstats[i].cur, &cur);
    mpglGetProgramiv(target, progstats[i].max, &max);
    mp_msg(MSGT_VO, MSGL_V, "[gl]   %s: %i/%i\n", progstats[i].name, cur, max);
  }
  return 1;
}

#define MAX_PROGSZ (1024*1024)

/**
 * \brief setup a fragment program that will do YUV->RGB conversion
 * \param parms struct containing parameters like conversion and scaler type,
 *              brightness, ...
 */
static void glSetupYUVFragprog(gl_conversion_params_t *params) {
  int type = params->type;
  int texw = params->texw;
  int texh = params->texh;
  int rect = params->target == GL_TEXTURE_RECTANGLE;
  static const char prog_hdr[] =
    "!!ARBfp1.0\n"
    "OPTION ARB_precision_hint_fastest;\n"
    // all scaler variables must go here so they aren't defined
    // multiple times when the same scaler is used more than once
    "TEMP coord, coord2, cdelta, parmx, parmy, a, b, yuv;\n";
  int prog_remain;
  char *yuv_prog, *prog_pos;
  int cur_texu = 3;
  char lum_scale_texs[1];
  char chrom_scale_texs[1];
  char conv_texs[1];
  GLint i;
  // this is the conversion matrix, with y, u, v factors
  // for red, green, blue and the constant offsets
  float yuv2rgb[3][4];
  create_conv_textures(params, &cur_texu, conv_texs);
  create_scaler_textures(YUV_LUM_SCALER(type), &cur_texu, lum_scale_texs);
  if (YUV_CHROM_SCALER(type) == YUV_LUM_SCALER(type))
    memcpy(chrom_scale_texs, lum_scale_texs, sizeof(chrom_scale_texs));
  else
    create_scaler_textures(YUV_CHROM_SCALER(type), &cur_texu, chrom_scale_texs);
  mpglGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &i);
  if (i < cur_texu)
    mp_msg(MSGT_VO, MSGL_ERR,
           "[gl] %i texture units needed for this type of YUV fragment support (found %i)\n",
           cur_texu, i);
  if (!mpglProgramString) {
    mp_msg(MSGT_VO, MSGL_FATAL, "[gl] ProgramString function missing!\n");
    return;
  }
  yuv_prog = malloc(MAX_PROGSZ);
  strcpy(yuv_prog, prog_hdr);
  prog_pos    = yuv_prog + sizeof(prog_hdr) - 1;
  prog_remain = MAX_PROGSZ - sizeof(prog_hdr);
  add_scaler(YUV_LUM_SCALER(type), &prog_pos, &prog_remain, lum_scale_texs,
             '0', 'r', rect, texw, texh, params->filter_strength);
  add_scaler(YUV_CHROM_SCALER(type), &prog_pos, &prog_remain, chrom_scale_texs,
             '1', 'g', rect, params->chrom_texw, params->chrom_texh, params->filter_strength);
  add_scaler(YUV_CHROM_SCALER(type), &prog_pos, &prog_remain, chrom_scale_texs,
             '2', 'b', rect, params->chrom_texw, params->chrom_texh, params->filter_strength);
  mp_get_yuv2rgb_coeffs(&params->csp_params, yuv2rgb);
  switch (YUV_CONVERSION(type)) {
    case YUV_CONVERSION_FRAGMENT:
      snprintf(prog_pos, prog_remain, yuv_prog_template,
               yuv2rgb[ROW_R][COL_Y], yuv2rgb[ROW_G][COL_Y], yuv2rgb[ROW_B][COL_Y],
               yuv2rgb[ROW_R][COL_U], yuv2rgb[ROW_G][COL_U], yuv2rgb[ROW_B][COL_U],
               yuv2rgb[ROW_R][COL_V], yuv2rgb[ROW_G][COL_V], yuv2rgb[ROW_B][COL_V],
               yuv2rgb[ROW_R][COL_C], yuv2rgb[ROW_G][COL_C], yuv2rgb[ROW_B][COL_C]);
      break;
    case YUV_CONVERSION_FRAGMENT_POW:
      snprintf(prog_pos, prog_remain, yuv_pow_prog_template,
               yuv2rgb[ROW_R][COL_Y], yuv2rgb[ROW_G][COL_Y], yuv2rgb[ROW_B][COL_Y],
               yuv2rgb[ROW_R][COL_U], yuv2rgb[ROW_G][COL_U], yuv2rgb[ROW_B][COL_U],
               yuv2rgb[ROW_R][COL_V], yuv2rgb[ROW_G][COL_V], yuv2rgb[ROW_B][COL_V],
               yuv2rgb[ROW_R][COL_C], yuv2rgb[ROW_G][COL_C], yuv2rgb[ROW_B][COL_C],
               (float)1.0 / params->csp_params.rgamma, (float)1.0 / params->csp_params.bgamma, (float)1.0 / params->csp_params.bgamma);
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
      snprintf(prog_pos, prog_remain, yuv_lookup_prog_template,
               yuv2rgb[ROW_R][COL_Y], yuv2rgb[ROW_G][COL_Y], yuv2rgb[ROW_B][COL_Y],
               yuv2rgb[ROW_R][COL_U], yuv2rgb[ROW_G][COL_U], yuv2rgb[ROW_B][COL_U],
               yuv2rgb[ROW_R][COL_V], yuv2rgb[ROW_G][COL_V], yuv2rgb[ROW_B][COL_V],
               yuv2rgb[ROW_R][COL_C], yuv2rgb[ROW_G][COL_C], yuv2rgb[ROW_B][COL_C],
               conv_texs[0], conv_texs[0], conv_texs[0]);
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
      snprintf(prog_pos, prog_remain, yuv_lookup3d_prog_template, conv_texs[0]);
      break;
    default:
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown conversion type %i\n", YUV_CONVERSION(type));
      break;
  }
  mp_msg(MSGT_VO, MSGL_DBG2, "[gl] generated fragment program:\n%s\n", yuv_prog);
  loadGPUProgram(GL_FRAGMENT_PROGRAM, yuv_prog);
  free(yuv_prog);
}

/**
 * \brief detect the best YUV->RGB conversion method available
 */
int glAutodetectYUVConversion(void) {
  const char *extensions = mpglGetString(GL_EXTENSIONS);
  if (!extensions || !mpglMultiTexCoord2f)
    return YUV_CONVERSION_NONE;
  if (strstr(extensions, "GL_ARB_fragment_program"))
    return YUV_CONVERSION_FRAGMENT;
  if (strstr(extensions, "GL_ATI_text_fragment_shader"))
    return YUV_CONVERSION_TEXT_FRAGMENT;
  if (strstr(extensions, "GL_ATI_fragment_shader"))
    return YUV_CONVERSION_COMBINERS_ATI;
  return YUV_CONVERSION_NONE;
}

/**
 * \brief setup YUV->RGB conversion
 * \param parms struct containing parameters like conversion and scaler type,
 *              brightness, ...
 * \ingroup glconversion
 */
void glSetupYUVConversion(gl_conversion_params_t *params) {
  float uvcos = params->csp_params.saturation * cos(params->csp_params.hue);
  float uvsin = params->csp_params.saturation * sin(params->csp_params.hue);
  switch (YUV_CONVERSION(params->type)) {
    case YUV_CONVERSION_COMBINERS:
      glSetupYUVCombiners(uvcos, uvsin);
      break;
    case YUV_CONVERSION_COMBINERS_ATI:
      glSetupYUVFragmentATI(&params->csp_params, 0);
      break;
    case YUV_CONVERSION_TEXT_FRAGMENT:
      glSetupYUVFragmentATI(&params->csp_params, 1);
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_FRAGMENT_POW:
      glSetupYUVFragprog(params);
      break;
    case YUV_CONVERSION_NONE:
      break;
    default:
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown conversion type %i\n", YUV_CONVERSION(params->type));
  }
}

/**
 * \brief enable the specified YUV conversion
 * \param target texture target for Y, U and V textures (e.g. GL_TEXTURE_2D)
 * \param type type of YUV conversion
 * \ingroup glconversion
 */
void glEnableYUVConversion(GLenum target, int type) {
  switch (YUV_CONVERSION(type)) {
    case YUV_CONVERSION_COMBINERS:
      mpglActiveTexture(GL_TEXTURE1);
      mpglEnable(target);
      mpglActiveTexture(GL_TEXTURE2);
      mpglEnable(target);
      mpglActiveTexture(GL_TEXTURE0);
      mpglEnable(GL_REGISTER_COMBINERS_NV);
      break;
    case YUV_CONVERSION_COMBINERS_ATI:
      mpglActiveTexture(GL_TEXTURE1);
      mpglEnable(target);
      mpglActiveTexture(GL_TEXTURE2);
      mpglEnable(target);
      mpglActiveTexture(GL_TEXTURE0);
      mpglEnable(GL_FRAGMENT_SHADER_ATI);
      break;
    case YUV_CONVERSION_TEXT_FRAGMENT:
      mpglActiveTexture(GL_TEXTURE1);
      mpglEnable(target);
      mpglActiveTexture(GL_TEXTURE2);
      mpglEnable(target);
      mpglActiveTexture(GL_TEXTURE0);
      mpglEnable(GL_TEXT_FRAGMENT_SHADER_ATI);
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
    case YUV_CONVERSION_FRAGMENT_POW:
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_NONE:
      mpglEnable(GL_FRAGMENT_PROGRAM);
      break;
  }
}

/**
 * \brief disable the specified YUV conversion
 * \param target texture target for Y, U and V textures (e.g. GL_TEXTURE_2D)
 * \param type type of YUV conversion
 * \ingroup glconversion
 */
void glDisableYUVConversion(GLenum target, int type) {
  switch (YUV_CONVERSION(type)) {
    case YUV_CONVERSION_COMBINERS:
      mpglActiveTexture(GL_TEXTURE1);
      mpglDisable(target);
      mpglActiveTexture(GL_TEXTURE2);
      mpglDisable(target);
      mpglActiveTexture(GL_TEXTURE0);
      mpglDisable(GL_REGISTER_COMBINERS_NV);
      break;
    case YUV_CONVERSION_COMBINERS_ATI:
      mpglActiveTexture(GL_TEXTURE1);
      mpglDisable(target);
      mpglActiveTexture(GL_TEXTURE2);
      mpglDisable(target);
      mpglActiveTexture(GL_TEXTURE0);
      mpglDisable(GL_FRAGMENT_SHADER_ATI);
      break;
    case YUV_CONVERSION_TEXT_FRAGMENT:
      mpglDisable(GL_TEXT_FRAGMENT_SHADER_ATI);
      // HACK: at least the Mac OS X 10.5 PPC Radeon drivers are broken and
      // without this disable the texture units while the program is still
      // running (10.4 PPC seems to work without this though).
      mpglFlush();
      mpglActiveTexture(GL_TEXTURE1);
      mpglDisable(target);
      mpglActiveTexture(GL_TEXTURE2);
      mpglDisable(target);
      mpglActiveTexture(GL_TEXTURE0);
      break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
    case YUV_CONVERSION_FRAGMENT_POW:
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_NONE:
      mpglDisable(GL_FRAGMENT_PROGRAM);
      break;
  }
}

/**
 * \brief draw a texture part at given 2D coordinates
 * \param x screen top coordinate
 * \param y screen left coordinate
 * \param w screen width coordinate
 * \param h screen height coordinate
 * \param tx texture top coordinate in pixels
 * \param ty texture left coordinate in pixels
 * \param tw texture part width in pixels
 * \param th texture part height in pixels
 * \param sx width of texture in pixels
 * \param sy height of texture in pixels
 * \param rect_tex whether this texture uses texture_rectangle extension
 * \param is_yv12 if != 0, also draw the textures from units 1 and 2,
 *                bits 8 - 15 and 16 - 23 specify the x and y scaling of those textures
 * \param flip flip the texture upside down
 * \ingroup gltexture
 */
void glDrawTex(GLfloat x, GLfloat y, GLfloat w, GLfloat h,
               GLfloat tx, GLfloat ty, GLfloat tw, GLfloat th,
               int sx, int sy, int rect_tex, int is_yv12, int flip) {
  int chroma_x_shift = (is_yv12 >>  8) & 31;
  int chroma_y_shift = (is_yv12 >> 16) & 31;
  GLfloat xscale = 1 << chroma_x_shift;
  GLfloat yscale = 1 << chroma_y_shift;
  GLfloat tx2 = tx / xscale, ty2 = ty / yscale, tw2 = tw / xscale, th2 = th / yscale;
  if (!rect_tex) {
    tx /= sx; ty /= sy; tw /= sx; th /= sy;
    tx2 = tx, ty2 = ty, tw2 = tw, th2 = th;
  }
  if (flip) {
    y += h;
    h = -h;
  }
  mpglBegin(GL_QUADS);
  mpglTexCoord2f(tx, ty);
  if (is_yv12) {
    mpglMultiTexCoord2f(GL_TEXTURE1, tx2, ty2);
    mpglMultiTexCoord2f(GL_TEXTURE2, tx2, ty2);
  }
  mpglVertex2f(x, y);
  mpglTexCoord2f(tx, ty + th);
  if (is_yv12) {
    mpglMultiTexCoord2f(GL_TEXTURE1, tx2, ty2 + th2);
    mpglMultiTexCoord2f(GL_TEXTURE2, tx2, ty2 + th2);
  }
  mpglVertex2f(x, y + h);
  mpglTexCoord2f(tx + tw, ty + th);
  if (is_yv12) {
    mpglMultiTexCoord2f(GL_TEXTURE1, tx2 + tw2, ty2 + th2);
    mpglMultiTexCoord2f(GL_TEXTURE2, tx2 + tw2, ty2 + th2);
  }
  mpglVertex2f(x + w, y + h);
  mpglTexCoord2f(tx + tw, ty);
  if (is_yv12) {
    mpglMultiTexCoord2f(GL_TEXTURE1, tx2 + tw2, ty2);
    mpglMultiTexCoord2f(GL_TEXTURE2, tx2 + tw2, ty2);
  }
  mpglVertex2f(x + w, y);
  mpglEnd();
}

#ifdef CONFIG_GL_WIN32
#include "w32_common.h"
/**
 * \brief little helper since wglGetProcAddress definition does not fit our
 *        getProcAddress
 * \param procName name of function to look up
 * \return function pointer returned by wglGetProcAddress
 */
static void *w32gpa(const GLubyte *procName) {
  HMODULE oglmod;
  void *res = wglGetProcAddress(procName);
  if (res) return res;
  oglmod = GetModuleHandle("opengl32.dll");
  return GetProcAddress(oglmod, procName);
}

static int setGlWindow_w32(MPGLContext *ctx)
{
  HWND win = vo_w32_window;
  int *vinfo = &ctx->vinfo.w32;
  HGLRC *context = &ctx->context.w32;
  int new_vinfo;
  HDC windc = vo_w32_get_dc(win);
  HGLRC new_context = 0;
  int keep_context = 0;
  int res = SET_WINDOW_FAILED;

  // should only be needed when keeping context, but not doing glFinish
  // can cause flickering even when we do not keep it.
  if (*context)
  mpglFinish();
  new_vinfo = GetPixelFormat(windc);
  if (*context && *vinfo && new_vinfo && *vinfo == new_vinfo) {
      // we can keep the wglContext
      new_context = *context;
      keep_context = 1;
  } else {
    // create a context
    new_context = wglCreateContext(windc);
    if (!new_context) {
      mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GL context!\n");
      goto out;
    }
  }

  // set context
  if (!wglMakeCurrent(windc, new_context)) {
    mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL context!\n");
    if (!keep_context) {
      wglDeleteContext(new_context);
    }
    goto out;
  }

  // set new values
  vo_w32_window = win;
  {
    RECT rect;
    GetClientRect(win, &rect);
    vo_dwidth = rect.right;
    vo_dheight = rect.bottom;
  }
  if (!keep_context) {
    if (*context)
      wglDeleteContext(*context);
    *context = new_context;
    *vinfo = new_vinfo;
    getFunctions(w32gpa, NULL);

    // and inform that reinit is neccessary
    res = SET_WINDOW_REINIT;
  } else
    res = SET_WINDOW_OK;

out:
  vo_w32_release_dc(win, windc);
  return res;
}

static void releaseGlContext_w32(MPGLContext *ctx) {
  int *vinfo = &ctx->vinfo.w32;
  HGLRC *context = &ctx->context.w32;
  *vinfo = 0;
  if (*context) {
    wglMakeCurrent(0, 0);
    wglDeleteContext(*context);
  }
  *context = 0;
}

static void swapGlBuffers_w32(MPGLContext *ctx) {
  HDC vo_hdc = vo_w32_get_dc(vo_w32_window);
  SwapBuffers(vo_hdc);
  vo_w32_release_dc(vo_w32_window, vo_hdc);
}
#endif
#ifdef CONFIG_GL_X11
#include "x11_common.h"

/**
 * \brief Returns the XVisualInfo associated with Window win.
 * \param win Window whose XVisualInfo is returne.
 * \return XVisualInfo of the window. Caller must use XFree to free it.
 */
static XVisualInfo *getWindowVisualInfo(Window win) {
  XWindowAttributes xw_attr;
  XVisualInfo vinfo_template;
  int tmp;
  XGetWindowAttributes(mDisplay, win, &xw_attr);
  vinfo_template.visualid = XVisualIDFromVisual(xw_attr.visual);
  return XGetVisualInfo(mDisplay, VisualIDMask, &vinfo_template, &tmp);
}

static void appendstr(char **dst, const char *str)
{
    int newsize;
    char *newstr;
    if (!str)
        return;
    newsize = strlen(*dst) + 1 + strlen(str) + 1;
    newstr = realloc(*dst, newsize);
    if (!newstr)
        return;
    *dst = newstr;
    strcat(*dst, " ");
    strcat(*dst, str);
}

/**
 * \brief Changes the window in which video is displayed.
 * If possible only transfers the context to the new window, otherwise
 * creates a new one, which must be initialized by the caller.
 * \param vinfo Currently used visual.
 * \param context Currently used context.
 * \param win window that should be used for drawing.
 * \return one of SET_WINDOW_FAILED, SET_WINDOW_OK or SET_WINDOW_REINIT.
 * In case of SET_WINDOW_REINIT the context could not be transfered
 * and the caller must initialize it correctly.
 * \ingroup glcontext
 */
static int setGlWindow_x11(MPGLContext *ctx)
{
  XVisualInfo **vinfo = &ctx->vinfo.x11;
  GLXContext *context = &ctx->context.x11;
  Window win = vo_window;
  XVisualInfo *new_vinfo;
  GLXContext new_context = NULL;
  int keep_context = 0;

  // should only be needed when keeping context, but not doing glFinish
  // can cause flickering even when we do not keep it.
  if (*context)
  mpglFinish();
  new_vinfo = getWindowVisualInfo(win);
  if (*context && *vinfo && new_vinfo &&
      (*vinfo)->visualid == new_vinfo->visualid) {
      // we can keep the GLXContext
      new_context = *context;
      XFree(new_vinfo);
      new_vinfo = *vinfo;
      keep_context = 1;
  } else {
    // create a context
    new_context = glXCreateContext(mDisplay, new_vinfo, NULL, True);
    if (!new_context) {
      mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GLX context!\n");
      XFree(new_vinfo);
      return SET_WINDOW_FAILED;
    }
  }

  // set context
  if (!glXMakeCurrent(mDisplay, vo_window, new_context)) {
    mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GLX context!\n");
    if (!keep_context) {
      glXDestroyContext(mDisplay, new_context);
      XFree(new_vinfo);
    }
    return SET_WINDOW_FAILED;
  }

  // set new values
  vo_window = win;
  vo_x11_update_geometry();
  if (!keep_context) {
    void *(*getProcAddress)(const GLubyte *);
    const char *(*glXExtStr)(Display *, int);
    char *glxstr = strdup("");
    if (*context)
      glXDestroyContext(mDisplay, *context);
    *context = new_context;
    if (*vinfo)
      XFree(*vinfo);
    *vinfo = new_vinfo;
    getProcAddress = getdladdr("glXGetProcAddress");
    if (!getProcAddress)
      getProcAddress = getdladdr("glXGetProcAddressARB");
    glXExtStr = getdladdr("glXQueryExtensionsString");
    if (glXExtStr)
        appendstr(&glxstr, glXExtStr(mDisplay, DefaultScreen(mDisplay)));
    glXExtStr = getdladdr("glXGetClientString");
    if (glXExtStr)
        appendstr(&glxstr, glXExtStr(mDisplay, GLX_EXTENSIONS));
    glXExtStr = getdladdr("glXGetServerString");
    if (glXExtStr)
        appendstr(&glxstr, glXExtStr(mDisplay, GLX_EXTENSIONS));

    getFunctions(getProcAddress, glxstr);
    if (!mpglGenPrograms && mpglGetString &&
        getProcAddress &&
        strstr(mpglGetString(GL_EXTENSIONS), "GL_ARB_vertex_program")) {
      mp_msg(MSGT_VO, MSGL_WARN, "Broken glXGetProcAddress detected, trying workaround\n");
      getFunctions(NULL, glxstr);
    }
    free(glxstr);

    // and inform that reinit is neccessary
    return SET_WINDOW_REINIT;
  }
  return SET_WINDOW_OK;
}

/**
 * \brief free the VisualInfo and GLXContext of an OpenGL context.
 * \ingroup glcontext
 */
static void releaseGlContext_x11(MPGLContext *ctx) {
  XVisualInfo **vinfo = &ctx->vinfo.x11;
  GLXContext *context = &ctx->context.x11;
  if (*vinfo)
    XFree(*vinfo);
  *vinfo = NULL;
  if (*context)
  {
    mpglFinish();
    glXMakeCurrent(mDisplay, None, NULL);
    glXDestroyContext(mDisplay, *context);
  }
  *context = 0;
}

static void swapGlBuffers_x11(MPGLContext *ctx) {
  glXSwapBuffers(mDisplay, vo_window);
}

static int x11_check_events(void) {
  return vo_x11_check_events(mDisplay);
}
#endif

#ifdef CONFIG_GL_SDL
#ifdef CONFIG_SDL_SDL_H
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

static void swapGlBuffers_sdl(MPGLContext *ctx) {
  SDL_GL_SwapBuffers();
}

#endif

static int setGlWindow_dummy(MPGLContext *ctx) {
  getFunctions(NULL, NULL);
  return SET_WINDOW_OK;
}

static void releaseGlContext_dummy(MPGLContext *ctx) {
}

static int dummy_check_events(void) {
  return 0;
}

int init_mpglcontext(MPGLContext *ctx, enum MPGLType type) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->setGlWindow = setGlWindow_dummy;
  ctx->releaseGlContext = releaseGlContext_dummy;
  ctx->check_events = dummy_check_events;
  ctx->type = type;
  switch (ctx->type) {
#ifdef CONFIG_GL_WIN32
  case GLTYPE_W32:
    ctx->setGlWindow = setGlWindow_w32;
    ctx->releaseGlContext = releaseGlContext_w32;
    ctx->swapGlBuffers = swapGlBuffers_w32;
    ctx->update_xinerama_info = w32_update_xinerama_info;
    ctx->border = vo_w32_border;
    ctx->check_events = vo_w32_check_events;
    ctx->fullscreen = vo_w32_fullscreen;
    ctx->ontop = vo_w32_ontop;
    return vo_w32_init();
#endif
#ifdef CONFIG_GL_X11
  case GLTYPE_X11:
    ctx->setGlWindow = setGlWindow_x11;
    ctx->releaseGlContext = releaseGlContext_x11;
    ctx->swapGlBuffers = swapGlBuffers_x11;
    ctx->update_xinerama_info = update_xinerama_info;
    ctx->border = vo_x11_border;
    ctx->check_events = x11_check_events;
    ctx->fullscreen = vo_x11_fullscreen;
    ctx->ontop = vo_x11_ontop;
    return vo_init();
#endif
#ifdef CONFIG_GL_SDL
  case GLTYPE_SDL:
    SDL_Init(SDL_INIT_VIDEO);
    ctx->swapGlBuffers = swapGlBuffers_sdl;
    return 1;
#endif
  default:
    return 0;
  }
}

void uninit_mpglcontext(MPGLContext *ctx) {
  ctx->releaseGlContext(ctx);
  switch (ctx->type) {
#ifdef CONFIG_GL_WIN32
  case GLTYPE_W32:
    vo_w32_uninit();
    break;
#endif
#ifdef CONFIG_GL_X11
  case GLTYPE_X11:
    vo_x11_uninit();
    break;
#endif
#ifdef CONFIG_GL_SDL
  case GLTYPE_SDL:
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    break;
#endif
  }
  memset(ctx, 0, sizeof(*ctx));
}
