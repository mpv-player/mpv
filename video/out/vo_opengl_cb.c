#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include "config.h"

#include "talloc.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/options.h"
#include "aspect.h"
#include "vo.h"
#include "video/mp_image.h"
#include "sub/osd.h"
#include "osdep/timer.h"

#include "common/global.h"
#include "player/client.h"

#include "gl_common.h"
#include "gl_video.h"
#include "gl_hwdec.h"

#include "libmpv/opengl_cb.h"

/*
 * mpv_opengl_cb_context is created by the host application - the host application
 * can access it any time, even if the VO is destroyed (or not created yet).
 * The OpenGL object allows initializing the renderer etc. The VO object is only
 * here to transfer the video frames somehow.
 */

#define FRAME_DROP_POP      0 // drop the oldest frame in queue
#define FRAME_DROP_CLEAR    1 // drop all frames in queue

struct vo_priv {
    struct vo *vo;

    struct mpv_opengl_cb_context *ctx;

    // Immutable after VO init
    int use_gl_debug;
    struct gl_video_opts *renderer_opts;
    int frame_queue_size;
    int frame_drop_mode;
};

struct mpv_opengl_cb_context {
    struct mp_log *log;
    struct mp_client_api *client_api;

    pthread_mutex_t lock;

    // --- Protected by lock
    bool initialized;
    mpv_opengl_cb_update_fn update_cb;
    void *update_cb_ctx;
    struct mp_image *waiting_frame;
    struct mp_image **frame_queue;
    int queued_frames;
    struct mp_image_params img_params;
    bool reconfigured;
    int vp_w, vp_h;
    bool flip;
    bool force_update;
    bool imgfmt_supported[IMGFMT_END - IMGFMT_START];
    struct mp_vo_opts vo_opts;
    bool update_new_opts;
    struct vo_priv *new_opts; // use these options, instead of the VO ones
    struct m_config *new_opts_cfg;
    bool eq_changed;
    struct mp_csp_equalizer eq;
    int64_t recent_flip;

    // --- All of these can only be accessed from the thread where the host
    //     application's OpenGL context is current - i.e. only while the
    //     host application is calling certain mpv_opengl_cb_* APIs.
    GL *gl;
    struct gl_video *renderer;
    struct gl_hwdec *hwdec;
    struct mp_hwdec_info hwdec_info; // it's also semi-immutable after init

    // --- Immutable or semi-threadsafe.

    const char *hwapi;

    struct vo *active;
};

static void update(struct vo_priv *p);

// all queue manipulation functions shold be called under locked state

static struct mp_image *frame_queue_pop(struct mpv_opengl_cb_context *ctx)
{
    if (ctx->queued_frames == 0)
        return NULL;
    struct mp_image *ret = ctx->frame_queue[0];
    MP_TARRAY_REMOVE_AT(ctx->frame_queue, ctx->queued_frames, 0);
    return ret;
}

static void frame_queue_drop(struct mpv_opengl_cb_context *ctx)
{
    struct mp_image *mpi = frame_queue_pop(ctx);
    if (mpi) {
        talloc_free(mpi);
        if (ctx->active)
            vo_increment_drop_count(ctx->active, 1);
    }
}

static void frame_queue_clear(struct mpv_opengl_cb_context *ctx)
{
    for (int i = 0; i < ctx->queued_frames; i++)
        talloc_free(ctx->frame_queue[i]);
    talloc_free(ctx->frame_queue);
    ctx->frame_queue = NULL;
    ctx->queued_frames = 0;
}

static void frame_queue_drop_all(struct mpv_opengl_cb_context *ctx)
{
    int frames = ctx->queued_frames;
    frame_queue_clear(ctx);
    if (ctx->active && frames > 0)
        vo_increment_drop_count(ctx->active, frames);
}

static void frame_queue_push(struct mpv_opengl_cb_context *ctx, struct mp_image *mpi)
{
    MP_TARRAY_APPEND(ctx, ctx->frame_queue, ctx->queued_frames, mpi);
}

static void frame_queue_shrink(struct mpv_opengl_cb_context *ctx, int size)
{
    while (ctx->queued_frames > size)
        frame_queue_drop(ctx);
}

static void forget_frames(struct mpv_opengl_cb_context *ctx)
{
    frame_queue_clear(ctx);
    mp_image_unrefp(&ctx->waiting_frame);
}

static void free_ctx(void *ptr)
{
    mpv_opengl_cb_context *ctx = ptr;

    // This can trigger if the client API user doesn't call
    // mpv_opengl_cb_uninit_gl() properly.
    assert(!ctx->initialized);

    pthread_mutex_destroy(&ctx->lock);
}

