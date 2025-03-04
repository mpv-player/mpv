#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpv_talloc.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "misc/dispatch.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/options.h"
#include "aspect.h"
#include "dr_helper.h"
#include "vo.h"
#include "video/mp_image.h"
#include "sub/osd.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

#include "common/global.h"
#include "player/client.h"

#include "libmpv.h"

#if HAVE_MACOS_COCOA_CB
#include "osdep/mac/app_bridge.h"
#endif

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
 *  Locking:  mpv core > VO > mpv_render_context.lock > mp_client_api.lock
 *              > mpv_render_context.update_lock
 *  And: render thread > VO (wait for present)
 *       VO > render thread (wait for present done, via timeout)
 *
 *  Locking gets more complex with advanced_control enabled. Use
 *  mpv_render_context.dispatch with care; synchronous calls can add lock
 *  dependencies.
 */

struct vo_priv {
    struct mpv_render_context *ctx; // immutable after init
};

struct mpv_render_context {
    struct mp_log *log;
    struct mpv_global *global;
    struct mp_client_api *client_api;

    atomic_bool in_use;

    // --- Immutable after init
    struct mp_dispatch_queue *dispatch;
    bool advanced_control;
    struct dr_helper *dr;           // NULL if advanced_control disabled

    mp_mutex control_lock;
    // --- Protected by control_lock
    mp_render_cb_control_fn control_cb;
    void *control_cb_ctx;

    mp_mutex update_lock;
    mp_cond update_cond;     // paired with update_lock

    // --- Protected by update_lock
    mpv_render_update_fn update_cb;
    void *update_cb_ctx;

    mp_mutex lock;
    mp_cond video_wait;      // paired with lock

    // --- Protected by lock
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

const struct render_backend_fns *render_backends[] = {
    &render_backend_gpu,
    &render_backend_sw,
    NULL
};

static void update(struct mpv_render_context *ctx)
{
    mp_mutex_lock(&ctx->update_lock);
    if (ctx->update_cb)
        ctx->update_cb(ctx->update_cb_ctx);

    mp_cond_broadcast(&ctx->update_cond);
    mp_mutex_unlock(&ctx->update_lock);
}

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
    mp_cond_broadcast(&ctx->video_wait);
    if (all) {
        talloc_free(ctx->cur_frame);
        ctx->cur_frame = NULL;
    }
}

static void dispatch_wakeup(void *ptr)
{
    struct mpv_render_context *ctx = ptr;

    update(ctx);
}

static struct mp_image *render_get_image(void *ptr, int imgfmt, int w, int h,
                                         int stride_align, int flags)
{
    struct mpv_render_context *ctx = ptr;

    return ctx->renderer->fns->get_image(ctx->renderer, imgfmt, w, h, stride_align, flags);
}

int mpv_render_context_create(mpv_render_context **res, mpv_handle *mpv,
                              mpv_render_param *params)
{
    mpv_render_context *ctx = talloc_zero(NULL, mpv_render_context);
    mp_mutex_init(&ctx->control_lock);
    mp_mutex_init(&ctx->lock);
    mp_mutex_init(&ctx->update_lock);
    mp_cond_init(&ctx->update_cond);
    mp_cond_init(&ctx->video_wait);

    ctx->global = mp_client_get_global(mpv);
    ctx->client_api = ctx->global->client_api;
    ctx->log = mp_log_new(ctx, ctx->global->log, "libmpv_render");

    ctx->vo_opts_cache = m_config_cache_alloc(ctx, ctx->global, &vo_sub_opts);
    ctx->vo_opts = ctx->vo_opts_cache->opts;

    ctx->dispatch = mp_dispatch_create(ctx);
    mp_dispatch_set_wakeup_fn(ctx->dispatch, dispatch_wakeup, ctx);

    if (GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_ADVANCED_CONTROL, int, 0))
        ctx->advanced_control = true;

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

    if (ctx->renderer->fns->get_image && ctx->advanced_control)
        ctx->dr = dr_helper_create(ctx->dispatch, render_get_image, ctx);

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
    mp_mutex_lock(&ctx->update_lock);
    ctx->update_cb = callback;
    ctx->update_cb_ctx = callback_ctx;
    if (ctx->update_cb)
        ctx->update_cb(ctx->update_cb_ctx);
    mp_mutex_unlock(&ctx->update_lock);
}

void mp_render_context_set_control_callback(mpv_render_context *ctx,
                                            mp_render_cb_control_fn callback,
                                            void *callback_ctx)
{
    mp_mutex_lock(&ctx->control_lock);
    ctx->control_cb = callback;
    ctx->control_cb_ctx = callback_ctx;
    mp_mutex_unlock(&ctx->control_lock);
}

