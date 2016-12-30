#ifndef MP_GL_EGL_HELPERS_H
#define MP_GL_EGL_HELPERS_H

#include <stdbool.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

struct mp_log;

bool mpegl_create_context(EGLDisplay display, struct mp_log *log, int vo_flags,
                          EGLContext *out_context, EGLConfig *out_config);

struct mpegl_opts {
    // combination of VOFLAG_* values.
    int vo_flags;

    // for callbacks
    void *user_data;

    // if set, pick the desired config from the given list and return its index
    // defaults to 0 (they are sorted by eglChooseConfig)
    int (*refine_config)(void *user_data, EGLConfig *configs, int num_configs);
};

bool mpegl_create_context_opts(EGLDisplay display, struct mp_log *log,
                               struct mpegl_opts *opts,
                               EGLContext *out_context, EGLConfig *out_config);

#endif
