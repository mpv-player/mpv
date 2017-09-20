#pragma once

#include <stdbool.h>
#include <math.h>

#include "ra.h"
#include "context.h"

// A 3x2 matrix, with the translation part separate.
struct gl_transform {
    // row-major, e.g. in mathematical notation:
    //  | m[0][0] m[0][1] |
    //  | m[1][0] m[1][1] |
    float m[2][2];
    float t[2];
};

static const struct gl_transform identity_trans = {
    .m = {{1.0, 0.0}, {0.0, 1.0}},
    .t = {0.0, 0.0},
};

void gl_transform_ortho(struct gl_transform *t, float x0, float x1,
                        float y0, float y1);

// This treats m as an affine transformation, in other words m[2][n] gets
// added to the output.
static inline void gl_transform_vec(struct gl_transform t, float *x, float *y)
{
    float vx = *x, vy = *y;
    *x = vx * t.m[0][0] + vy * t.m[0][1] + t.t[0];
    *y = vx * t.m[1][0] + vy * t.m[1][1] + t.t[1];
}

struct mp_rect_f {
    float x0, y0, x1, y1;
};

// Semantic equality (fuzzy comparison)
static inline bool mp_rect_f_seq(struct mp_rect_f a, struct mp_rect_f b)
{
    return fabs(a.x0 - b.x0) < 1e-6 && fabs(a.x1 - b.x1) < 1e-6 &&
           fabs(a.y0 - b.y0) < 1e-6 && fabs(a.y1 - b.y1) < 1e-6;
}

static inline void gl_transform_rect(struct gl_transform t, struct mp_rect_f *r)
{
    gl_transform_vec(t, &r->x0, &r->y0);
    gl_transform_vec(t, &r->x1, &r->y1);
}

static inline bool gl_transform_eq(struct gl_transform a, struct gl_transform b)
{
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            if (a.m[x][y] != b.m[x][y])
                return false;
        }
    }

    return a.t[0] == b.t[0] && a.t[1] == b.t[1];
}

void gl_transform_trans(struct gl_transform t, struct gl_transform *x);

void gl_transform_ortho_fbo(struct gl_transform *t, struct ra_fbo fbo);

// A pool of buffers, which can grow as needed
struct ra_buf_pool {
    struct ra_buf_params current_params;
    struct ra_buf **buffers;
    int num_buffers;
    int index;
};

void ra_buf_pool_uninit(struct ra *ra, struct ra_buf_pool *pool);

// Note: params->initial_data is *not* supported
struct ra_buf *ra_buf_pool_get(struct ra *ra, struct ra_buf_pool *pool,
                               const struct ra_buf_params *params);

// Helper that wraps ra_tex_upload using texture upload buffers to ensure that
// params->buf is always set. This is intended for RA-internal usage.
bool ra_tex_upload_pbo(struct ra *ra, struct ra_buf_pool *pbo,
                       const struct ra_tex_upload_params *params);

// Layout rules for GLSL's packing modes
struct ra_layout std140_layout(struct ra_renderpass_input *inp);
struct ra_layout std430_layout(struct ra_renderpass_input *inp);

bool ra_tex_resize(struct ra *ra, struct mp_log *log, struct ra_tex **tex,
                   int w, int h, const struct ra_format *fmt);

// A wrapper around ra_timer that does result pooling, averaging etc.
struct timer_pool;

struct timer_pool *timer_pool_create(struct ra *ra);
void timer_pool_destroy(struct timer_pool *pool);
void timer_pool_start(struct timer_pool *pool);
void timer_pool_stop(struct timer_pool *pool);
struct mp_pass_perf timer_pool_measure(struct timer_pool *pool);

// print a multi line string with line numbers (e.g. for shader sources)
// log, lev: module and log level, as in mp_msg()
void mp_log_source(struct mp_log *log, int lev, const char *src);
