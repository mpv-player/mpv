#ifndef MP_GL_EGL_HELPERS_H
#define MP_GL_EGL_HELPERS_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

struct GL;
void mp_egl_get_depth(struct GL *gl, EGLConfig fbc);

#endif
