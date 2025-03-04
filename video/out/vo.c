/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpv_talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "misc/dispatch.h"
#include "misc/rendezvous.h"
#include "options/options.h"
#include "misc/bstr.h"
#include "vo.h"
#include "aspect.h"
#include "dr_helper.h"
#include "input/input.h"
#include "options/m_config.h"
#include "common/msg.h"
#include "common/global.h"
#include "common/stats.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "sub/osd.h"
#include "osdep/io.h"
#include "osdep/threads.h"

extern const struct vo_driver video_out_mediacodec_embed;
extern const struct vo_driver video_out_x11;
extern const struct vo_driver video_out_vdpau;
extern const struct vo_driver video_out_xv;
extern const struct vo_driver video_out_gpu;
extern const struct vo_driver video_out_gpu_next;
extern const struct vo_driver video_out_libmpv;
extern const struct vo_driver video_out_null;
extern const struct vo_driver video_out_image;
extern const struct vo_driver video_out_lavc;
extern const struct vo_driver video_out_caca;
extern const struct vo_driver video_out_drm;
extern const struct vo_driver video_out_direct3d;
extern const struct vo_driver video_out_sdl;
extern const struct vo_driver video_out_vaapi;
extern const struct vo_driver video_out_dmabuf_wayland;
extern const struct vo_driver video_out_wlshm;
extern const struct vo_driver video_out_tct;
extern const struct vo_driver video_out_sixel;
extern const struct vo_driver video_out_kitty;

static const struct vo_driver *const video_out_drivers[] =
{
#if HAVE_ANDROID
    &video_out_mediacodec_embed,
#endif
    &video_out_gpu,
    &video_out_gpu_next,
#if HAVE_VDPAU
    &video_out_vdpau,
#endif
#if HAVE_DIRECT3D
    &video_out_direct3d,
#endif
#if HAVE_WAYLAND && HAVE_MEMFD_CREATE
    &video_out_wlshm,
#endif
#if HAVE_XV
    &video_out_xv,
#endif
#if HAVE_SDL2_VIDEO
    &video_out_sdl,
#endif
#if HAVE_DMABUF_WAYLAND
    &video_out_dmabuf_wayland,
#endif
#if HAVE_VAAPI_X11 && HAVE_GPL
    &video_out_vaapi,
#endif
#if HAVE_X11
    &video_out_x11,
#endif
    &video_out_libmpv,
    &video_out_null,
    // should not be auto-selected
    &video_out_image,
    &video_out_tct,
#if HAVE_CACA
    &video_out_caca,
#endif
#if HAVE_DRM
    &video_out_drm,
#endif
#if HAVE_SIXEL
    &video_out_sixel,
#endif
    &video_out_kitty,
    &video_out_lavc,
};

struct vo_internal {
    mp_thread thread;
    struct mp_dispatch_queue *dispatch;
    struct dr_helper *dr_helper;

    // --- The following fields are protected by lock
    mp_mutex lock;
    mp_cond wakeup;

    bool need_wakeup;
    bool terminate;

    bool hasframe;
    bool hasframe_rendered;
    bool request_redraw;            // redraw request from player to VO
    bool want_redraw;               // redraw request from VO to player
    bool send_reset;                // send VOCTRL_RESET
    bool paused;
    bool visible;
    bool wakeup_on_done;
    int queued_events;              // event mask for the user
    int internal_events;            // event mask for us

    double nominal_vsync_interval;

    double vsync_interval;
    int64_t *vsync_samples;
    int num_vsync_samples;
    int64_t num_total_vsync_samples;
    int64_t prev_vsync;
    double base_vsync;
    int drop_point;
    double estimated_vsync_interval;
    double estimated_vsync_jitter;
    bool expecting_vsync;
    int64_t num_successive_vsyncs;

    int64_t flip_queue_offset; // queue flip events at most this much in advance
    int64_t timing_offset;     // same (but from options; not VO configured)

    int64_t delayed_count;
    int64_t drop_count;
    bool dropped_frame;             // the previous frame was dropped

    struct vo_frame *current_frame; // last frame queued to the VO

    int64_t wakeup_pts;             // time at which to pull frame from decoder

    bool rendering;                 // true if an image is being rendered
    struct vo_frame *frame_queued;  // should be drawn next
    int req_frames;                 // VO's requested value of num_frames
    uint64_t current_frame_id;

    double display_fps;
    double reported_display_fps;

    struct stats_ctx *stats;
};

extern const struct m_sub_options gl_video_conf;

static void forget_frames(struct vo *vo);
static MP_THREAD_VOID vo_thread(void *ptr);

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(video_out_drivers))
        return false;
    const struct vo_driver *vo = video_out_drivers[index];
    *dst = (struct m_obj_desc) {
        .name = vo->name,
        .description = vo->description,
        .priv_size = vo->priv_size,
        .priv_defaults = vo->priv_defaults,
        .options = vo->options,
        .options_prefix = vo->options_prefix,
        .global_opts = vo->global_opts,
        .hidden = vo->encode,
        .p = vo,
    };
    return true;
}

// For the vo option
const struct m_obj_list vo_obj_list = {
    .get_desc = get_desc,
    .description = "video outputs",
    .aliases = {
        {"gl", "gpu"},
        {"direct3d_shaders", "direct3d"},
        {"opengl", "gpu"},
        {"opengl-cb", "libmpv"},
        {0}
    },
    .allow_trailer = true,
    .disallow_positional_parameters = true,
    .use_global_options = true,
};

static void dispatch_wakeup_cb(void *ptr)
{
    struct vo *vo = ptr;
    vo_wakeup(vo);
}

// Initialize or update options from vo->opts
static void read_opts(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    mp_mutex_lock(&in->lock);
    in->timing_offset = (uint64_t)(MP_TIME_S_TO_NS(vo->opts->timing_offset));
    mp_mutex_unlock(&in->lock);
}

