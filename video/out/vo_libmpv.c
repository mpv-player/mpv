#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include "config.h"

#include "mpv_talloc.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/options.h"
#include "aspect.h"
#include "vo.h"
#include "video/mp_image.h"
#include "sub/osd.h"
#include "osdep/atomic.h"
#include "osdep/timer.h"

#include "common/global.h"
#include "player/client.h"

#include "libmpv.h"

/*
 * mpv_render_context is managed by the host application - the host application
 * can access it any time, even if the VO is destroyed (or not created yet).
 *
 * - the libmpv user can mix render API and normal API; thus render API
 *   functions can wait on the core, but not the reverse
 * - the core does blocking calls into the VO thread, thus the VO functions
 *   can't wait on the user calling the API functions
 * - to make video timing work like it should, the VO thread waits on the
 *   render API user anyway, and the (unlikely) deadlock is avoided with
 *   a timeout
 *
 *  So: mpv core > VO > mpv_render_context > mp_client_api.lock (locking)
 *  And: render thread > VO (wait for present)
 *       VO > render thread (wait for present done, via timeout)
 */

struct vo_priv {
    struct mpv_render_context *ctx;
};

struct mpv_render_context {
    struct mp_log *log;
    struct mpv_global *global;
    struct mp_client_api *client_api;

    atomic_bool in_use;

    pthread_mutex_t control_lock;
    mp_render_cb_control_fn control_cb;
    void *control_cb_ctx;

    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // --- Protected by lock
    mpv_render_update_fn update_cb;
    void *update_cb_ctx;
    struct vo_frame *next_frame;    // next frame to draw
    int64_t present_count;          // incremented when next frame can be shown
    int64_t expected_flip_count;    // next vsync event for next_frame
    bool redrawing;                 // next_frame was a redraw request
    int64_t flip_count;
    struct vo_frame *cur_frame;
    struct mp_image_params img_params;
    int vp_w, vp_h;
    bool flip;
    bool imgfmt_supported[IMGFMT_END - IMGFMT_START];
    bool need_reconfig;
    bool need_resize;
    bool need_reset;
    bool need_update_external;
    struct vo *vo;

    // --- Mostly immutable after init.
    struct mp_hwdec_devices *hwdec_devs;

    // --- All of these can only be accessed from mpv_render_*() API, for
    //     which the user makes sure they're called synchronized.
    struct render_backend *renderer;
    struct m_config_cache *vo_opts_cache;
    struct mp_vo_opts *vo_opts;
};

static void update(struct mpv_render_context *ctx);

const struct render_backend_fns *render_backends[] = {
    &render_backend_gpu,
    NULL
};

void *get_mpv_render_param(mpv_render_param *params, mpv_render_param_type type,
                           void *def)
{
    for (int n = 0; params && params[n].type; n++) {
        if (params[n].type == type)
            return params[n].data;
    }
    return def;
}

static void forget_frames(struct mpv_render_context *ctx, bool all)
{
    pthread_cond_broadcast(&ctx->wakeup);
    if (all) {
        talloc_free(ctx->cur_frame);
        ctx->cur_frame = NULL;
    }
}

int mpv_render_context_create(mpv_render_context **res, mpv_handle *mpv,
                              mpv_render_param *params)
{
    mpv_render_context *ctx = talloc_zero(NULL, mpv_render_context);
    pthread_mutex_init(&ctx->control_lock, NULL);
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->wakeup, NULL);

    ctx->global = mp_client_get_global(mpv);
    ctx->client_api = ctx->global->client_api;
    ctx->log = mp_log_new(ctx, ctx->global->log, "libmpv_render");

    ctx->vo_opts_cache = m_config_cache_alloc(ctx, ctx->global, &vo_sub_opts);
    ctx->vo_opts = ctx->vo_opts_cache->opts;

    int err = MPV_ERROR_NOT_IMPLEMENTED;
    for (int n = 0; render_backends[n]; n++) {
        ctx->renderer = talloc_zero(NULL, struct render_backend);
        *ctx->renderer = (struct render_backend){
            .global = ctx->global,
            .log = ctx->log,
            .fns = render_backends[n],
        };
        err = ctx->renderer->fns->init(ctx->renderer, params);
        if (err >= 0)
            break;
        ctx->renderer->fns->destroy(ctx->renderer);
        talloc_free(ctx->renderer->priv);
        TA_FREEP(&ctx->renderer);
        if (err != MPV_ERROR_NOT_IMPLEMENTED)
            break;
    }

    if (err < 0) {
        mpv_render_context_free(ctx);
        return err;
    }

    ctx->hwdec_devs = ctx->renderer->hwdec_devs;

    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        ctx->imgfmt_supported[n - IMGFMT_START] =
            ctx->renderer->fns->check_format(ctx->renderer, n);
    }

    if (!mp_set_main_render_context(ctx->client_api, ctx, true)) {
        MP_ERR(ctx, "There is already a mpv_render_context set.\n");
        mpv_render_context_free(ctx);
        return MPV_ERROR_GENERIC;
    }

    *res = ctx;
    return 0;
}