void mpv_render_context_free(mpv_render_context *ctx)
{
    if (!ctx)
        return;

    // From here on, ctx becomes invisible and cannot be newly acquired. Only
    // a VO could still hold a reference.
    mp_set_main_render_context(ctx->client_api, ctx, false);

    if (atomic_load(&ctx->in_use)) {
        // Start destroy the VO, and also bring down the decoder etc., which
        // still might be using the hwdec context or use DR images. The above
        // mp_set_main_render_context() call guarantees it can't come back (so
        // ctx->vo can't change to non-NULL).
        // In theory, this races with vo_libmpv exiting and another VO being
        // used, which is a harmless grotesque corner case.
        kill_video_async(ctx->client_api);

        while (atomic_load(&ctx->in_use)) {
            // As a nasty detail, we need to wait until the VO is released, but
            // also need to react to update() calls during it (the update calls
            // are supposed to trigger processing ctx->dispatch). We solve this
            // by making the VO uninit function call mp_dispatch_interrupt().
            //
            // Other than that, processing ctx->dispatch is needed to serve the
            // video decoder, which might still not be fully destroyed, and e.g.
            // performs calls to release DR images (or, as a grotesque corner
            // case may even try to allocate new ones).
            //
            // Once the VO is released, ctx->dispatch becomes truly inactive.
            // (The libmpv API user could call mpv_render_context_update() while
            // mpv_render_context_free() is being called, but of course this is
            // invalid.)
            mp_dispatch_queue_process(ctx->dispatch, INFINITY);
        }
    }

    mp_mutex_lock(&ctx->lock);
    // Barrier - guarantee uninit() has left the lock region. It will access ctx
    // until the lock has been released, so we must not proceed with destruction
    // before we can acquire the lock. (The opposite, uninit() acquiring the
    // lock, can not happen anymore at this point - we've waited for VO uninit,
    // and prevented that new VOs can be created.)
    mp_mutex_unlock(&ctx->lock);

    mp_assert(!atomic_load(&ctx->in_use));
    mp_assert(!ctx->vo);

    // With the dispatch queue not being served anymore, allow frame free
    // requests from this thread to be served directly.
    if (ctx->dr)
        dr_helper_acquire_thread(ctx->dr);

    // Possibly remaining outstanding work.
    mp_dispatch_queue_process(ctx->dispatch, 0);

    forget_frames(ctx, true);

    if (ctx->renderer) {
        ctx->renderer->fns->destroy(ctx->renderer);
        talloc_free(ctx->renderer->priv);
        talloc_free(ctx->renderer);
    }
    talloc_free(ctx->dr);
    talloc_free(ctx->dispatch);

    mp_cond_destroy(&ctx->update_cond);
    mp_cond_destroy(&ctx->video_wait);
    mp_mutex_destroy(&ctx->update_lock);
    mp_mutex_destroy(&ctx->lock);
    mp_mutex_destroy(&ctx->control_lock);

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
    mp_mutex_lock(&ctx->lock);

    int do_render =
        !GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_SKIP_RENDERING, int, 0);

    if (do_render) {
        int vp_w, vp_h;
        int err = ctx->renderer->fns->get_target_size(ctx->renderer, params,
                                                    &vp_w, &vp_h);
        if (err < 0) {
            mp_mutex_unlock(&ctx->lock);
            return err;
        }

        if (ctx->vo && (ctx->vp_w != vp_w || ctx->vp_h != vp_h ||
                        ctx->need_resize))
        {
            ctx->vp_w = vp_w;
            ctx->vp_h = vp_h;

            m_config_cache_update(ctx->vo_opts_cache);

            struct mp_rect src, dst;
            struct mp_osd_res osd;
            mp_get_src_dst_rects(ctx->log, ctx->vo_opts, ctx->vo->driver->caps,
                                &ctx->img_params, vp_w, abs(vp_h),
                                1.0, &src, &dst, &osd);

            ctx->renderer->fns->resize(ctx->renderer, &src, &dst, &osd);
        }
        ctx->need_resize = false;
    }

    if (ctx->need_reconfig)
        ctx->renderer->fns->reconfig(ctx->renderer, &ctx->img_params);
    ctx->need_reconfig = false;

    if (ctx->need_update_external)
        ctx->renderer->fns->update_external(ctx->renderer, ctx->vo);
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
        mp_cond_broadcast(&ctx->video_wait);
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

    mp_mutex_unlock(&ctx->lock);

    MP_STATS(ctx, "glcb-render");

    int err = 0;

    if (do_render)
        err = ctx->renderer->fns->render(ctx->renderer, params, frame);

    if (frame != &dummy)
        talloc_free(frame);

    if (GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME,
                             int, 1))
    {
        mp_mutex_lock(&ctx->lock);
        while (wait_present_count > ctx->present_count)
            mp_cond_wait(&ctx->video_wait, &ctx->lock);
        mp_mutex_unlock(&ctx->lock);
    }

    return err;
}

