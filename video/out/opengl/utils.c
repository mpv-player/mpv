#include "common/msg.h"
#include "video/out/vo.h"
#include "utils.h"

// Standard parallel 2D projection, except y1 < y0 means that the coordinate
// system is flipped, not the projection.
void gl_transform_ortho(struct gl_transform *t, float x0, float x1,
                        float y0, float y1)
{
    if (y1 < y0) {
        float tmp = y0;
        y0 = tmp - y1;
        y1 = tmp;
    }

    t->m[0][0] = 2.0f / (x1 - x0);
    t->m[0][1] = 0.0f;
    t->m[1][0] = 0.0f;
    t->m[1][1] = 2.0f / (y1 - y0);
    t->t[0] = -(x1 + x0) / (x1 - x0);
    t->t[1] = -(y1 + y0) / (y1 - y0);
}

// Apply the effects of one transformation to another, transforming it in the
// process. In other words: post-composes t onto x
void gl_transform_trans(struct gl_transform t, struct gl_transform *x)
{
    struct gl_transform xt = *x;
    x->m[0][0] = t.m[0][0] * xt.m[0][0] + t.m[0][1] * xt.m[1][0];
    x->m[1][0] = t.m[1][0] * xt.m[0][0] + t.m[1][1] * xt.m[1][0];
    x->m[0][1] = t.m[0][0] * xt.m[0][1] + t.m[0][1] * xt.m[1][1];
    x->m[1][1] = t.m[1][0] * xt.m[0][1] + t.m[1][1] * xt.m[1][1];
    gl_transform_vec(t, &x->t[0], &x->t[1]);
}

// Create a texture and a FBO using the texture as color attachments.
//  fmt: texture internal format
// Returns success.
bool fbotex_init(struct fbotex *fbo, struct ra *ra, struct mp_log *log,
                 int w, int h, const struct ra_format *fmt)
{
    assert(!fbo->tex);
    return fbotex_change(fbo, ra, log, w, h, fmt, 0);
}

// Like fbotex_init(), except it can be called on an already initialized FBO;
// and if the parameters are the same as the previous call, do not touch it.
// flags can be 0, or a combination of FBOTEX_FUZZY_W and FBOTEX_FUZZY_H.
// Enabling FUZZY for W or H means the w or h does not need to be exact.
bool fbotex_change(struct fbotex *fbo, struct ra *ra, struct mp_log *log,
                   int w, int h, const struct ra_format *fmt, int flags)
{
    if (fbo->tex) {
        int cw = w, ch = h;
        int rw = fbo->tex->params.w, rh = fbo->tex->params.h;

        if ((flags & FBOTEX_FUZZY_W) && cw < rw)
            cw = rw;
        if ((flags & FBOTEX_FUZZY_H) && ch < rh)
            ch = rh;

        if (rw == cw && rh == ch && fbo->tex->params.format == fmt) {
            fbo->lw = w;
            fbo->lh = h;
            return true;
        }
    }

    int lw = w, lh = h;

    if (flags & FBOTEX_FUZZY_W)
        w = MP_ALIGN_UP(w, 256);
    if (flags & FBOTEX_FUZZY_H)
        h = MP_ALIGN_UP(h, 256);

    mp_verbose(log, "Create FBO: %dx%d (%dx%d)\n", lw, lh, w, h);

    if (!fmt || !fmt->renderable || !fmt->linear_filter) {
        mp_err(log, "Format %s not supported.\n", fmt ? fmt->name : "(unset)");
        return false;
    }

    fbotex_uninit(fbo);

    *fbo = (struct fbotex) {
        .ra = ra,
        .rw = w,
        .rh = h,
        .lw = lw,
        .lh = lh,
    };

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = w,
        .h = h,
        .d = 1,
        .format = fmt,
        .src_linear = true,
        .render_src = true,
        .render_dst = true,
    };

    fbo->tex = ra_tex_create(fbo->ra, &params);

    if (!fbo->tex) {
        mp_err(log, "Error: framebuffer could not be created.\n");
        fbotex_uninit(fbo);
        return false;
    }

    return true;
}

void fbotex_uninit(struct fbotex *fbo)
{
    if (fbo->ra) {
        ra_tex_free(fbo->ra, &fbo->tex);
        *fbo = (struct fbotex) {0};
    }
}

struct timer_pool {
    struct ra *ra;
    ra_timer *timer;
    bool running; // detect invalid usage

    uint64_t samples[PERF_SAMPLE_COUNT];
    int sample_idx;
    int sample_count;

    uint64_t avg_sum;
    uint64_t peak;
};

struct timer_pool *timer_pool_create(struct ra *ra)
{
    ra_timer *timer = ra->fns->timer_create(ra);
    if (!timer)
        return NULL;

    struct timer_pool *pool = talloc(NULL, struct timer_pool);
    if (!pool) {
        ra->fns->timer_destroy(ra, timer);
        return NULL;
    }

    *pool = (struct timer_pool){ .ra = ra, .timer = timer };
    return pool;
}

void timer_pool_destroy(struct timer_pool *pool)
{
    if (!pool)
        return;

    pool->ra->fns->timer_destroy(pool->ra, pool->timer);
    talloc_free(pool);
}

void timer_pool_start(struct timer_pool *pool)
{
    if (!pool)
        return;

    assert(!pool->running);
    pool->ra->fns->timer_start(pool->ra, pool->timer);
    pool->running = true;
}

void timer_pool_stop(struct timer_pool *pool)
{
    if (!pool)
        return;

    assert(pool->running);
    uint64_t res = pool->ra->fns->timer_stop(pool->ra, pool->timer);
    pool->running = false;

    if (res) {
        // Input res into the buffer and grab the previous value
        uint64_t old = pool->samples[pool->sample_idx];
        pool->samples[pool->sample_idx++] = res;
        pool->sample_idx %= PERF_SAMPLE_COUNT;

        // Update average and sum
        pool->avg_sum = pool->avg_sum + res - old;
        pool->sample_count = MPMIN(pool->sample_count + 1, PERF_SAMPLE_COUNT);

        // Update peak if necessary
        if (res >= pool->peak) {
            pool->peak = res;
        } else if (pool->peak == old) {
            // It's possible that the last peak was the value we just removed,
            // if so we need to scan for the new peak
            uint64_t peak = res;
            for (int i = 0; i < PERF_SAMPLE_COUNT; i++)
                peak = MPMAX(peak, pool->samples[i]);
            pool->peak = peak;
        }
    }
}

struct mp_pass_perf timer_pool_measure(struct timer_pool *pool)
{
    if (!pool)
        return (struct mp_pass_perf){0};

    struct mp_pass_perf res = {
        .count = pool->sample_count,
        .index = (pool->sample_idx - pool->sample_count) % PERF_SAMPLE_COUNT,
        .peak = pool->peak,
        .samples = pool->samples,
    };

    res.last = pool->samples[(pool->sample_idx - 1) % PERF_SAMPLE_COUNT];

    if (pool->sample_count > 0) {
        res.avg  = pool->avg_sum / pool->sample_count;
    }

    return res;
}