void mpv_render_context_set_update_callback(mpv_render_context *ctx,
                                            mpv_render_update_fn callback,
                                            void *callback_ctx)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->update_cb = callback;
    ctx->update_cb_ctx = callback_ctx;
    pthread_mutex_unlock(&ctx->lock);
}

void mp_render_context_set_control_callback(mpv_render_context *ctx,
                                            mp_render_cb_control_fn callback,
                                            void *callback_ctx)
{
    pthread_mutex_lock(&ctx->control_lock);
    ctx->control_cb = callback;
    ctx->control_cb_ctx = callback_ctx;
    pthread_mutex_unlock(&ctx->control_lock);
}

void mpv_render_context_free(mpv_render_context *ctx)
{
    if (!ctx)
        return;

    // From here on, ctx becomes invisible and cannot be newly acquired. Only
    // a VO could still hold a reference.
    mp_set_main_render_context(ctx->client_api, ctx, false);

    // If it's still in use, a VO using it must be active. Destroy the VO, and
    // also bring down the decoder etc., which still might be using the hwdec
    // context. The above removal guarantees it can't come back (so ctx->vo
    // can't change to non-NULL).
    if (atomic_load(&ctx->in_use))
        kill_video(ctx->client_api);

    assert(!atomic_load(&ctx->in_use));
    assert(!ctx->vo);

    forget_frames(ctx, true);

    ctx->renderer->fns->destroy(ctx->renderer);
    talloc_free(ctx->renderer->priv);
    talloc_free(ctx->renderer);

    pthread_cond_destroy(&ctx->wakeup);
    pthread_mutex_destroy(&ctx->lock);
    pthread_mutex_destroy(&ctx->control_lock);

    talloc_free(ctx);
}

// Try to mark the context as "in exclusive use" (e.g. by a VO).
// Note: the function must not acquire any locks, because it's called with an
// external leaf lock held.
bool mp_render_context_acquire(mpv_render_context *ctx)
{
    bool prev = false;
    return atomic_compare_exchange_strong(&ctx->in_use, &prev, true);
}

