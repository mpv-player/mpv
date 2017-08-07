#pragma once

#include "common.h"
#include "ra.h"
#include "gl_utils.h"

// For ra.priv
struct ra_gl {
    GL *gl;
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

int ra_init_gl(struct ra *ra, GL *gl);
struct ra_tex *ra_create_wrapped_texture(struct ra *ra, GLuint gl_texture,
                                         GLenum gl_target, GLint gl_iformat,
                                         GLenum gl_format, GLenum gl_type,
                                         int w, int h);
struct ra_tex *ra_create_wrapped_fb(struct ra *ra, GLuint gl_fbo, int w, int h);

GL *ra_gl_get(struct ra *ra);