struct mpv_opengl_cb_context *mp_opengl_create(struct mpv_global *g,
                                               struct mp_client_api *client_api)
{
    mpv_opengl_cb_context *ctx = talloc_zero(NULL, mpv_opengl_cb_context);
    talloc_set_destructor(ctx, free_ctx);
    pthread_mutex_init(&ctx->lock, NULL);

    ctx->gl = talloc_zero(ctx, GL);

    ctx->log = mp_log_new(ctx, g->log, "opengl-cb");
    ctx->client_api = client_api;

    switch (g->opts->hwdec_api) {
    case HWDEC_AUTO:    ctx->hwapi = "auto"; break;
    case HWDEC_VDPAU:   ctx->hwapi = "vdpau"; break;
    case HWDEC_VDA:     ctx->hwapi = "vda"; break;
    case HWDEC_VAAPI:   ctx->hwapi = "vaapi"; break;
    default:            ctx->hwapi = "";
    }

    return ctx;
}

// To be called from VO thread, with p->ctx->lock held.
static void copy_vo_opts(struct vo *vo)
{
    struct vo_priv *p = vo->priv;

    // We're being lazy: none of the options we need use dynamic data, so
    // copy the struct with an assignment.
    // Just remove all the dynamic data to avoid confusion.
    struct mp_vo_opts opts = *vo->opts;
    opts.video_driver_list = opts.vo_defs = NULL;
    opts.winname = NULL;
    opts.sws_opts = NULL;
    p->ctx->vo_opts = opts;
}

void mpv_opengl_cb_set_update_callback(struct mpv_opengl_cb_context *ctx,
                                      mpv_opengl_cb_update_fn callback,
                                      void *callback_ctx)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->update_cb = callback;
    ctx->update_cb_ctx = callback_ctx;
    pthread_mutex_unlock(&ctx->lock);
}

int mpv_opengl_cb_init_gl(struct mpv_opengl_cb_context *ctx, const char *exts,
                          mpv_opengl_cb_get_proc_address_fn get_proc_address,
                          void *get_proc_address_ctx)
{
    if (ctx->renderer)
        return MPV_ERROR_INVALID_PARAMETER;

    mpgl_load_functions2(ctx->gl, get_proc_address, get_proc_address_ctx,
                         exts, ctx->log);
    ctx->renderer = gl_video_init(ctx->gl, ctx->log);
    if (!ctx->renderer)
        return MPV_ERROR_UNSUPPORTED;

    ctx->hwdec = gl_hwdec_load_api(ctx->log, ctx->gl, ctx->hwapi);
    gl_video_set_hwdec(ctx->renderer, ctx->hwdec);
    if (ctx->hwdec)
        ctx->hwdec_info.hwctx = ctx->hwdec->hwctx;

    pthread_mutex_lock(&ctx->lock);
    ctx->eq = *gl_video_eq_ptr(ctx->renderer);
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        ctx->imgfmt_supported[n - IMGFMT_START] =
            gl_video_check_format(ctx->renderer, n);
    }
    ctx->initialized = true;
    pthread_mutex_unlock(&ctx->lock);

    gl_video_unset_gl_state(ctx->renderer);
    return 0;
}

int mpv_opengl_cb_uninit_gl(struct mpv_opengl_cb_context *ctx)
{
    // Bring down the decoder etc., which still might be using the hwdec
    // context. Setting initialized=false guarantees it can't come back.

    pthread_mutex_lock(&ctx->lock);
    forget_frames(ctx);
    ctx->initialized = false;
    pthread_mutex_unlock(&ctx->lock);

    kill_video(ctx->client_api);

    pthread_mutex_lock(&ctx->lock);
    assert(!ctx->active);
    pthread_mutex_unlock(&ctx->lock);

    gl_video_uninit(ctx->renderer);
    ctx->renderer = NULL;
    gl_hwdec_uninit(ctx->hwdec);
    ctx->hwdec = NULL;
    talloc_free(ctx->gl);
    ctx->gl = NULL;
    talloc_free(ctx->new_opts_cfg);
    ctx->new_opts = NULL;
    ctx->new_opts_cfg = NULL;
    return 0;
}

