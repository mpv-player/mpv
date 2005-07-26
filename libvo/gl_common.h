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

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
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

extern void (APIENTRY *BindBuffer)(GLenum, GLuint);
extern GLvoid* (APIENTRY *MapBuffer)(GLenum, GLenum); 
extern GLboolean (APIENTRY *UnmapBuffer)(GLenum);
extern void (APIENTRY *BufferData)(GLenum, GLsizeiptr, const GLvoid *, GLenum);
extern void (APIENTRY *CombinerParameterfv)(GLenum, const GLfloat *);
extern void (APIENTRY *CombinerParameteri)(GLenum, GLint);
extern void (APIENTRY *CombinerInput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                                      GLenum);
extern void (APIENTRY *CombinerOutput)(GLenum, GLenum, GLenum, GLenum, GLenum,
                                       GLenum, GLenum, GLboolean, GLboolean,
                                       GLboolean);
extern void (APIENTRY *ActiveTexture)(GLenum);
extern void (APIENTRY *BindTexture)(GLenum, GLuint);
extern void (APIENTRY *MultiTexCoord2f)(GLenum, GLfloat, GLfloat);
extern void (APIENTRY *BindProgram)(GLenum, GLuint);
extern void (APIENTRY *ProgramString)(GLenum, GLenum, GLsizei, const GLvoid *);
extern void (APIENTRY *ProgramEnvParameter4f)(GLenum, GLuint, GLfloat, GLfloat,
                                              GLfloat, GLfloat);

#endif
