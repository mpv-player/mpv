#pragma once

#include "common.h"
#include "ra.h"
#include "utils.h"

// For ra.priv
struct ra_gl {
    GL *gl;
};

// For ra_tex.priv
struct ra_tex_gl {
    GLenum target;
    GLuint texture;
    GLuint fbo; // 0 if no rendering requested
    // These 3 fields can be 0 if unknown.
    GLint internal_format;
    GLenum format;
    GLenum type;
    struct gl_pbo_upload pbo;
};

// For ra_mapped_buffer.priv
struct ra_mapped_buffer_gl {
    GLuint pbo;
    GLsync fence;
};

int ra_init_gl(struct ra *ra, GL *gl);
