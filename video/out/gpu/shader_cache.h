#pragma once

#include "common/common.h"
#include "misc/bstr.h"
#include "ra.h"

// For mp_pass_perf
#include "video/out/vo.h"

struct mp_log;
struct mpv_global;
struct gl_shader_cache;

struct gl_shader_cache *gl_sc_create(struct ra *ra, struct mpv_global *global,
                                     struct mp_log *log);
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

// A hint that the next data-type (i.e. non-binding) uniform is expected to
// change frequently. This refers to the _f, _i, _vecN etc. uniform types.
void gl_sc_uniform_dynamic(struct gl_shader_cache *sc);
void gl_sc_uniform_texture(struct gl_shader_cache *sc, char *name,
                           struct ra_tex *tex);
void gl_sc_uniform_image2D_wo(struct gl_shader_cache *sc, const char *name,
                              struct ra_tex *tex);
void gl_sc_ssbo(struct gl_shader_cache *sc, char *name, struct ra_buf *buf,
                char *format, ...) PRINTF_ATTRIBUTE(4, 5);
void gl_sc_uniform_f(struct gl_shader_cache *sc, char *name, float f);
void gl_sc_uniform_i(struct gl_shader_cache *sc, char *name, int f);
void gl_sc_uniform_vec2(struct gl_shader_cache *sc, char *name, float f[2]);
void gl_sc_uniform_vec3(struct gl_shader_cache *sc, char *name, float f[3]);
void gl_sc_uniform_mat2(struct gl_shader_cache *sc, char *name,
                        bool transpose, float *v);
void gl_sc_uniform_mat3(struct gl_shader_cache *sc, char *name,
                        bool transpose, float *v);

// Return the correct bvecN() variant for using mix() in this GLSL version
const char *gl_sc_bvec(struct gl_shader_cache *sc, int dims);

void gl_sc_blend(struct gl_shader_cache *sc,
                 enum ra_blend blend_src_rgb,
                 enum ra_blend blend_dst_rgb,
                 enum ra_blend blend_src_alpha,
                 enum ra_blend blend_dst_alpha);
void gl_sc_enable_extension(struct gl_shader_cache *sc, char *name);
struct mp_pass_perf gl_sc_dispatch_draw(struct gl_shader_cache *sc,
                                        struct ra_tex *target, bool discard,
                                        const struct ra_renderpass_input *vao,
                                        int vao_len, size_t vertex_stride,
                                        void *ptr, size_t num);
struct mp_pass_perf gl_sc_dispatch_compute(struct gl_shader_cache *sc,
                                           int w, int h, int d);
// The application can call this on errors, to reset the current shader. This
// is normally done implicitly by gl_sc_dispatch_*
void gl_sc_reset(struct gl_shader_cache *sc);
void gl_sc_set_cache_dir(struct gl_shader_cache *sc, const char *dir);
