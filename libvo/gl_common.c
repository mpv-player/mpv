#include "gl_common.h"

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

#include "img_format.h"

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
#ifdef GL_VERSION_1_2
  MAP(GL_BGR), MAP(GL_BGRA),
#endif

  //type
  MAP(GL_BYTE), MAP(GL_UNSIGNED_BYTE), MAP(GL_SHORT), MAP(GL_UNSIGNED_SHORT),
  MAP(GL_INT), MAP(GL_UNSIGNED_INT), MAP(GL_FLOAT), MAP(GL_DOUBLE),
  MAP(GL_2_BYTES), MAP(GL_3_BYTES), MAP(GL_4_BYTES),
  // rest 1.2 only
#ifdef GL_VERSION_1_2
  MAP(GL_UNSIGNED_BYTE_3_3_2), MAP(GL_UNSIGNED_BYTE_2_3_3_REV),
  MAP(GL_UNSIGNED_SHORT_5_6_5), MAP(GL_UNSIGNED_SHORT_5_6_5_REV),
  MAP(GL_UNSIGNED_SHORT_4_4_4_4), MAP(GL_UNSIGNED_SHORT_4_4_4_4_REV),
  MAP(GL_UNSIGNED_SHORT_5_5_5_1), MAP(GL_UNSIGNED_SHORT_1_5_5_5_REV),
  MAP(GL_UNSIGNED_INT_8_8_8_8), MAP(GL_UNSIGNED_INT_8_8_8_8_REV),
  MAP(GL_UNSIGNED_INT_10_10_10_2), MAP(GL_UNSIGNED_INT_2_10_10_10_REV),
#endif
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

#undef TEXTUREFORMAT_ALWAYS
//! always return this format as internal texture format in glFindFormat
#define TEXTUREFORMAT_ALWAYS GL_RGB8

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
int glFindFormat(uint32_t fmt, uint32_t *bpp, GLenum *gl_texfmt,
                  GLenum *gl_format, GLenum *gl_type)
{
  int dummy1;
  GLenum dummy2;
  if (bpp == NULL) bpp = &dummy1;
  if (gl_texfmt == NULL) gl_texfmt = &dummy2;
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
    case IMGFMT_Y800:
    case IMGFMT_Y8:
      *gl_texfmt = 1;
      *bpp = 8;
      *gl_format = GL_LUMINANCE;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
#ifdef GL_VERSION_1_2
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
#endif
    default:
      *gl_texfmt = 4;
      *gl_format = GL_RGBA;
      *gl_type = GL_UNSIGNED_BYTE;
      return 0;
  }
#ifdef TEXTUREFORMAT_ALWAYS
  *gl_texfmt = TEXTUREFORMAT_ALWAYS;
#endif
  return 1;
}

#ifdef GL_WIN32
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
/**
 * Returns the XVisualInfo associated with Window win.
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

