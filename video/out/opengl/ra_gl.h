#pragma once

#include "common.h"
#include "utils.h"

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