int mpv_render_context_render(mpv_render_context *ctx, mpv_render_param *params)
{
    int vp_w, vp_h;
    int err = ctx->renderer->fns->get_target_size(ctx->renderer, params,
                                                  &vp_w, &vp_h);
    if (err < 0)
        return err;

    pthread_mutex_lock(&ctx->lock);

    struct vo *vo = ctx->vo;

    if (vo && (ctx->vp_w != vp_w || ctx->vp_h != vp_h || ctx->need_resize)) {
        ctx->vp_w = vp_w;
        ctx->vp_h = vp_h;

        m_config_cache_update(ctx->vo_opts_cache);

        struct mp_rect src, dst;
        struct mp_osd_res osd;
        mp_get_src_dst_rects(ctx->log, ctx->vo_opts, vo->driver->caps,
                             &ctx->img_params, vp_w, abs(vp_h),
                             1.0, &src, &dst, &osd);

        ctx->renderer->fns->resize(ctx->renderer, &src, &dst, &osd);
    }
    ctx->need_resize = false;

    if (ctx->need_reconfig)
        ctx->renderer->fns->reconfig(ctx->renderer, &ctx->img_params);
    ctx->need_reconfig = false;

    if (ctx->need_update_external)
        ctx->renderer->fns->update_external(ctx->renderer, vo);
    ctx->need_update_external = false;

    if (ctx->need_reset) {
        ctx->renderer->fns->reset(ctx->renderer);
        if (ctx->cur_frame)
            ctx->cur_frame->still = true;
    }
    ctx->need_reset = false;

    struct vo_frame *frame = ctx->next_frame;
    int64_t wait_present_count = ctx->present_count;
    if (frame) {
        ctx->next_frame = NULL;
        if (!(frame->redraw || !frame->current))
            wait_present_count += 1;
        pthread_cond_signal(&ctx->wakeup);
        talloc_free(ctx->cur_frame);
        ctx->cur_frame = vo_frame_ref(frame);
    } else {
        frame = vo_frame_ref(ctx->cur_frame);
        if (frame)
            frame->redraw = true;
        MP_STATS(ctx, "glcb-noframe");
    }
    struct vo_frame dummy = {0};
    if (!frame)
        frame = &dummy;

    pthread_mutex_unlock(&ctx->lock);

    MP_STATS(ctx, "glcb-render");

    err = ctx->renderer->fns->render(ctx->renderer, params, frame);

    if (frame != &dummy)
        talloc_free(frame);

    pthread_mutex_lock(&ctx->lock);
    while (wait_present_count > ctx->present_count)
        pthread_cond_wait(&ctx->wakeup, &ctx->lock);
    pthread_mutex_unlock(&ctx->lock);

    return err;
}

void mpv_render_context_report_swap(mpv_render_context *ctx)
{
    MP_STATS(ctx, "glcb-reportflip");

    pthread_mutex_lock(&ctx->lock);
    ctx->flip_count += 1;
    pthread_cond_signal(&ctx->wakeup);
    pthread_mutex_unlock(&ctx->lock);
}

int mpv_render_context_set_parameter(mpv_render_context *ctx,
                                     mpv_render_param param)
{
    int err = ctx->renderer->fns->set_parameter(ctx->renderer, param);
    if (err >= 0) {
        // Might need to redraw.
        pthread_mutex_lock(&ctx->lock);
        if (ctx->vo)
            update(ctx);
        pthread_mutex_unlock(&ctx->lock);
    }
    return err;
}

// Called locked.
static void update(struct mpv_render_context *ctx)
{
    if (ctx->update_cb)
        ctx->update_cb(ctx->update_cb_ctx);
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct vo_priv *p = vo->priv;

    pthread_mutex_lock(&p->ctx->lock);
    assert(!p->ctx->next_frame);
    p->ctx->next_frame = vo_frame_ref(frame);
    p->ctx->expected_flip_count = p->ctx->flip_count + 1;
    p->ctx->redrawing = frame->redraw || !frame->current;
    update(p->ctx);
    pthread_mutex_unlock(&p->ctx->lock);
}

static void flip_page(struct vo *vo)
{
    struct vo_priv *p = vo->priv;
    struct timespec ts = mp_rel_time_to_timespec(0.2);

    pthread_mutex_lock(&p->ctx->lock);

    // Wait until frame was rendered
    while (p->ctx->next_frame) {
        if (pthread_cond_timedwait(&p->ctx->wakeup, &p->ctx->lock, &ts)) {
            if (p->ctx->next_frame) {
                MP_VERBOSE(vo, "mpv_render_context_render() not being called "
                           "or stuck.\n");
                goto done;
            }
        }
    }

    // Unblock mpv_render_context_render().
    p->ctx->present_count += 1;
    pthread_cond_signal(&p->ctx->wakeup);

    if (p->ctx->redrawing)
        goto done; // do not block for redrawing

    // Wait until frame was presented
    while (p->ctx->expected_flip_count > p->ctx->flip_count) {
        // mpv_render_report_swap() is declared as optional API.
        // Assume the user calls it consistently _if_ it's called at all.
        if (!p->ctx->flip_count)
            break;
        if (pthread_cond_timedwait(&p->ctx->wakeup, &p->ctx->lock, &ts)) {
            MP_VERBOSE(vo, "mpv_render_report_swap() not being called.\n");
            goto done;
        }
    }

done:

    // Cleanup after the API user is not reacting, or is being unusually slow.
    if (p->ctx->next_frame) {
        talloc_free(p->ctx->cur_frame);
        p->ctx->cur_frame = p->ctx->next_frame;
        p->ctx->next_frame = NULL;
        p->ctx->present_count += 2;
        pthread_cond_signal(&p->ctx->wakeup);
        vo_increment_drop_count(vo, 1);
    }

    pthread_mutex_unlock(&p->ctx->lock);
}

