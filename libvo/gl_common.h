#ifndef __GL_COMMON_H__
#define __GL_COMMON_H__

#include "mp_msg.h"
#include "config.h"

#include <GL/gl.h>
#include "video_out.h"

#ifdef GL_WIN32
#include <windows.h>
#include <GL/glext.h>
#include "w32_common.h"
#else
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "x11_common.h"
#endif

void glAdjustAlignment(int stride);

const char *glValName(GLint value);

int glFindFormat(uint32_t format, uint32_t *bpp, GLenum *gl_texfmt,
                  GLenum *gl_format, GLenum *gl_type);

//! could not set new window, will continue drawing into the old one.
#define SET_WINDOW_FAILED -1
//! new window is set, could even transfer the OpenGL context.
#define SET_WINDOW_OK 0
//! new window is set, but the OpenGL context needs to be reinitialized.
#define SET_WINDOW_REINIT 1

#ifdef GL_WIN32
int setGlWindow(int *vinfo, HGLRC *context, HWND win);
void releaseGlContext(int *vinfo, HGLRC *context);
#else
int setGlWindow(XVisualInfo **vinfo, GLXContext *context, Window win);
void releaseGlContext(XVisualInfo **vinfo, GLXContext *context);
#endif

#endif