void mpv_render_context_report_swap(mpv_render_context *ctx)
{
    MP_STATS(ctx, "glcb-reportflip");

    mp_mutex_lock(&ctx->lock);
    ctx->flip_count += 1;
    mp_cond_broadcast(&ctx->video_wait);
    mp_mutex_unlock(&ctx->lock);
}

uint64_t mpv_render_context_update(mpv_render_context *ctx)
{
    uint64_t res = 0;

    mp_dispatch_queue_process(ctx->dispatch, 0);

    mp_mutex_lock(&ctx->lock);
    if (ctx->next_frame)
        res |= MPV_RENDER_UPDATE_FRAME;
    mp_mutex_unlock(&ctx->lock);
    return res;
}

int mpv_render_context_set_parameter(mpv_render_context *ctx,
                                     mpv_render_param param)
{
    return ctx->renderer->fns->set_parameter(ctx->renderer, param);
}

int mpv_render_context_get_info(mpv_render_context *ctx,
                                mpv_render_param param)
{
    int res = MPV_ERROR_NOT_IMPLEMENTED;
    mp_mutex_lock(&ctx->lock);

    switch (param.type) {
    case MPV_RENDER_PARAM_NEXT_FRAME_INFO: {
        mpv_render_frame_info *info = param.data;
        *info = (mpv_render_frame_info){0};
        struct vo_frame *frame = ctx->next_frame;
        if (frame) {
            info->flags =
                MPV_RENDER_FRAME_INFO_PRESENT |
                (frame->redraw ? MPV_RENDER_FRAME_INFO_REDRAW : 0) |
                (frame->repeat ? MPV_RENDER_FRAME_INFO_REPEAT : 0) |
                (frame->display_synced && !frame->redraw ?
                    MPV_RENDER_FRAME_INFO_BLOCK_VSYNC : 0);
            info->target_time = frame->pts;
        }
        res = 0;
        break;
    }
    default:;
    }

    mp_mutex_unlock(&ctx->lock);
    return res;
}

static bool draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;

    mp_mutex_lock(&ctx->lock);
    mp_assert(!ctx->next_frame);
    ctx->next_frame = vo_frame_ref(frame);
    ctx->expected_flip_count = ctx->flip_count + 1;
    ctx->redrawing = frame->redraw || !frame->current;
    mp_mutex_unlock(&ctx->lock);

    update(ctx);
    return VO_TRUE;
}

static void flip_page(struct vo *vo)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;
    int64_t until = mp_time_ns() + MP_TIME_MS_TO_NS(200);

    mp_mutex_lock(&ctx->lock);

    // Wait until frame was rendered
    while (ctx->next_frame) {
        if (mp_cond_timedwait_until(&ctx->video_wait, &ctx->lock, until)) {
            if (ctx->next_frame) {
                MP_VERBOSE(vo, "mpv_render_context_render() not being called "
                           "or stuck.\n");
                goto done;
            }
        }
    }

    // Unblock mpv_render_context_render().
    ctx->present_count += 1;
    mp_cond_broadcast(&ctx->video_wait);

    if (ctx->redrawing)
        goto done; // do not block for redrawing

    // Wait until frame was presented
    while (ctx->expected_flip_count > ctx->flip_count) {
        // mpv_render_report_swap() is declared as optional API.
        // Assume the user calls it consistently _if_ it's called at all.
        if (!ctx->flip_count)
            break;
        if (mp_cond_timedwait_until(&ctx->video_wait, &ctx->lock, until)) {
            MP_VERBOSE(vo, "mpv_render_report_swap() not being called.\n");
            goto done;
        }
    }

done:

    // Cleanup after the API user is not reacting, or is being unusually slow.
    if (ctx->next_frame) {
        talloc_free(ctx->cur_frame);
        ctx->cur_frame = ctx->next_frame;
        ctx->next_frame = NULL;
        ctx->present_count += 2;
        mp_cond_signal(&ctx->video_wait);
        vo_increment_drop_count(vo, 1);
    }

    mp_mutex_unlock(&ctx->lock);
}

static int query_format(struct vo *vo, int format)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;

    bool ok = false;
    mp_mutex_lock(&ctx->lock);
    if (format >= IMGFMT_START && format < IMGFMT_END)
        ok = ctx->imgfmt_supported[format - IMGFMT_START];
    mp_mutex_unlock(&ctx->lock);
    return ok;
}