static void update_opts(void *p)
{
    struct vo *vo = p;

    if (m_config_cache_update(vo->opts_cache)) {
        read_opts(vo);
        if (vo->driver->control) {
            vo->driver->control(vo, VOCTRL_VO_OPTS_CHANGED, NULL);
            // "Legacy" update of video position related options.
            // Unlike VOCTRL_VO_OPTS_CHANGED, often not propagated to backends.
            vo->driver->control(vo, VOCTRL_SET_PANSCAN, NULL);
        }
    }
}

// Does not include thread- and VO uninit.
static void dealloc_vo(struct vo *vo)
{
    forget_frames(vo); // implicitly synchronized

    // These must be free'd before vo->in->dispatch.
    talloc_free(vo->opts_cache);
    talloc_free(vo->gl_opts_cache);
    talloc_free(vo->eq_opts_cache);
    mp_mutex_destroy(&vo->params_mutex);

    mp_mutex_destroy(&vo->in->lock);
    mp_cond_destroy(&vo->in->wakeup);
    talloc_free(vo);
}

static struct vo *vo_create(bool probing, struct mpv_global *global,
                            struct vo_extra *ex, char *name)
{
    mp_assert(ex->wakeup_cb);

    struct mp_log *log = mp_log_new(NULL, global->log, "vo");
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &vo_obj_list, bstr0(name))) {
        mp_msg(log, MSGL_ERR, "Video output %s not found!\n", name);
        talloc_free(log);
        return NULL;
    };
    struct vo *vo = talloc_ptrtype(NULL, vo);
    *vo = (struct vo) {
        .log = mp_log_new(vo, log, name),
        .driver = desc.p,
        .global = global,
        .encode_lavc_ctx = ex->encode_lavc_ctx,
        .input_ctx = ex->input_ctx,
        .osd = ex->osd,
        .monitor_par = 1,
        .extra = *ex,
        .probing = probing,
        .in = talloc(vo, struct vo_internal),
    };
    mp_mutex_init(&vo->params_mutex);
    talloc_steal(vo, log);
    *vo->in = (struct vo_internal) {
        .dispatch = mp_dispatch_create(vo),
        .req_frames = 1,
        .estimated_vsync_jitter = -1,
        .stats = stats_ctx_create(vo, global, "vo"),
    };
    mp_dispatch_set_wakeup_fn(vo->in->dispatch, dispatch_wakeup_cb, vo);
    mp_mutex_init(&vo->in->lock);
    mp_cond_init(&vo->in->wakeup);

    vo->opts_cache = m_config_cache_alloc(NULL, global, &vo_sub_opts);
    vo->opts = vo->opts_cache->opts;

    m_config_cache_set_dispatch_change_cb(vo->opts_cache, vo->in->dispatch,
                                          update_opts, vo);

    vo->gl_opts_cache = m_config_cache_alloc(NULL, global, &gl_video_conf);
    vo->eq_opts_cache = m_config_cache_alloc(NULL, global, &mp_csp_equalizer_conf);

    mp_input_set_mouse_transform(vo->input_ctx, NULL, NULL);
    if (vo->driver->encode != !!vo->encode_lavc_ctx)
        goto error;
    vo->priv = m_config_group_from_desc(vo, vo->log, global, &desc, name);
    if (!vo->priv)
        goto error;

    if (mp_thread_create(&vo->in->thread, vo_thread, vo))
        goto error;
    if (mp_rendezvous(vo, 0) < 0) { // init barrier
        mp_thread_join(vo->in->thread);
        goto error;
    }
    return vo;

error:
    dealloc_vo(vo);
    return NULL;
}

struct vo *init_best_video_out(struct mpv_global *global, struct vo_extra *ex)
{
    struct mp_vo_opts *opts = mp_get_config_group(NULL, global, &vo_sub_opts);
    struct m_obj_settings *vo_list = opts->video_driver_list;
    struct vo *vo = NULL;
    // first try the preferred drivers, with their optional subdevice param:
    if (vo_list && vo_list[0].name) {
        for (int n = 0; vo_list[n].name; n++) {
            // Something like "-vo name," allows fallback to autoprobing.
            if (strlen(vo_list[n].name) == 0)
                goto autoprobe;
            bool p = !!vo_list[n + 1].name;
            vo = vo_create(p, global, ex, vo_list[n].name);
            if (vo)
                goto done;
        }
        goto done;
    }
autoprobe:
    // now try the rest...
    for (int i = 0; i < MP_ARRAY_SIZE(video_out_drivers); i++) {
        const struct vo_driver *driver = video_out_drivers[i];
        if (driver == &video_out_null)
            break;
        vo = vo_create(true, global, ex, (char *)driver->name);
        if (vo)
            goto done;
    }
done:
    talloc_free(opts);
    return vo;
}

static void terminate_vo(void *p)
{
    struct vo *vo = p;
    struct vo_internal *in = vo->in;
    in->terminate = true;
}

void vo_destroy(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_dispatch_run(in->dispatch, terminate_vo, vo);
    mp_thread_join(vo->in->thread);
    dealloc_vo(vo);
}

// Wakeup the playloop to queue new video frames etc.
static void wakeup_core(struct vo *vo)
{
    vo->extra.wakeup_cb(vo->extra.wakeup_ctx);
}

// Drop timing information on discontinuities like seeking.
// Always called locked.
static void reset_vsync_timings(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    in->drop_point = 0;
    in->base_vsync = 0;
    in->expecting_vsync = false;
    in->num_successive_vsyncs = 0;
}

static double vsync_stddef(struct vo *vo, double ref_vsync)
{
    struct vo_internal *in = vo->in;
    double jitter = 0;
    for (int n = 0; n < in->num_vsync_samples; n++) {
        double diff = in->vsync_samples[n] - ref_vsync;
        jitter += diff * diff;
    }
    return sqrt(jitter / in->num_vsync_samples);
}