static int query_format(struct vo *vo, int format)
{
    struct vo_priv *p = vo->priv;

    bool ok = false;
    pthread_mutex_lock(&p->ctx->lock);
    if (format >= IMGFMT_START && format < IMGFMT_END)
        ok = p->ctx->imgfmt_supported[format - IMGFMT_START];
    pthread_mutex_unlock(&p->ctx->lock);
    return ok;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct vo_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_RESET:
        pthread_mutex_lock(&p->ctx->lock);
        forget_frames(p->ctx, false);
        p->ctx->need_reset = true;
        update(p->ctx);
        pthread_mutex_unlock(&p->ctx->lock);
        return VO_TRUE;
    case VOCTRL_PAUSE:
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_SET_EQUALIZER:
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        pthread_mutex_lock(&p->ctx->lock);
        p->ctx->need_resize = true;
        update(p->ctx);
        pthread_mutex_unlock(&p->ctx->lock);
        return VO_TRUE;
    case VOCTRL_UPDATE_RENDER_OPTS:
        pthread_mutex_lock(&p->ctx->lock);
        p->ctx->need_update_external = true;
        update(p->ctx);
        pthread_mutex_unlock(&p->ctx->lock);
        return VO_TRUE;
    }

    int r = VO_NOTIMPL;
    pthread_mutex_lock(&p->ctx->control_lock);
    if (p->ctx->control_cb) {
        int events = 0;
        r = p->ctx->control_cb(p->ctx->control_cb_ctx, &events, request, data);
        vo_event(vo, events);
    }
    pthread_mutex_unlock(&p->ctx->control_lock);

    return r;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct vo_priv *p = vo->priv;

    pthread_mutex_lock(&p->ctx->lock);
    forget_frames(p->ctx, true);
    p->ctx->img_params = *params;
    p->ctx->need_reconfig = true;
    p->ctx->need_resize = true;
    pthread_mutex_unlock(&p->ctx->lock);
    control(vo, VOCTRL_RECONFIG, NULL);

    return 0;
}

static void uninit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;

    control(vo, VOCTRL_UNINIT, NULL);
    pthread_mutex_lock(&p->ctx->lock);
    forget_frames(p->ctx, true);
    p->ctx->img_params = (struct mp_image_params){0};
    p->ctx->need_reconfig = true;
    p->ctx->need_resize = true;
    p->ctx->need_update_external = true;
    p->ctx->need_reset = true;
    p->ctx->vo = NULL;
    update(p->ctx);
    pthread_mutex_unlock(&p->ctx->lock);

    bool state = atomic_exchange(&p->ctx->in_use, false);
    assert(state); // obviously must have been set
}

static int preinit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;

    p->ctx = mp_client_api_acquire_render_context(vo->global->client_api);

    if (!p->ctx) {
        if (!vo->probing)
            MP_FATAL(vo, "No render context set.\n");
        return -1;
    }

    pthread_mutex_lock(&p->ctx->lock);
    p->ctx->vo = vo;
    p->ctx->need_resize = true;
    p->ctx->need_update_external = true;
    pthread_mutex_unlock(&p->ctx->lock);

    vo->hwdec_devs = p->ctx->hwdec_devs;
    control(vo, VOCTRL_PREINIT, NULL);

    return 0;
}

const struct vo_driver video_out_libmpv = {
    .description = "render API for libmpv",
    .name = "libmpv",
    .caps = VO_CAP_ROTATE90,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct vo_priv),
};
