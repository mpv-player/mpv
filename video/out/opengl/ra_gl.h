#pragma once

#include "common.h"
#include "ra.h"
#include "gl_utils.h"

// For ra.priv
struct ra_gl {
    GL *gl;
    bool debug_enable;
    bool timer_active; // hack for GL_TIME_ELAPSED limitations
};

// For ra_tex.priv
struct ra_tex_gl {
    bool own_objects;
    GLenum target;
    GLuint texture; // 0 if no texture data associated
    GLuint fbo; // 0 if no rendering requested, or it default framebuffer
    // These 3 fields can be 0 if unknown.
    GLint internal_format;
    GLenum format;
    GLenum type;
    struct gl_pbo_upload pbo;
};

// For ra_buf.priv
struct ra_buf_gl {
    GLuint buffer;
    GLsync fence;
};

// For ra_renderpass.priv
struct ra_renderpass_gl {
    GLuint program;
    // 1 entry for each ra_renderpass_params.inputs[] entry
    GLint *uniform_loc;
    int num_uniform_loc; // == ra_renderpass_params.num_inputs
    struct gl_vao vao;
};

struct ra *ra_create_gl(GL *gl, struct mp_log *log);
struct ra_tex *ra_create_wrapped_tex(struct ra *ra,
                                     const struct ra_tex_params *params,
                                     GLuint gl_texture);
struct ra_tex *ra_create_wrapped_fb(struct ra *ra, GLuint gl_fbo, int w, int h);
GL *ra_gl_get(struct ra *ra);
void ra_gl_set_debug(struct ra *ra, bool enable);
void ra_gl_get_format(const struct ra_format *fmt, GLint *out_internal_format,
                      GLenum *out_format, GLenum *out_type);
void ra_gl_get_raw_tex(struct ra *ra, struct ra_tex *tex,
                       GLuint *out_texture, GLenum *out_target);
bool ra_is_gl(struct ra *ra);