#define MAX_VSYNC_SAMPLES 1000
#define DELAY_VSYNC_SAMPLES 10

// Check if we should switch to measured average display FPS if it seems
// "better" then the system-reported one. (Note that small differences are
// handled as drift instead.)
static void check_estimated_display_fps(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    bool use_estimated = false;
    if (in->num_total_vsync_samples >= MAX_VSYNC_SAMPLES / 2 &&
        in->estimated_vsync_interval <= 1e9 / 20.0 &&
        in->estimated_vsync_interval >= 1e9 / 400.0)
    {
        for (int n = 0; n < in->num_vsync_samples; n++) {
            if (fabs(in->vsync_samples[n] - in->estimated_vsync_interval)
                >= in->estimated_vsync_interval / 4)
                goto done;
        }
        double mjitter = vsync_stddef(vo, in->estimated_vsync_interval);
        double njitter = vsync_stddef(vo, in->nominal_vsync_interval);
        if (mjitter * 1.01 < njitter)
            use_estimated = true;
        done: ;
    }
    if (use_estimated == (fabs(in->vsync_interval - in->nominal_vsync_interval) < 1e9)) {
        if (use_estimated) {
            MP_TRACE(vo, "adjusting display FPS to a value closer to %.3f Hz\n",
                       1e9 / in->estimated_vsync_interval);
        } else {
            MP_TRACE(vo, "switching back to assuming display fps = %.3f Hz\n",
                       1e9 / in->nominal_vsync_interval);
        }
    }
    in->vsync_interval = use_estimated ? in->estimated_vsync_interval
                                       : in->nominal_vsync_interval;
}

// Attempt to detect vsyncs delayed/skipped by the driver. This tries to deal
// with strong jitter too, because some drivers have crap vsync timing.
static void vsync_skip_detection(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    int window = 4;
    double t_r = in->prev_vsync, t_e = in->base_vsync, diff = 0.0, desync_early = 0.0;
    for (int n = 0; n < in->drop_point; n++) {
        diff += t_r - t_e;
        t_r -= in->vsync_samples[n];
        t_e -= in->vsync_interval;
        if (n == window + 1)
            desync_early = diff / window;
    }
    double desync = diff / in->num_vsync_samples;
    if (in->drop_point > window * 2 &&
        fabs(desync - desync_early) >= in->vsync_interval * 3 / 4)
    {
        // Assume a drop. An underflow can technically speaking not be a drop
        // (it's up to the driver what this is supposed to mean), but no reason
        // to treat it differently.
        in->base_vsync = in->prev_vsync;
        in->delayed_count += 1;
        in->drop_point = 0;
        MP_STATS(vo, "vo-delayed");
    }
    if (in->drop_point > 10)
        in->base_vsync += desync / 10;  // smooth out drift
}

// Always called locked.
static void update_vsync_timing_after_swap(struct vo *vo,
                                           struct vo_vsync_info *vsync)
{
    struct vo_internal *in = vo->in;

    int64_t vsync_time = vsync->last_queue_display_time;
    int64_t prev_vsync = in->prev_vsync;
    in->prev_vsync = vsync_time;

    if (!in->expecting_vsync) {
        reset_vsync_timings(vo);
        return;
    }

    in->num_successive_vsyncs++;
    if (in->num_successive_vsyncs <= DELAY_VSYNC_SAMPLES) {
        in->base_vsync = vsync_time;
        return;
    }

    if (vsync_time <= 0 || vsync_time <= prev_vsync) {
        in->prev_vsync = 0;
        in->base_vsync = 0;
        return;
    }

    if (prev_vsync <= 0)
        return;

    if (in->num_vsync_samples >= MAX_VSYNC_SAMPLES)
        in->num_vsync_samples -= 1;
    MP_TARRAY_INSERT_AT(in, in->vsync_samples, in->num_vsync_samples, 0,
                        vsync_time - prev_vsync);
    in->drop_point = MPMIN(in->drop_point + 1, in->num_vsync_samples);
    in->num_total_vsync_samples += 1;
    if (in->base_vsync) {
        in->base_vsync += in->vsync_interval;
    } else {
        in->base_vsync = vsync_time;
    }

    double avg = 0;
    for (int n = 0; n < in->num_vsync_samples; n++) {
        mp_assert(in->vsync_samples[n] > 0);
        avg += in->vsync_samples[n];
    }
    in->estimated_vsync_interval = avg / in->num_vsync_samples;
    in->estimated_vsync_jitter =
        vsync_stddef(vo, in->vsync_interval) / in->vsync_interval;

    check_estimated_display_fps(vo);
    vsync_skip_detection(vo);

    MP_STATS(vo, "value %f jitter", in->estimated_vsync_jitter);
    MP_STATS(vo, "value %f vsync-diff", MP_TIME_NS_TO_S(in->vsync_samples[0]));
}

// to be called from VO thread only
static void update_display_fps(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    if (in->internal_events & VO_EVENT_WIN_STATE) {
        in->internal_events &= ~(unsigned)VO_EVENT_WIN_STATE;

        mp_mutex_unlock(&in->lock);

        double fps = 0;
        vo->driver->control(vo, VOCTRL_GET_DISPLAY_FPS, &fps);

        mp_mutex_lock(&in->lock);

        in->reported_display_fps = fps;
    }

    double display_fps = vo->opts->display_fps_override;
    if (display_fps <= 0)
        display_fps = in->reported_display_fps;

    if (in->display_fps != display_fps) {
        in->nominal_vsync_interval =  display_fps > 0 ? 1e9 / display_fps : 0;
        in->vsync_interval = MPMAX(in->nominal_vsync_interval, 1);
        in->display_fps = display_fps;

        MP_VERBOSE(vo, "Assuming %f FPS for display sync.\n", display_fps);

        // make sure to update the player
        in->queued_events |= VO_EVENT_WIN_STATE;
        wakeup_core(vo);
    }

    mp_mutex_unlock(&in->lock);
}