static void run_control_on_render_thread(void *p)
{
    void **args = p;
    struct mpv_render_context *ctx = args[0];
    int request = (intptr_t)args[1];
    void *data = args[2];
    int ret = VO_NOTIMPL;

    switch (request) {
    case VOCTRL_SCREENSHOT: {
        mp_mutex_lock(&ctx->lock);
        struct vo_frame *frame = vo_frame_ref(ctx->cur_frame);
        mp_mutex_unlock(&ctx->lock);
        if (frame && ctx->renderer->fns->screenshot)
            ctx->renderer->fns->screenshot(ctx->renderer, frame, data);
        talloc_free(frame);
        break;
    }
    case VOCTRL_PERFORMANCE_DATA: {
        if (ctx->renderer->fns->perfdata) {
            ctx->renderer->fns->perfdata(ctx->renderer, data);
            ret = VO_TRUE;
        }
        break;
    }
    }

    *(int *)args[3] = ret;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;

    switch (request) {
    case VOCTRL_RESET:
        mp_mutex_lock(&ctx->lock);
        forget_frames(ctx, false);
        ctx->need_reset = true;
        mp_mutex_unlock(&ctx->lock);
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_PAUSE:
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        mp_mutex_lock(&ctx->lock);
        ctx->need_resize = true;
        mp_mutex_unlock(&ctx->lock);
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_UPDATE_RENDER_OPTS:
        mp_mutex_lock(&ctx->lock);
        ctx->need_update_external = true;
        mp_mutex_unlock(&ctx->lock);
        vo->want_redraw = true;
        return VO_TRUE;
    }

    // VOCTRLs to be run on the renderer thread (if possible at all).
    if (ctx->advanced_control) {
        switch (request) {
        case VOCTRL_SCREENSHOT:
        case VOCTRL_PERFORMANCE_DATA: {
            int ret;
            void *args[] = {ctx, (void *)(intptr_t)request, data, &ret};
            mp_dispatch_run(ctx->dispatch, run_control_on_render_thread, args);
            return ret;
        }
        }
    }

    int r = VO_NOTIMPL;
    mp_mutex_lock(&ctx->control_lock);
    if (ctx->control_cb) {
        int events = 0;
        r = p->ctx->control_cb(vo, p->ctx->control_cb_ctx,
                               &events, request, data);
        vo_event(vo, events);
    }
    mp_mutex_unlock(&ctx->control_lock);

    return r;
}

static struct mp_image *get_image(struct vo *vo, int imgfmt, int w, int h,
                                  int stride_align, int flags)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;

    if (ctx->dr)
        return dr_helper_get_image(ctx->dr, imgfmt, w, h, stride_align, flags);

    return NULL;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;

    mp_mutex_lock(&ctx->lock);
    forget_frames(ctx, true);
    ctx->img_params = *params;
    ctx->need_reconfig = true;
    ctx->need_resize = true;
    mp_mutex_unlock(&ctx->lock);

    control(vo, VOCTRL_RECONFIG, NULL);

    return 0;
}

static void uninit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;
    struct mpv_render_context *ctx = p->ctx;

    control(vo, VOCTRL_UNINIT, NULL);

    mp_mutex_lock(&ctx->lock);

    forget_frames(ctx, true);
    ctx->img_params = (struct mp_image_params){0};
    ctx->need_reconfig = true;
    ctx->need_resize = true;
    ctx->need_update_external = true;
    ctx->need_reset = true;
    ctx->vo = NULL;

    // The following do not normally need ctx->lock, however, ctx itself may
    // become invalid once we release ctx->lock.
    bool prev_in_use = atomic_exchange(&ctx->in_use, false);
    mp_assert(prev_in_use); // obviously must have been set
    mp_dispatch_interrupt(ctx->dispatch);

    mp_mutex_unlock(&ctx->lock);
}

static int preinit(struct vo *vo)
{
#if HAVE_MACOS_COCOA_CB
    cocoa_init_cocoa_cb();
#else
    if (vo->probing)
        return -1;
#endif

    struct vo_priv *p = vo->priv;

    struct mpv_render_context *ctx =
        mp_client_api_acquire_render_context(vo->global->client_api);
    p->ctx = ctx;

    if (!ctx) {
        if (!vo->probing)
            MP_FATAL(vo, "No render context set.\n");
        return -1;
    }

    mp_mutex_lock(&ctx->lock);
    ctx->vo = vo;
    ctx->need_resize = true;
    ctx->need_update_external = true;
    mp_mutex_unlock(&ctx->lock);

    vo->hwdec_devs = ctx->hwdec_devs;
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
    .get_image_ts = get_image,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct vo_priv),
};