int mpv_opengl_cb_draw(mpv_opengl_cb_context *ctx, int fbo, int vp_w, int vp_h)
{
    assert(ctx->renderer);

    gl_video_set_gl_state(ctx->renderer);

    pthread_mutex_lock(&ctx->lock);

    struct vo *vo = ctx->active;

    ctx->force_update |= ctx->reconfigured;

    if (ctx->vp_w != vp_w || ctx->vp_h != vp_h)
        ctx->force_update = true;

    if (ctx->force_update && vo) {
        ctx->force_update = false;
        ctx->vp_w = vp_w;
        ctx->vp_h = vp_h;

        struct mp_rect src, dst;
        struct mp_osd_res osd;
        mp_get_src_dst_rects(ctx->log, &ctx->vo_opts, vo->driver->caps,
                             &ctx->img_params, vp_w, abs(vp_h),
                             1.0, &src, &dst, &osd);

        gl_video_resize(ctx->renderer, vp_w, vp_h, &src, &dst, &osd);
    }

    if (ctx->reconfigured) {
        gl_video_set_osd_source(ctx->renderer, vo ? vo->osd : NULL);
        gl_video_config(ctx->renderer, &ctx->img_params);
    }
    if (ctx->update_new_opts) {
        struct vo_priv *p = vo ? vo->priv : NULL;
        struct vo_priv *opts = ctx->new_opts ? ctx->new_opts : p;
        if (opts) {
            gl_video_set_options(ctx->renderer, opts->renderer_opts, NULL);
            ctx->gl->debug_context = opts->use_gl_debug;
            gl_video_set_debug(ctx->renderer, opts->use_gl_debug);
            frame_queue_shrink(ctx, opts->frame_queue_size);
        }
    }
    ctx->reconfigured = false;
    ctx->update_new_opts = false;

    struct mp_csp_equalizer *eq = gl_video_eq_ptr(ctx->renderer);
    if (ctx->eq_changed) {
        memcpy(eq->values, ctx->eq.values, sizeof(eq->values));
        gl_video_eq_update(ctx->renderer);
    }
    ctx->eq_changed = false;
    ctx->eq = *eq;

    struct mp_image *mpi = frame_queue_pop(ctx);

    pthread_mutex_unlock(&ctx->lock);

    if (mpi)
        gl_video_set_image(ctx->renderer, mpi);

    gl_video_render_frame(ctx->renderer, fbo, NULL);

    gl_video_unset_gl_state(ctx->renderer);

    pthread_mutex_lock(&ctx->lock);
    const int left = ctx->queued_frames;
    if (vo && left > 0)
        update(vo->priv);
    pthread_mutex_unlock(&ctx->lock);

    return left;
}

int mpv_opengl_cb_report_flip(mpv_opengl_cb_context *ctx, int64_t time)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->recent_flip = time > 0 ? time : mp_time_us();
    pthread_mutex_unlock(&ctx->lock);

    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct vo_priv *p = vo->priv;

    pthread_mutex_lock(&p->ctx->lock);
    mp_image_setrefp(&p->ctx->waiting_frame, mpi);
    talloc_free(mpi);
    pthread_mutex_unlock(&p->ctx->lock);
}

// Called locked.
static void update(struct vo_priv *p)
{
    if (p->ctx->update_cb)
        p->ctx->update_cb(p->ctx->update_cb_ctx);
}

static void flip_page(struct vo *vo)
{
    struct vo_priv *p = vo->priv;

    pthread_mutex_lock(&p->ctx->lock);
    if (p->ctx->queued_frames >= p->frame_queue_size) {
        if (p->frame_drop_mode == FRAME_DROP_CLEAR)
            frame_queue_drop_all(p->ctx);
        else // FRAME_DROP_POP mode
            frame_queue_shrink(p->ctx, p->frame_queue_size - 1);
    }
    frame_queue_push(p->ctx, p->ctx->waiting_frame);
    p->ctx->waiting_frame = NULL;
    update(p);
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

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct vo_priv *p = vo->priv;

    pthread_mutex_lock(&p->ctx->lock);
    forget_frames(p->ctx);
    p->ctx->img_params = *params;
    p->ctx->reconfigured = true;
    pthread_mutex_unlock(&p->ctx->lock);

    return 0;
}

// list of options which can be changed at runtime
#define OPT_BASE_STRUCT struct vo_priv
static const struct m_option change_opts[] = {
    OPT_FLAG("debug", use_gl_debug, 0),
    OPT_INTRANGE("frame-queue-size", frame_queue_size, 0, 1, 100, OPTDEF_INT(2)),
    OPT_CHOICE("frame-drop-mode", frame_drop_mode, 0,
               ({"pop", FRAME_DROP_POP},
                {"clear", FRAME_DROP_CLEAR})),
    OPT_SUBSTRUCT("", renderer_opts, gl_video_conf, 0),
    {0}
};
#undef OPT_BASE_STRUCT