static void check_vo_caps(struct vo *vo)
{
    int rot = vo->params->rotate;
    if (rot) {
        bool ok = rot % 90 ? false : (vo->driver->caps & VO_CAP_ROTATE90);
        if (!ok) {
           MP_WARN(vo, "Video is flagged as rotated by %d degrees, but the "
                   "video output does not support this.\n", rot);
        }
    }
}

static void run_reconfig(void *p)
{
    void **pp = p;
    struct vo *vo = pp[0];
    struct mp_image *img = pp[1];
    int *ret = pp[2];

    struct mp_image_params *params = &img->params;

    struct vo_internal *in = vo->in;

    MP_VERBOSE(vo, "reconfig to %s\n", mp_image_params_to_str(params));

    update_opts(vo);

    mp_image_params_get_dsize(params, &vo->dwidth, &vo->dheight);

    mp_mutex_lock(&vo->params_mutex);
    talloc_free(vo->params);
    vo->params = talloc_dup(vo, params);
    vo->has_peak_detect_values = false;
    mp_mutex_unlock(&vo->params_mutex);

    if (vo->driver->reconfig2) {
        *ret = vo->driver->reconfig2(vo, img);
    } else {
        *ret = vo->driver->reconfig(vo, vo->params);
    }
    vo->config_ok = *ret >= 0;
    if (vo->config_ok) {
        check_vo_caps(vo);
    } else {
        mp_mutex_lock(&vo->params_mutex);
        talloc_free(vo->params);
        vo->params = NULL;
        vo->has_peak_detect_values = false;
        mp_mutex_unlock(&vo->params_mutex);
    }

    mp_mutex_lock(&in->lock);
    talloc_free(in->current_frame);
    in->current_frame = NULL;
    forget_frames(vo);
    reset_vsync_timings(vo);
    mp_mutex_unlock(&in->lock);

    update_display_fps(vo);
}

int vo_reconfig(struct vo *vo, struct mp_image_params *params)
{
    int ret;
    struct mp_image dummy = {0};
    mp_image_set_params(&dummy, params);
    void *p[] = {vo, &dummy, &ret};
    mp_dispatch_run(vo->in->dispatch, run_reconfig, p);
    return ret;
}

int vo_reconfig2(struct vo *vo, struct mp_image *img)
{
    int ret;
    void *p[] = {vo, img, &ret};
    mp_dispatch_run(vo->in->dispatch, run_reconfig, p);
    return ret;
}

static void run_control(void *p)
{
    void **pp = p;
    struct vo *vo = pp[0];
    int request = (intptr_t)pp[1];
    void *data = pp[2];
    update_opts(vo);
    int ret = vo->driver->control(vo, request, data);
    if (pp[3])
        *(int *)pp[3] = ret;
}

int vo_control(struct vo *vo, int request, void *data)
{
    int ret;
    void *p[] = {vo, (void *)(intptr_t)request, data, &ret};
    mp_dispatch_run(vo->in->dispatch, run_control, p);
    return ret;
}

// Run vo_control() without waiting for a reply.
// (Only works for some VOCTRLs.)
void vo_control_async(struct vo *vo, int request, void *data)
{
    void *p[4] = {vo, (void *)(intptr_t)request, NULL, NULL};
    void **d = talloc_memdup(NULL, p, sizeof(p));

    switch (request) {
    case VOCTRL_UPDATE_PLAYBACK_STATE:
        d[2] = talloc_dup(d, (struct voctrl_playback_state *)data);
        break;
    case VOCTRL_KILL_SCREENSAVER:
    case VOCTRL_RESTORE_SCREENSAVER:
        break;
    default:
        abort(); // requires explicit support
    }

    mp_dispatch_enqueue_autofree(vo->in->dispatch, run_control, d);
}

// must be called locked
static void forget_frames(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    in->hasframe = false;
    in->hasframe_rendered = false;
    in->drop_count = 0;
    in->delayed_count = 0;
    talloc_free(in->frame_queued);
    in->frame_queued = NULL;
    in->current_frame_id += VO_MAX_REQ_FRAMES + 1;
    // don't unref current_frame; we always want to be able to redraw it
    if (in->current_frame) {
        in->current_frame->num_vsyncs = 0; // but reset future repeats
        in->current_frame->display_synced = false; // mark discontinuity
    }
}

// VOs which have no special requirements on UI event loops etc. can set the
// vo_driver.wait_events callback to this (and leave vo_driver.wakeup unset).
// This function must not be used or called for other purposes.
void vo_wait_default(struct vo *vo, int64_t until_time)
{
    struct vo_internal *in = vo->in;

    mp_mutex_lock(&in->lock);
    if (!in->need_wakeup)
        mp_cond_timedwait_until(&in->wakeup, &in->lock, until_time);
    mp_mutex_unlock(&in->lock);
}

// Called unlocked.
static void wait_vo(struct vo *vo, int64_t until_time)
{
    struct vo_internal *in = vo->in;

    if (vo->driver->wait_events) {
        vo->driver->wait_events(vo, until_time);
    } else {
        vo_wait_default(vo, until_time);
    }
    mp_mutex_lock(&in->lock);
    in->need_wakeup = false;
    mp_mutex_unlock(&in->lock);
}

static void wakeup_locked(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    mp_cond_broadcast(&in->wakeup);
    if (vo->driver->wakeup)
        vo->driver->wakeup(vo);
    in->need_wakeup = true;
}

