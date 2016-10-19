#ifndef MP_GL_EGL_HELPERS_H
#define MP_GL_EGL_HELPERS_H

#include <stdbool.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

struct mp_log;

bool mpegl_create_context(EGLDisplay display, struct mp_log *log, int vo_flags,
                          EGLContext *out_context, EGLConfig *out_config);

#endif
