#ifndef MP_GL_EGL_HELPERS_H
#define MP_GL_EGL_HELPERS_H

#include <stdbool.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "video/out/gpu/context.h"

struct mp_log;

bool mpegl_create_context(struct ra_ctx *ctx, EGLDisplay display,
                          EGLContext *out_context, EGLConfig *out_config);

struct mpegl_cb {
    // if set, pick the desired config from the given list and return its index
    // defaults to 0 (they are sorted by eglChooseConfig). return a negative
    // number to indicate an error condition or that no suitable configs could
    // be found.
    int (*refine_config)(void *user_data, EGLConfig *configs, int num_configs);
    void *user_data;
};

bool mpegl_create_context_cb(struct ra_ctx *ctx, EGLDisplay display,
                             struct mpegl_cb cb, EGLContext *out_context,
                             EGLConfig *out_config);

struct GL;
void mpegl_load_functions(struct GL *gl, struct mp_log *log);

#endif