// Wakeup VO thread, and make it check for new events with VOCTRL_CHECK_EVENTS.
// To be used by threaded VO backends.
void vo_wakeup(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    mp_mutex_lock(&in->lock);
    wakeup_locked(vo);
    mp_mutex_unlock(&in->lock);
}

static int64_t get_current_frame_end(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    if (!in->current_frame)
        return -1;
    return in->current_frame->pts + MPMAX(in->current_frame->duration, 0);
}

static int64_t get_display_synced_frame_end(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_assert(!in->frame_queued);
    int64_t res = 0;
    if (in->base_vsync && in->vsync_interval > 1 && in->current_frame) {
        res = in->base_vsync;
        int extra = !!in->rendering;
        res += (in->current_frame->num_vsyncs + extra) * in->vsync_interval;
        if (!in->current_frame->display_synced)
            res = 0;
    }
    return res;
}

static bool still_displaying(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    bool working = in->rendering || in->frame_queued;
    if (working)
        goto done;

    int64_t frame_end = get_display_synced_frame_end(vo);
    if (frame_end > 0) {
        working = frame_end > in->base_vsync;
        goto done;
    }

    frame_end = get_current_frame_end(vo);
    if (frame_end < 0)
        goto done;
    working = mp_time_ns() < frame_end;

done:
    return working && in->hasframe;
}

// Return true if there is still a frame being displayed (or queued).
bool vo_still_displaying(struct vo *vo)
{
    mp_mutex_lock(&vo->in->lock);
    bool res = still_displaying(vo);
    mp_mutex_unlock(&vo->in->lock);
    return res;
}

// Make vo issue a wakeup once vo_still_displaying() becomes false.
void vo_request_wakeup_on_done(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&vo->in->lock);
    if (still_displaying(vo)) {
        in->wakeup_on_done = true;
    } else {
        wakeup_core(vo);
    }
    mp_mutex_unlock(&vo->in->lock);
}

// Whether vo_queue_frame() can be called. If the VO is not ready yet, the
// function will return false, and the VO will call the wakeup callback once
// it's ready.
// next_pts is the exact time when the next frame should be displayed. If the
// VO is ready, but the time is too "early", return false, and call the wakeup
// callback once the time is right.
// If next_pts is negative, disable any timing and draw the frame as fast as
// possible.
bool vo_is_ready_for_frame(struct vo *vo, int64_t next_pts)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    bool blocked = vo->driver->initially_blocked &&
                   !(in->internal_events & VO_EVENT_INITIAL_UNBLOCK);
    bool r = vo->config_ok && !in->frame_queued && !blocked &&
             (!in->current_frame || in->current_frame->num_vsyncs < 1);
    if (r && next_pts >= 0) {
        // Don't show the frame too early - it would basically freeze the
        // display by disallowing OSD redrawing or VO interaction.
        // Actually render the frame at earliest the given offset before target
        // time.
        next_pts -= in->timing_offset;
        next_pts -= in->flip_queue_offset;
        int64_t now = mp_time_ns();
        if (next_pts > now)
            r = false;
        if (!in->wakeup_pts || next_pts < in->wakeup_pts) {
            in->wakeup_pts = next_pts;
            // If we have to wait, update the vo thread's timer.
            if (!r)
                wakeup_locked(vo);
        }
    }
    mp_mutex_unlock(&in->lock);
    return r;
}

// Check if the VO reports that the mpv window is visible.
bool vo_is_visible(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    bool r = in->visible;
    mp_mutex_unlock(&in->lock);
    return r;
}

// Direct the VO thread to put the currently queued image on the screen.
// vo_is_ready_for_frame() must have returned true before this call.
// Ownership of frame is handed to the vo.
void vo_queue_frame(struct vo *vo, struct vo_frame *frame)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    mp_assert(vo->config_ok && !in->frame_queued &&
           (!in->current_frame || in->current_frame->num_vsyncs < 1));
    in->hasframe = true;
    frame->frame_id = ++(in->current_frame_id);
    in->frame_queued = frame;
    in->wakeup_pts = frame->display_synced
                   ? 0 : frame->pts + MPMAX(frame->duration, 0);
    wakeup_locked(vo);
    mp_mutex_unlock(&in->lock);
}

// If a frame is currently being rendered (or queued), wait until it's done.
// Otherwise, return immediately.
void vo_wait_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    while (in->frame_queued || in->rendering)
        mp_cond_wait(&in->wakeup, &in->lock);
    mp_mutex_unlock(&in->lock);
}

// Wait until realtime is >= ts
// called without lock
static void wait_until(struct vo *vo, int64_t target)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    while (target > mp_time_ns()) {
        if (in->queued_events & VO_EVENT_LIVE_RESIZING)
            break;
        if (mp_cond_timedwait_until(&in->wakeup, &in->lock, target))
            break;
    }
    mp_mutex_unlock(&in->lock);
}

