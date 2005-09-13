#include <stdlib.h>
#include <string.h>
#include "gl_common.h"

void (APIENTRY *GenBuffers)(GLsizei, GLuint *);
void (APIENTRY *DeleteBuffers)(GLsizei, const GLuint *);
void (APIENTRY *BindBuffer)(GLenum, GLuint);
GLvoid* (APIENTRY *MapBuffer)(GLenum, GLenum); 
GLboolean (APIENTRY *UnmapBuffer)(GLenum);
void (APIENTRY *BufferData)(GLenum, intptr_t, const GLvoid *, GLenum);
void (APIENTRY *CombinerParameterfv)(GLenum, const GLfloat *);
void (APIENTRY *CombinerParameteri)(GLenum, GLint);
void (APIENTRY *CombinerInput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                               GLenum);
void (APIENTRY *CombinerOutput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                                GLenum, GLenum, GLboolean, GLboolean,
                                GLboolean);
void (APIENTRY *ActiveTexture)(GLenum);
void (APIENTRY *BindTexture)(GLenum, GLuint);
void (APIENTRY *MultiTexCoord2f)(GLenum, GLfloat, GLfloat);
void (APIENTRY *BindProgram)(GLenum, GLuint);
void (APIENTRY *ProgramString)(GLenum, GLenum, GLsizei, const GLvoid *);
void (APIENTRY *ProgramEnvParameter4f)(GLenum, GLuint, GLfloat, GLfloat,
                                       GLfloat, GLfloat);
int (APIENTRY *SwapInterval)(int);

/**
 * \brief adjusts the GL_UNPACK_ALGNMENT to fit the stride.
 * \param stride number of bytes per line for which alignment should fit.
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
  glPixelStorei (GL_UNPACK_ALIGNMENT, gl_alignment);
}

struct gl_name_map_struct {
  GLint value;
  char *name;
};

#undef MAP
#define MAP(a) {a, #a}
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
 */
