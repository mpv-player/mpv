#pragma once

#include "common.h"
#include "gl_utils.h"
#include "misc/bstr.h"
#include "ra.h"

struct gl_shader_cache;

struct gl_shader_cache *gl_sc_create(GL *gl, struct mp_log *log);
void gl_sc_destroy(struct gl_shader_cache *sc);
bool gl_sc_error_state(struct gl_shader_cache *sc);
void gl_sc_reset_error(struct gl_shader_cache *sc);
void gl_sc_add(struct gl_shader_cache *sc, const char *text);
void gl_sc_addf(struct gl_shader_cache *sc, const char *textf, ...)
    PRINTF_ATTRIBUTE(2, 3);
void gl_sc_hadd(struct gl_shader_cache *sc, const char *text);
void gl_sc_haddf(struct gl_shader_cache *sc, const char *textf, ...)
    PRINTF_ATTRIBUTE(2, 3);
void gl_sc_hadd_bstr(struct gl_shader_cache *sc, struct bstr text);
void gl_sc_paddf(struct gl_shader_cache *sc, const char *textf, ...)
    PRINTF_ATTRIBUTE(2, 3);
void gl_sc_uniform_tex(struct gl_shader_cache *sc, char *name, GLenum target,
                       GLuint texture);
void gl_sc_uniform_texture(struct gl_shader_cache *sc, char *name,
                           struct ra_tex *tex);
void gl_sc_uniform_tex_ui(struct gl_shader_cache *sc, char *name, GLuint texture);
void gl_sc_uniform_image2D(struct gl_shader_cache *sc, const char *name,
                           GLuint texture, GLuint iformat, GLenum access);
void gl_sc_ssbo(struct gl_shader_cache *sc, char *name, GLuint ssbo,
                char *format, ...) PRINTF_ATTRIBUTE(4, 5);
void gl_sc_uniform_f(struct gl_shader_cache *sc, char *name, GLfloat f);
void gl_sc_uniform_i(struct gl_shader_cache *sc, char *name, GLint f);
void gl_sc_uniform_vec2(struct gl_shader_cache *sc, char *name, GLfloat f[2]);
void gl_sc_uniform_vec3(struct gl_shader_cache *sc, char *name, GLfloat f[3]);
void gl_sc_uniform_mat2(struct gl_shader_cache *sc, char *name,
                        bool transpose, GLfloat *v);
void gl_sc_uniform_mat3(struct gl_shader_cache *sc, char *name,
                        bool transpose, GLfloat *v);
void gl_sc_set_vertex_format(struct gl_shader_cache *sc,
                             const struct gl_vao_entry *entries,
                             size_t vertex_size);
void gl_sc_enable_extension(struct gl_shader_cache *sc, char *name);
struct mp_pass_perf gl_sc_generate(struct gl_shader_cache *sc, GLenum type);
void gl_sc_draw_data(struct gl_shader_cache *sc, GLenum prim, void *ptr,
                     size_t num);
void gl_sc_reset(struct gl_shader_cache *sc);
struct mpv_global;
void gl_sc_set_cache_dir(struct gl_shader_cache *sc, struct mpv_global *global,
                         const char *dir);