static bool render_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    struct vo_frame *frame = NULL;
    bool more_frames = false;

    update_display_fps(vo);

    mp_mutex_lock(&in->lock);

    if (in->frame_queued) {
        talloc_free(in->current_frame);
        in->current_frame = in->frame_queued;
        in->frame_queued = NULL;
    } else if (in->paused || !in->current_frame || !in->hasframe ||
               (in->current_frame->display_synced && in->current_frame->num_vsyncs < 1) ||
               !in->current_frame->display_synced)
    {
        goto done;
    }

    frame = vo_frame_ref(in->current_frame);
    mp_assert(frame);

    if (frame->display_synced) {
        frame->pts = 0;
        frame->duration = -1;
    }

    int64_t now = mp_time_ns();
    int64_t pts = frame->pts;
    int64_t duration = frame->duration;
    int64_t end_time = pts + duration;

    // Time at which we should flip_page on the VO.
    int64_t target = frame->display_synced ? 0 : pts - in->flip_queue_offset;

    // "normal" strict drop threshold.
    in->dropped_frame = duration >= 0 && end_time < now;

    in->dropped_frame &= !frame->display_synced;
    in->dropped_frame &= !(vo->driver->caps & VO_CAP_FRAMEDROP);
    in->dropped_frame &= frame->can_drop;
    // Even if we're hopelessly behind, rather degrade to 10 FPS playback,
    // instead of just freezing the display forever.
    in->dropped_frame &= now - in->prev_vsync < MP_TIME_MS_TO_NS(100);
    in->dropped_frame &= in->hasframe_rendered;

    // Setup parameters for the next time this frame is drawn. ("frame" is the
    // frame currently drawn, while in->current_frame is the potentially next.)
    in->current_frame->repeat = true;
    if (frame->display_synced) {
        // Increment the offset only if it's not the last vsync. The current_frame
        // can still be reused. This is mostly important for redraws that might
        // overshoot the target vsync point.
        if (in->current_frame->num_vsyncs > 1) {
            in->current_frame->vsync_offset += in->current_frame->vsync_interval;
            in->current_frame->ideal_frame_vsync += in->current_frame->ideal_frame_vsync_duration;
        }
        in->dropped_frame |= in->current_frame->num_vsyncs < 1;
    }
    if (in->current_frame->num_vsyncs > 0)
        in->current_frame->num_vsyncs -= 1;

    // Always render when paused (it's typically the last frame for a while).
    in->dropped_frame &= !in->paused;

    bool use_vsync = in->current_frame->display_synced && !in->paused;
    if (use_vsync && !in->expecting_vsync) // first DS frame in a row
        in->prev_vsync = now;
    in->expecting_vsync = use_vsync;

    // Store the initial value before we unlock.
    bool request_redraw = in->request_redraw;

    if (in->dropped_frame) {
        in->drop_count += 1;
        wakeup_core(vo);
    } else {
        in->rendering = true;
        in->hasframe_rendered = true;
        int64_t prev_drop_count = vo->in->drop_count;
        // Can the core queue new video now? Non-display-sync uses a separate
        // timer instead, but possibly benefits from preparing a frame early.
        bool can_queue = !in->frame_queued &&
            (in->current_frame->num_vsyncs < 1 || !use_vsync);
        mp_mutex_unlock(&in->lock);

        if (can_queue)
            wakeup_core(vo);

        stats_time_start(in->stats, "video-draw");

        in->visible = vo->driver->draw_frame(vo, frame);

        stats_time_end(in->stats, "video-draw");

        wait_until(vo, target);

        stats_time_start(in->stats, "video-flip");

        vo->driver->flip_page(vo);

        struct vo_vsync_info vsync = {
            .last_queue_display_time = -1,
            .skipped_vsyncs = -1,
        };
        if (vo->driver->get_vsync)
            vo->driver->get_vsync(vo, &vsync);

        // Make up some crap if presentation feedback is missing.
        if (vsync.last_queue_display_time <= 0)
            vsync.last_queue_display_time = mp_time_ns();

        stats_time_end(in->stats, "video-flip");

        mp_mutex_lock(&in->lock);
        in->dropped_frame = prev_drop_count < vo->in->drop_count;
        in->rendering = false;

        update_vsync_timing_after_swap(vo, &vsync);
    }

    if (vo->driver->caps & VO_CAP_NORETAIN) {
        talloc_free(in->current_frame);
        in->current_frame = NULL;
    }

    if (in->dropped_frame) {
        MP_STATS(vo, "drop-vo");
    } else {
        // If the initial redraw request was true or mpv is still playing,
        // then we can clear it here since we just performed a redraw, or the
        // next loop will draw what we need. However if there initially is
        // no redraw request, then something can change this (i.e. the OSD)
        // while the vo was unlocked. If we are paused, don't touch
        // in->request_redraw in that case.
        if (request_redraw || !in->paused)
            in->request_redraw = false;
    }

    if (in->current_frame && in->current_frame->num_vsyncs &&
        in->current_frame->display_synced)
        more_frames = true;

    if (in->frame_queued && in->frame_queued->display_synced)
        more_frames = true;

    mp_cond_broadcast(&in->wakeup); // for vo_wait_frame()

done:
    if (!vo->driver->frame_owner || in->dropped_frame)
        talloc_free(frame);
    mp_mutex_unlock(&in->lock);

    return more_frames;
}

static void do_redraw(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    if (!vo->config_ok || (vo->driver->caps & VO_CAP_NORETAIN))
        return;

    mp_mutex_lock(&in->lock);
    in->request_redraw = false;
    bool full_redraw = in->dropped_frame;
    struct vo_frame *frame = NULL;
    if (!vo->driver->untimed)
        frame = vo_frame_ref(in->current_frame);
    if (frame)
        in->dropped_frame = false;
    struct vo_frame dummy = {0};
    if (!frame)
        frame = &dummy;
    frame->redraw = !full_redraw; // unconditionally redraw if it was dropped
    frame->repeat = false;
    frame->still = true;
    frame->pts = 0;
    frame->duration = -1;
    mp_mutex_unlock(&in->lock);

    vo->driver->draw_frame(vo, frame);
    vo->driver->flip_page(vo);

    if (frame != &dummy && !vo->driver->frame_owner)
        talloc_free(frame);
}

static struct mp_image *get_image_vo(void *ctx, int imgfmt, int w, int h,
                                     int stride_align, int flags)
{
    struct vo *vo = ctx;
    return vo->driver->get_image(vo, imgfmt, w, h, stride_align, flags);
}