int glFindFormat(uint32_t fmt, uint32_t *bpp, GLint *gl_texfmt,
                  GLenum *gl_format, GLenum *gl_type)
{
  int supported = 1;
  int dummy1;
  GLenum dummy2;
  GLint dummy3;
  if (bpp == NULL) bpp = &dummy1;
  if (gl_texfmt == NULL) gl_texfmt = &dummy3;
  if (gl_format == NULL) gl_format = &dummy2;
  if (gl_type == NULL) gl_type = &dummy2;
  
  *bpp = IMGFMT_IS_BGR(fmt)?IMGFMT_BGR_DEPTH(fmt):IMGFMT_RGB_DEPTH(fmt);
  *gl_texfmt = 3;
  switch (fmt) {
    case IMGFMT_RGB24:
      *gl_format = GL_RGB;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case IMGFMT_RGBA:
      *gl_texfmt = 4;
      *gl_format = GL_RGBA;
      *gl_type = GL_UNSIGNED_BYTE;
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

static void *setNull(const GLubyte *s) {
  return NULL;
}

static void *(*getProcAddress)(const GLubyte *procName) = NULL;

/**
 * \brief find the function pointers of some useful OpenGL extensions
 */
static void getFunctions() {
  if (!getProcAddress)
    getProcAddress = setNull;
  GenBuffers = getProcAddress("glGenBuffers");
  if (!GenBuffers)
    GenBuffers = getProcAddress("glGenBuffersARB");
  DeleteBuffers = getProcAddress("glDeleteBuffers");
  if (!DeleteBuffers)
    DeleteBuffers = getProcAddress("glDeleteBuffersARB");
  BindBuffer = getProcAddress("glBindBuffer");
  if (!BindBuffer)
    BindBuffer = getProcAddress("glBindBufferARB");
  MapBuffer = getProcAddress("glMapBuffer");
  if (!MapBuffer)
    MapBuffer = getProcAddress("glMapBufferARB");
  UnmapBuffer = getProcAddress("glUnmapBuffer");
  if (!UnmapBuffer)
    UnmapBuffer = getProcAddress("glUnmapBufferARB");
  BufferData = getProcAddress("glBufferData");
  if (!BufferData)
    BufferData = getProcAddress("glBufferDataARB");
  CombinerParameterfv = getProcAddress("glCombinerParameterfv");
  if (!CombinerParameterfv)
    CombinerParameterfv = getProcAddress("glCombinerParameterfvNV");
  CombinerParameteri = getProcAddress("glCombinerParameteri");
  if (!CombinerParameteri)
    CombinerParameteri = getProcAddress("glCombinerParameteriNV");
  CombinerInput = getProcAddress("glCombinerInput");
  if (!CombinerInput)
    CombinerInput = getProcAddress("glCombinerInputNV");
  CombinerOutput = getProcAddress("glCombinerOutput");
  if (!CombinerOutput)
    CombinerOutput = getProcAddress("glCombinerOutputNV");
  ActiveTexture = getProcAddress("glActiveTexture");
  if (!ActiveTexture)
    ActiveTexture = getProcAddress("glActiveTextureARB");
  BindTexture = getProcAddress("glBindTexture");
  if (!BindTexture)
    BindTexture = getProcAddress("glBindTextureARB");
  MultiTexCoord2f = getProcAddress("glMultiTexCoord2f");
  if (!MultiTexCoord2f)
    MultiTexCoord2f = getProcAddress("glMultiTexCoord2fARB");
  BindProgram = getProcAddress("glBindProgram");
  if (!BindProgram)
    BindProgram = getProcAddress("glBindProgramARB");
  if (!BindProgram)
    BindProgram = getProcAddress("glBindProgramNV");
  ProgramString = getProcAddress("glProgramString");
  if (!ProgramString)
    ProgramString = getProcAddress("glProgramStringARB");
  if (!ProgramString)
    ProgramString = getProcAddress("glProgramStringNV");
  ProgramEnvParameter4f = getProcAddress("glProgramEnvParameter4f");
  if (!ProgramEnvParameter4f)
    ProgramEnvParameter4f = getProcAddress("glProgramEnvParameter4fARB");
  if (!ProgramEnvParameter4f)
    ProgramEnvParameter4f = getProcAddress("glProgramEnvParameter4fNV");
  SwapInterval = getProcAddress("glXSwapInterval");
  if (!SwapInterval)
    SwapInterval = getProcAddress("glXSwapIntervalEXT");
  if (!SwapInterval)
    SwapInterval = getProcAddress("glXSwapIntervalSGI");
  if (!SwapInterval)
    SwapInterval = getProcAddress("wglSwapInterval");
  if (!SwapInterval)
    SwapInterval = getProcAddress("wglSwapIntervalEXT");
  if (!SwapInterval)
    SwapInterval = getProcAddress("wglSwapIntervalSGI");
}

/**
 * \brief create a texture and set some defaults
 * \param target texture taget, usually GL_TEXTURE_2D
 * \param fmt internal texture format
 * \param filter filter used for scaling, e.g. GL_LINEAR
 * \param w texture width
 * \param h texture height
 * \param val luminance value to fill texture with
 */
void glCreateClearTex(GLenum target, GLenum fmt, GLint filter,
                      int w, int h, unsigned char val) {
  GLfloat fval = (GLfloat)val / 255.0;
  GLfloat border[4] = {fval, fval, fval, fval};
  GLenum clrfmt = (fmt == GL_ALPHA) ? GL_ALPHA : GL_LUMINANCE;
  char *init = (char *)malloc(w * h);
  memset(init, val, w * h);
  glAdjustAlignment(w);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
  glTexImage2D(target, 0, fmt, w, h, 0, clrfmt, GL_UNSIGNED_BYTE, init);
  glTexParameterf(target, GL_TEXTURE_PRIORITY, 1.0);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Border texels should not be used with CLAMP_TO_EDGE
  // We set a sane default anyway.
  glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, border);
  free(init);
}

/**
 * \brief return the number of bytes oer pixel for the given format
 * \param format OpenGL format
 * \param type OpenGL type
 * \return bytes per pixel
 *
 * Does not handle all possible variants, just those used by MPlayer
 */
int glFmt2bpp(GLenum format, GLenum type) {
  switch (type) {
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
      return 1;
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
      return 2;
  }
  if (type != GL_UNSIGNED_BYTE)
    return 0; //not implemented
  switch (format) {
    case GL_LUMINANCE:
    case GL_ALPHA:
      return 1;
    case GL_RGB:
    case GL_BGR:
      return 3;
    case GL_RGBA:
    case GL_BGRA:
      return 4;
  }
  return 0; // unkown
}

/**
 * \brief upload a texture, handling things like stride and slices
 * \param target texture target, usually GL_TEXTURE_2D
 * \param format OpenGL format of data
 * \param type OpenGL type of data
 * \param data data to upload
 * \param stride data stride
 * \param x x offset in texture
 * \param y y offset in texture
 * \param w width of the texture part to upload
 * \param h height of the texture part to upload
 * \param slice height of an upload slice, 0 for all at once
 */
void glUploadTex(GLenum target, GLenum format, GLenum type,
                 const char *data, int stride,
                 int x, int y, int w, int h, int slice) {
  int y_max = y + h;
  if (w <= 0 || h <= 0) return;
  if (slice <= 0)
    slice = h;
  // this is not always correct, but should work for MPlayer
  glAdjustAlignment(stride);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / glFmt2bpp(format, type));
  for (; y + slice <= y_max; y += slice) {
    glTexSubImage2D(target, 0, x, y, w, slice, format, type, data);
    data += stride * slice;
  }
  if (y < y_max)
    glTexSubImage2D(target, 0, x, y, w, y_max - y, format, type, data);
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
 */
void glDrawTex(GLfloat x, GLfloat y, GLfloat w, GLfloat h,
               GLfloat tx, GLfloat ty, GLfloat tw, GLfloat th,
               int sx, int sy, int rect_tex) {
  if (!rect_tex) {
    tx /= sx; ty /= sy; tw /= sx; th /= sy;
  }
  glBegin(GL_QUADS);
  glTexCoord2f(tx, ty);
  glVertex2f(x, y);
  glTexCoord2f(tx, ty + th);
  glVertex2f(x, y + h);
  glTexCoord2f(tx + tw, ty + th);
  glVertex2f(x + w, y + h);
  glTexCoord2f(tx + tw, ty);
  glVertex2f(x + w, y);
  glEnd();
}

#ifdef GL_WIN32
static void *w32gpa(const GLubyte *procName) {
  return wglGetProcAddress(procName);
}

int setGlWindow(int *vinfo, HGLRC *context, HWND win)
{
  int new_vinfo;
  HDC windc = GetDC(win);
  HGLRC new_context = 0;
  int keep_context = 0;

  // should only be needed when keeping context, but not doing glFinish
  // can cause flickering even when we do not keep it.
  glFinish();
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
      return SET_WINDOW_FAILED;
    }
  }

  // set context
  if (!wglMakeCurrent(windc, new_context)) {
    mp_msg (MSGT_VO, MSGL_FATAL, "[gl] Could not set GL context!\n");
    if (!keep_context) {
      wglDeleteContext(new_context);
    }
    return SET_WINDOW_FAILED;
  }

  // set new values
  vo_window = win;
  vo_hdc = windc;
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
    getProcAddress = w32gpa;
    getFunctions();

    // and inform that reinit is neccessary
    return SET_WINDOW_REINIT;
  }
  return SET_WINDOW_OK;
}