static bool reparse_cmdline(struct vo_priv *p, char *args)
{
    struct m_config *cfg = NULL;
    struct vo_priv *opts = NULL;
    int r = 0;

    pthread_mutex_lock(&p->ctx->lock);
    const struct vo_priv *vodef = p->vo->driver->priv_defaults;
    cfg = m_config_new(NULL, p->vo->log, sizeof(*opts), vodef, change_opts);
    opts = cfg->optstruct;
    r = m_config_parse_suboptions(cfg, "opengl-cb", args);

    if (r >= 0) {
        talloc_free(p->ctx->new_opts_cfg);
        p->ctx->new_opts = opts;
        p->ctx->new_opts_cfg = cfg;
        p->ctx->update_new_opts = true;
        cfg = NULL;
        update(p);
    }

    talloc_free(cfg);
    pthread_mutex_unlock(&p->ctx->lock);
    return r >= 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct vo_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *args = data;
        pthread_mutex_lock(&p->ctx->lock);
        bool r = mp_csp_equalizer_get(&p->ctx->eq, args->name, args->valueptr) >= 0;
        pthread_mutex_unlock(&p->ctx->lock);
        return r ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_SET_EQUALIZER: {
        struct voctrl_set_equalizer_args *args = data;
        pthread_mutex_lock(&p->ctx->lock);
        bool r = mp_csp_equalizer_set(&p->ctx->eq, args->name, args->value) >= 0;
        if (r) {
            p->ctx->eq_changed = true;
            update(p);
        }
        pthread_mutex_unlock(&p->ctx->lock);
        return r ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_REDRAW_FRAME:
        pthread_mutex_lock(&p->ctx->lock);
        update(p);
        pthread_mutex_unlock(&p->ctx->lock);
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        pthread_mutex_lock(&p->ctx->lock);
        copy_vo_opts(vo);
        p->ctx->force_update = true;
        update(p);
        pthread_mutex_unlock(&p->ctx->lock);
        return VO_TRUE;
    case VOCTRL_SET_COMMAND_LINE: {
        char *arg = data;
        return reparse_cmdline(p, arg);
    }
    case VOCTRL_GET_HWDEC_INFO: {
        struct mp_hwdec_info **arg = data;
        *arg = p->ctx ? &p->ctx->hwdec_info : NULL;
        return true;
    }
    case VOCTRL_GET_RECENT_FLIP_TIME: {
        int r = VO_FALSE;
        pthread_mutex_lock(&p->ctx->lock);
        if (p->ctx->recent_flip) {
            *(int64_t *)data = p->ctx->recent_flip;
            r = VO_TRUE;
        }
        pthread_mutex_unlock(&p->ctx->lock);
        return r;
    }
    }

    return VO_NOTIMPL;
}

static void uninit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;

    pthread_mutex_lock(&p->ctx->lock);
    forget_frames(p->ctx);
    p->ctx->img_params = (struct mp_image_params){0};
    p->ctx->reconfigured = true;
    p->ctx->active = NULL;
    update(p);
    pthread_mutex_unlock(&p->ctx->lock);
}

static int preinit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;
    p->vo = vo;
    p->ctx = vo->extra.opengl_cb_context;
    if (!p->ctx) {
        MP_FATAL(vo, "No context set.\n");
        return -1;
    }

    pthread_mutex_lock(&p->ctx->lock);
    if (!p->ctx->initialized) {
        MP_FATAL(vo, "OpenGL context not initialized.\n");
        pthread_mutex_unlock(&p->ctx->lock);
        return -1;
    }
    p->ctx->active = vo;
    p->ctx->reconfigured = true;
    p->ctx->update_new_opts = true;
    copy_vo_opts(vo);
    pthread_mutex_unlock(&p->ctx->lock);

    return 0;
}

#define OPT_BASE_STRUCT struct vo_priv
static const struct m_option options[] = {
    OPT_FLAG("debug", use_gl_debug, 0),
    OPT_INTRANGE("frame-queue-size", frame_queue_size, 0, 1, 100, OPTDEF_INT(2)),
    OPT_CHOICE("frame-drop-mode", frame_drop_mode, 0,
               ({"pop", FRAME_DROP_POP},
                {"clear", FRAME_DROP_CLEAR})),
    OPT_SUBSTRUCT("", renderer_opts, gl_video_conf, 0),
    {0},
};

const struct vo_driver video_out_opengl_cb = {
    .description = "OpenGL Callbacks for libmpv",
    .name = "opengl-cb",
    .caps = VO_CAP_ROTATE90,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct vo_priv),
    .options = options,
};