static MP_THREAD_VOID vo_thread(void *ptr)
{
    struct vo *vo = ptr;
    struct vo_internal *in = vo->in;
    bool vo_paused = false;

    mp_thread_set_name("vo");

    if (vo->driver->get_image) {
        in->dr_helper = dr_helper_create(in->dispatch, get_image_vo, vo);
        dr_helper_acquire_thread(in->dr_helper);
    }

    int r = vo->driver->preinit(vo) ? -1 : 0;
    mp_rendezvous(vo, r); // init barrier
    if (r < 0)
        goto done;

    read_opts(vo);
    update_display_fps(vo);
    vo_event(vo, VO_EVENT_WIN_STATE);

    while (1) {
        mp_dispatch_queue_process(vo->in->dispatch, 0);
        if (in->terminate)
            break;
        stats_event(in->stats, "iterations");
        vo->driver->control(vo, VOCTRL_CHECK_EVENTS, NULL);
        bool working = render_frame(vo);
        int64_t now = mp_time_ns();
        int64_t wait_until = now + MP_TIME_S_TO_NS(working ? 0 : 1000);
        bool wakeup_on_done = false;
        int64_t wakeup_core_after = 0;

        mp_mutex_lock(&in->lock);
        if (in->wakeup_pts) {
            if (in->wakeup_pts > now) {
                wait_until = MPMIN(wait_until, in->wakeup_pts);
            } else {
                in->wakeup_pts = 0;
                wakeup_core(vo);
            }
        }
        if (vo->want_redraw && !in->want_redraw) {
            in->want_redraw = true;
            wakeup_core(vo);
        }
        if ((!working && !in->rendering && !in->frame_queued) && in->wakeup_on_done) {
            // At this point we know VO is going to sleep
            int64_t frame_end = get_current_frame_end(vo);
            if (frame_end >= 0)
                wakeup_core_after = frame_end;
            wakeup_on_done = true;
            in->wakeup_on_done = false;
        }
        vo->want_redraw = false;
        bool redraw = in->request_redraw;
        bool send_reset = in->send_reset;
        in->send_reset = false;
        bool send_pause = in->paused != vo_paused;
        vo_paused = in->paused;
        mp_mutex_unlock(&in->lock);

        if (send_reset)
            vo->driver->control(vo, VOCTRL_RESET, NULL);
        if (send_pause)
            vo->driver->control(vo, vo_paused ? VOCTRL_PAUSE : VOCTRL_RESUME, NULL);
        if (wait_until > now && redraw) {
            vo->driver->control(vo, VOCTRL_REDRAW, NULL);
            do_redraw(vo); // now is a good time
            continue;
        }
        if (vo->want_redraw) // might have been set by VOCTRLs
            wait_until = 0;

        if (wait_until <= now)
            continue;

        if (wakeup_on_done) {
            // At this point wait_until should be longer than frame duration
            if (wakeup_core_after >= 0 && wait_until >= wakeup_core_after) {
                wait_vo(vo, wakeup_core_after);
                mp_mutex_lock(&in->lock);
                in->need_wakeup = true;
                mp_mutex_unlock(&in->lock);
            }
            wakeup_core(vo);
        }

        wait_vo(vo, wait_until);
    }
    forget_frames(vo); // implicitly synchronized
    talloc_free(in->current_frame);
    in->current_frame = NULL;
    vo->driver->uninit(vo);
done:
    TA_FREEP(&in->dr_helper);
    MP_THREAD_RETURN();
}

void vo_set_paused(struct vo *vo, bool paused)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    if (in->paused != paused) {
        in->paused = paused;
        if (in->paused && in->dropped_frame) {
            in->request_redraw = true;
            wakeup_core(vo);
        }
        reset_vsync_timings(vo);
        wakeup_locked(vo);
    }
    mp_mutex_unlock(&in->lock);
}

int64_t vo_get_drop_count(struct vo *vo)
{
    mp_mutex_lock(&vo->in->lock);
    int64_t r = vo->in->drop_count;
    mp_mutex_unlock(&vo->in->lock);
    return r;
}

void vo_increment_drop_count(struct vo *vo, int64_t n)
{
    mp_mutex_lock(&vo->in->lock);
    vo->in->drop_count += n;
    mp_mutex_unlock(&vo->in->lock);
}

// Make the VO redraw the OSD at some point in the future.
void vo_redraw(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    if (!in->request_redraw) {
        in->request_redraw = true;
        in->want_redraw = false;
        wakeup_locked(vo);
    }
    mp_mutex_unlock(&in->lock);
}

bool vo_want_redraw(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    bool r = in->want_redraw;
    mp_mutex_unlock(&in->lock);
    return r;
}

void vo_seek_reset(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    forget_frames(vo);
    reset_vsync_timings(vo);
    in->send_reset = true;
    wakeup_locked(vo);
    mp_mutex_unlock(&in->lock);
}

// Whether at least 1 frame was queued or rendered since last seek or reconfig.
bool vo_has_frame(struct vo *vo)
{
    return vo->in->hasframe;
}

static void run_query_format(void *p)
{
    void **pp = p;
    struct vo *vo = pp[0];
    uint8_t *list = pp[1];
    for (int format = IMGFMT_START; format < IMGFMT_END; format++)
        list[format - IMGFMT_START] = vo->driver->query_format(vo, format);
}

// For each item in the list (allocated as uint8_t[IMGFMT_END - IMGFMT_START]),
// set the supported format flags.
void vo_query_formats(struct vo *vo, uint8_t *list)
{
    void *p[] = {vo, list};
    mp_dispatch_run(vo->in->dispatch, run_query_format, p);
}

// Calculate the appropriate source and destination rectangle to
// get a correctly scaled picture, including pan-scan.
// out_src: visible part of the video
// out_dst: area of screen covered by the video source rectangle
// out_osd: OSD size, OSD margins, etc.
// Must be called from the VO thread only.
void vo_get_src_dst_rects(struct vo *vo, struct mp_rect *out_src,
                          struct mp_rect *out_dst, struct mp_osd_res *out_osd)
{
    if (!vo->params) {
        *out_src = *out_dst = (struct mp_rect){0};
        *out_osd = (struct mp_osd_res){0};
        return;
    }
    mp_get_src_dst_rects(vo->log, vo->opts, vo->driver->caps, vo->params,
                         vo->dwidth, vo->dheight, vo->monitor_par,
                         out_src, out_dst, out_osd);
}