void releaseGlContext(int *vinfo, HGLRC *context) {
  *vinfo = 0;
  if (*context) {
    wglMakeCurrent(0, 0);
    wglDeleteContext(*context);
  }
  *context = 0;
}
#else
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif
/**
 * \brief find address of a linked function
 * \param s name of function to find
 * \return address of function or NULL if not found
 *
 * Copied from xine
 */
static void *getdladdr(const GLubyte *s) {
#ifdef HAVE_LIBDL
#if defined(__sun) || defined(__sgi)
  static void *handle = dlopen(NULL, RTLD_LAZY);
  return dlsym(handle, s);
#else
  return dlsym(0, s);
#endif
#else
  return NULL;
#endif
}

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
 */
int setGlWindow(XVisualInfo **vinfo, GLXContext *context, Window win)
{
  XVisualInfo *new_vinfo;
  GLXContext new_context = NULL;
  int keep_context = 0;

  // should only be needed when keeping context, but not doing glFinish
  // can cause flickering even when we do not keep it.
  glFinish();
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
    mp_msg (MSGT_VO, MSGL_FATAL, "[gl] Could not set GLX context!\n");
    if (!keep_context) {
      glXDestroyContext (mDisplay, new_context);
      XFree(new_vinfo);
    }
    return SET_WINDOW_FAILED;
  }

  // set new values
  vo_window = win;
  {
    Window root;
    int tmp;
    XGetGeometry(mDisplay, vo_window, &root, &tmp, &tmp,
                  &vo_dwidth, &vo_dheight, &tmp, &tmp);
  }
  if (!keep_context) {
    if (*context)
      glXDestroyContext(mDisplay, *context);
    *context = new_context;
    if (*vinfo)
      XFree(*vinfo);
    *vinfo = new_vinfo;
    if (!getProcAddress)
      getProcAddress = getdladdr("glXGetProcAddress");
    if (!getProcAddress)
      getProcAddress = getdladdr("glXGetProcAddressARB");
    if (!getProcAddress)
      getProcAddress = getdladdr;
    getFunctions();

    // and inform that reinit is neccessary
    return SET_WINDOW_REINIT;
  }
  return SET_WINDOW_OK;
}

/**
 * \brief free the VisualInfo and GLXContext of an OpenGL context.
 */
void releaseGlContext(XVisualInfo **vinfo, GLXContext *context) {
  if (*vinfo)
    XFree(*vinfo);
  *vinfo = NULL;
  if (*context)
  {
    glFinish();
    glXMakeCurrent(mDisplay, None, NULL);
    glXDestroyContext(mDisplay, *context);
  }
  *context = 0;
}
#endif