// flip_page[_timed] will be called offset_us nanoseconds too early.
// (For vo_vdpau, which does its own timing.)
// num_req_frames set the requested number of requested vo_frame.frames.
// (For vo_gpu interpolation.)
void vo_set_queue_params(struct vo *vo, int64_t offset_ns, int num_req_frames)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    in->flip_queue_offset = offset_ns;
    in->req_frames = MPCLAMP(num_req_frames, 1, VO_MAX_REQ_FRAMES);
    mp_mutex_unlock(&in->lock);
}

int vo_get_num_req_frames(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    int res = in->req_frames;
    mp_mutex_unlock(&in->lock);
    return res;
}

double vo_get_vsync_interval(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    double res = vo->in->vsync_interval > 1 ? vo->in->vsync_interval : -1;
    mp_mutex_unlock(&in->lock);
    return res;
}

double vo_get_estimated_vsync_interval(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    double res = in->estimated_vsync_interval;
    mp_mutex_unlock(&in->lock);
    return res;
}

double vo_get_estimated_vsync_jitter(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    double res = in->estimated_vsync_jitter;
    mp_mutex_unlock(&in->lock);
    return res;
}

// Get the time in seconds at after which the currently rendering frame will
// end. Returns positive values if the frame is yet to be finished, negative
// values if it already finished.
// This can only be called while no new frame is queued (after
// vo_is_ready_for_frame). Returns 0 for non-display synced frames, or if the
// deadline for continuous display was missed.
double vo_get_delay(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    int64_t res = get_display_synced_frame_end(vo);
    mp_mutex_unlock(&in->lock);
    return res ? MP_TIME_NS_TO_S(res - mp_time_ns()) : 0;
}

void vo_discard_timing_info(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    reset_vsync_timings(vo);
    mp_mutex_unlock(&in->lock);
}

int64_t vo_get_delayed_count(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    int64_t res = vo->in->delayed_count;
    mp_mutex_unlock(&in->lock);
    return res;
}

double vo_get_display_fps(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    double res = vo->in->display_fps;
    mp_mutex_unlock(&in->lock);
    return res;
}

// Set specific event flags, and wakeup the playback core if needed.
// vo_query_and_reset_events() can retrieve the events again.
void vo_event(struct vo *vo, int event)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    if ((in->queued_events & event & VO_EVENTS_USER) != (event & VO_EVENTS_USER))
        wakeup_core(vo);
    if (event)
        wakeup_locked(vo);
    in->queued_events |= event;
    in->internal_events |= event;
    mp_mutex_unlock(&in->lock);
}

// Check event flags set with vo_event(). Return the mask of events that was
// set and included in the events parameter. Clear the returned events.
int vo_query_and_reset_events(struct vo *vo, int events)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    int r = in->queued_events & events;
    in->queued_events &= ~(unsigned)r;
    mp_mutex_unlock(&in->lock);
    return r;
}

struct mp_image *vo_get_current_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    struct mp_image *r = NULL;
    if (vo->in->current_frame)
        r = mp_image_new_ref(vo->in->current_frame->current);
    mp_mutex_unlock(&in->lock);
    return r;
}

struct vo_frame *vo_get_current_vo_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_mutex_lock(&in->lock);
    struct vo_frame *r = vo_frame_ref(vo->in->current_frame);
    mp_mutex_unlock(&in->lock);
    return r;
}

struct mp_image *vo_get_image(struct vo *vo, int imgfmt, int w, int h,
                              int stride_align, int flags)
{
    if (vo->driver->get_image_ts)
        return vo->driver->get_image_ts(vo, imgfmt, w, h, stride_align, flags);
    if (vo->in->dr_helper)
        return dr_helper_get_image(vo->in->dr_helper, imgfmt, w, h, stride_align, flags);
    return NULL;
}

static void destroy_frame(void *p)
{
    struct vo_frame *frame = p;
    for (int n = 0; n < frame->num_frames; n++)
        talloc_free(frame->frames[n]);
}

// Return a new reference to the given frame. The image pointers are also new
// references. Calling talloc_free() on the frame unrefs all currently set
// image references. (Assuming current==frames[0].)
struct vo_frame *vo_frame_ref(struct vo_frame *frame)
{
    if (!frame)
        return NULL;

    struct vo_frame *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, destroy_frame);
    *new = *frame;
    for (int n = 0; n < frame->num_frames; n++)
        new->frames[n] = mp_image_new_ref(frame->frames[n]);
    new->current = new->num_frames ? new->frames[0] : NULL;
    return new;
}

/*
 * lookup an integer in a table, table must have 0 as the last key
 * param: key key to search for
 * returns translation corresponding to key or "to" value of last mapping
 *         if not found.
 */
int lookup_keymap_table(const struct mp_keymap *map, int key)
{
    while (map->from && map->from != key)
        map++;
    return map->to;
}

struct mp_image_params vo_get_current_params(struct vo *vo)
{
    struct mp_image_params p = {0};
    mp_mutex_lock(&vo->params_mutex);
    if (vo->params)
        p = *vo->params;
    mp_mutex_unlock(&vo->params_mutex);
    return p;
}

struct mp_image_params vo_get_target_params(struct vo *vo)
{
    struct mp_image_params p = {0};
    mp_mutex_lock(&vo->params_mutex);
    if (vo->target_params)
        p = *vo->target_params;
    mp_mutex_unlock(&vo->params_mutex);
    return p;
}
