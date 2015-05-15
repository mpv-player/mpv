/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef __MINGW32__
#include <unistd.h>
#include <poll.h>
#endif

#include "talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "misc/dispatch.h"
#include "misc/rendezvous.h"
#include "options/options.h"
#include "misc/bstr.h"
#include "vo.h"
#include "aspect.h"
#include "input/input.h"
#include "options/m_config.h"
#include "common/msg.h"
#include "common/global.h"
#include "video/mp_image.h"
#include "sub/osd.h"
#include "osdep/io.h"
#include "osdep/threads.h"

extern const struct vo_driver video_out_x11;
extern const struct vo_driver video_out_vdpau;
extern const struct vo_driver video_out_xv;
extern const struct vo_driver video_out_opengl;
extern const struct vo_driver video_out_opengl_hq;
extern const struct vo_driver video_out_opengl_cb;
extern const struct vo_driver video_out_null;
extern const struct vo_driver video_out_image;
extern const struct vo_driver video_out_lavc;
extern const struct vo_driver video_out_caca;
extern const struct vo_driver video_out_drm;
extern const struct vo_driver video_out_direct3d;
extern const struct vo_driver video_out_direct3d_shaders;
extern const struct vo_driver video_out_sdl;
extern const struct vo_driver video_out_vaapi;
extern const struct vo_driver video_out_wayland;
extern const struct vo_driver video_out_rpi;

const struct vo_driver *const video_out_drivers[] =
{
#if HAVE_RPI
    &video_out_rpi,
#endif
#if HAVE_GL
    &video_out_opengl,
#endif
#if HAVE_VDPAU
    &video_out_vdpau,
#endif
#if HAVE_DIRECT3D
    &video_out_direct3d_shaders,
    &video_out_direct3d,
#endif
#if HAVE_XV
    &video_out_xv,
#endif
#if HAVE_SDL2
    &video_out_sdl,
#endif
#if HAVE_VAAPI
    &video_out_vaapi,
#endif
#if HAVE_X11
    &video_out_x11,
#endif
    &video_out_null,
    // should not be auto-selected
    &video_out_image,
#if HAVE_CACA
    &video_out_caca,
#endif
#if HAVE_DRM
    &video_out_drm,
#endif
#if HAVE_ENCODING
    &video_out_lavc,
#endif
#if HAVE_GL
    &video_out_opengl_hq,
    &video_out_opengl_cb,
#endif
#if HAVE_WAYLAND
    &video_out_wayland,
#endif
    NULL
};

struct vo_internal {
    pthread_t thread;
    struct mp_dispatch_queue *dispatch;

    // --- The following fields are protected by lock
    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    bool need_wakeup;
    bool terminate;

    int wakeup_pipe[2]; // used for VOs that use a unix FD for waiting


    bool hasframe;
    bool hasframe_rendered;
    bool request_redraw;            // redraw request from player to VO
    bool want_redraw;               // redraw request from VO to player
    bool send_reset;                // send VOCTRL_RESET
    bool paused;
    bool vsync_timed;               // the VO redraws itself as fast as possible
                                    // at every vsync
    int queued_events;              // event mask for the user
    int internal_events;            // event mask for us

    int64_t flip_queue_offset; // queue flip events at most this much in advance

    int64_t drop_count;
    bool dropped_frame;             // the previous frame was dropped

    struct mp_image *current_frame; // last frame queued to the VO

    int64_t wakeup_pts;             // time at which to pull frame from decoder

    bool rendering;                 // true if an image is being rendered
    struct mp_image *frame_queued;  // the image that should be rendered
    int64_t frame_pts;              // realtime of intended display
    int64_t frame_duration;         // realtime frame duration (for framedrop)

    double display_fps;

    // --- The following fields can be accessed from the VO thread only
    int64_t vsync_interval;
    int64_t vsync_interval_approx;
    int64_t last_flip;
    char *window_title;
};

static void forget_frames(struct vo *vo);
static void *vo_thread(void *ptr);

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(video_out_drivers) - 1)
        return false;
    const struct vo_driver *vo = video_out_drivers[index];
    *dst = (struct m_obj_desc) {
        .name = vo->name,
        .description = vo->description,
        .priv_size = vo->priv_size,
        .priv_defaults = vo->priv_defaults,
        .options = vo->options,
        .hidden = vo->encode || !strcmp(vo->name, "opengl-cb"),
        .p = vo,
    };
    return true;
}

// For the vo option
const struct m_obj_list vo_obj_list = {
    .get_desc = get_desc,
    .description = "video outputs",
    .aliases = {
        {"gl",        "opengl"},
        {"gl3",       "opengl-hq"},
        {0}
    },
    .allow_unknown_entries = true,
    .allow_trailer = true,
};

static void dispatch_wakeup_cb(void *ptr)
{
    struct vo *vo = ptr;
    vo_wakeup(vo);
}

// Does not include thread- and VO uninit.
static void dealloc_vo(struct vo *vo)
{
    forget_frames(vo); // implicitly synchronized
    pthread_mutex_destroy(&vo->in->lock);
    pthread_cond_destroy(&vo->in->wakeup);
    for (int n = 0; n < 2; n++)
        close(vo->in->wakeup_pipe[n]);
    talloc_free(vo);
}

static struct vo *vo_create(bool probing, struct mpv_global *global,
                            struct vo_extra *ex, char *name, char **args)
{
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
        .opts = &global->opts->vo,
        .global = global,
        .encode_lavc_ctx = ex->encode_lavc_ctx,
        .input_ctx = ex->input_ctx,
        .osd = ex->osd,
        .event_fd = -1,
        .monitor_par = 1,
        .extra = *ex,
        .probing = probing,
        .in = talloc(vo, struct vo_internal),
    };
    talloc_steal(vo, log);
    *vo->in = (struct vo_internal) {
        .dispatch = mp_dispatch_create(vo),
    };
    mp_make_wakeup_pipe(vo->in->wakeup_pipe);
    mp_dispatch_set_wakeup_fn(vo->in->dispatch, dispatch_wakeup_cb, vo);
    pthread_mutex_init(&vo->in->lock, NULL);
    pthread_cond_init(&vo->in->wakeup, NULL);

    mp_input_set_mouse_transform(vo->input_ctx, NULL, NULL);
    if (vo->driver->encode != !!vo->encode_lavc_ctx)
        goto error;
    struct m_config *config = m_config_from_obj_desc(vo, vo->log, &desc);
    if (m_config_apply_defaults(config, name, vo->opts->vo_defs) < 0)
        goto error;
    if (m_config_set_obj_params(config, args) < 0)
        goto error;
    vo->priv = config->optstruct;

    if (pthread_create(&vo->in->thread, NULL, vo_thread, vo))
        goto error;
    if (mp_rendezvous(vo, 0) < 0) { // init barrier
        pthread_join(vo->in->thread, NULL);
        goto error;
    }
    return vo;

error:
    dealloc_vo(vo);
    return NULL;
}

struct vo *init_best_video_out(struct mpv_global *global, struct vo_extra *ex)
{
    struct m_obj_settings *vo_list = global->opts->vo.video_driver_list;
    // first try the preferred drivers, with their optional subdevice param:
    if (vo_list && vo_list[0].name) {
        for (int n = 0; vo_list[n].name; n++) {
            // Something like "-vo name," allows fallback to autoprobing.
            if (strlen(vo_list[n].name) == 0)
                goto autoprobe;
            bool p = !!vo_list[n + 1].name;
            struct vo *vo = vo_create(p, global, ex, vo_list[n].name,
                                      vo_list[n].attribs);
            if (vo)
                return vo;
        }
        return NULL;
    }
autoprobe:
    // now try the rest...
    for (int i = 0; video_out_drivers[i]; i++) {
        const struct vo_driver *driver = video_out_drivers[i];
        if (driver == &video_out_null)
            break;
        struct vo *vo = vo_create(true, global, ex, (char *)driver->name, NULL);
        if (vo)
            return vo;
    }
    return NULL;
}

void vo_destroy(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    mp_dispatch_lock(in->dispatch);
    vo->in->terminate = true;
    mp_dispatch_unlock(in->dispatch);
    pthread_join(vo->in->thread, NULL);
    dealloc_vo(vo);
}

// to be called from VO thread only
static void update_display_fps(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    if (in->internal_events & VO_EVENT_WIN_STATE) {
        in->internal_events &= ~(unsigned)VO_EVENT_WIN_STATE;

        pthread_mutex_unlock(&in->lock);

        double display_fps = 0;
        if (vo->global->opts->frame_drop_fps > 0) {
            display_fps = vo->global->opts->frame_drop_fps;
        } else {
            vo->driver->control(vo, VOCTRL_GET_DISPLAY_FPS, &display_fps);
        }

        pthread_mutex_lock(&in->lock);

        if (in->display_fps != display_fps) {
            in->display_fps = display_fps;
            MP_VERBOSE(vo, "Assuming %f FPS for framedrop.\n", display_fps);

            // make sure to update the player
            in->queued_events |= VO_EVENT_WIN_STATE;
            mp_input_wakeup(vo->input_ctx);
        }
    }
    pthread_mutex_unlock(&in->lock);
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
    struct mp_image_params *params = pp[1];
    int flags = *(int *)pp[2];
    int *ret = pp[3];

    struct vo_internal *in = vo->in;

    vo->dwidth = params->d_w;
    vo->dheight = params->d_h;

    talloc_free(vo->params);
    vo->params = talloc_memdup(vo, params, sizeof(*params));

    *ret = vo->driver->reconfig(vo, vo->params, flags);
    vo->config_ok = *ret >= 0;
    if (vo->config_ok) {
        check_vo_caps(vo);
    } else {
        talloc_free(vo->params);
        vo->params = NULL;
    }

    pthread_mutex_lock(&in->lock);
    mp_image_unrefp(&in->current_frame);
    forget_frames(vo);
    pthread_mutex_unlock(&in->lock);

    update_display_fps(vo);
}

int vo_reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    int ret;
    void *p[] = {vo, params, &flags, &ret};
    mp_dispatch_run(vo->in->dispatch, run_reconfig, p);
    return ret;
}

static void run_control(void *p)
{
    void **pp = p;
    struct vo *vo = pp[0];
    uint32_t request = *(int *)pp[1];
    void *data = pp[2];
    if (request == VOCTRL_UPDATE_WINDOW_TITLE) // legacy fallback
        vo->in->window_title = talloc_strdup(vo, data);
    int ret = vo->driver->control(vo, request, data);
    *(int *)pp[3] = ret;
}

int vo_control(struct vo *vo, uint32_t request, void *data)
{
    int ret;
    void *p[] = {vo, &request, data, &ret};
    mp_dispatch_run(vo->in->dispatch, run_control, p);
    return ret;
}

// must be called locked
static void forget_frames(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    in->hasframe = false;
    in->hasframe_rendered = false;
    in->drop_count = 0;
    mp_image_unrefp(&in->frame_queued);
    // don't unref current_frame; we always want to be able to redraw it
}

#ifndef __MINGW32__
static void wait_event_fd(struct vo *vo, int64_t until_time)
{
    struct vo_internal *in = vo->in;

    struct pollfd fds[2] = {
        { .fd = vo->event_fd, .events = POLLIN },
        { .fd = in->wakeup_pipe[0], .events = POLLIN },
    };
    int64_t wait_us = until_time - mp_time_us();
    int timeout_ms = MPCLAMP((wait_us + 500) / 1000, 0, 10000);

    poll(fds, 2, timeout_ms);

    if (fds[1].revents & POLLIN) {
        char buf[100];
        read(in->wakeup_pipe[0], buf, sizeof(buf)); // flush
    }
}
static void wakeup_event_fd(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    write(in->wakeup_pipe[1], &(char){0}, 1);
}
#else
static void wait_event_fd(struct vo *vo, int64_t until_time){}
static void wakeup_event_fd(struct vo *vo){}
#endif

// Called unlocked.
static void wait_vo(struct vo *vo, int64_t until_time)
{
    struct vo_internal *in = vo->in;

    if (vo->event_fd >= 0) {
        wait_event_fd(vo, until_time);
        pthread_mutex_lock(&in->lock);
        in->need_wakeup = false;
        pthread_mutex_unlock(&in->lock);
    } else if (vo->driver->wait_events) {
        vo->driver->wait_events(vo, until_time);
        pthread_mutex_lock(&in->lock);
        in->need_wakeup = false;
        pthread_mutex_unlock(&in->lock);
    } else {
        pthread_mutex_lock(&in->lock);
        if (!in->need_wakeup) {
            struct timespec ts = mp_time_us_to_timespec(until_time);
            pthread_cond_timedwait(&in->wakeup, &in->lock, &ts);
        }
        in->need_wakeup = false;
        pthread_mutex_unlock(&in->lock);
    }
}

static void wakeup_locked(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    pthread_cond_signal(&in->wakeup);
    if (vo->event_fd >= 0)
        wakeup_event_fd(vo);
    if (vo->driver->wakeup)
        vo->driver->wakeup(vo);
    in->need_wakeup = true;
}

// Wakeup VO thread, and make it check for new events with VOCTRL_CHECK_EVENTS.
// To be used by threaded VO backends.
void vo_wakeup(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    pthread_mutex_lock(&in->lock);
    wakeup_locked(vo);
    pthread_mutex_unlock(&in->lock);
}

// Whether vo_queue_frame() can be called. If the VO is not ready yet, the
// function will return false, and the VO will call the wakeup callback once
// it's ready.
// next_pts is the exact time when the next frame should be displayed. If the
// VO is ready, but the time is too "early", return false, and call the wakeup
// callback once the time is right.
bool vo_is_ready_for_frame(struct vo *vo, int64_t next_pts)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    bool r = vo->config_ok && !in->frame_queued;
    if (r) {
        // Don't show the frame too early - it would basically freeze the
        // display by disallowing OSD redrawing or VO interaction.
        // Actually render the frame at earliest 50ms before target time.
        next_pts -= (uint64_t)(0.050 * 1e6);
        next_pts -= in->flip_queue_offset;
        int64_t now = mp_time_us();
        if (next_pts > now)
            r = false;
        if (!in->wakeup_pts || next_pts < in->wakeup_pts) {
            in->wakeup_pts = in->vsync_timed ? 0 : next_pts;
            wakeup_locked(vo);
        }
    }
    pthread_mutex_unlock(&in->lock);
    return r;
}

// Direct the VO thread to put the currently queued image on the screen.
// vo_is_ready_for_frame() must have returned true before this call.
// Ownership of the image is handed to the vo.
void vo_queue_frame(struct vo *vo, struct mp_image *image,
                    int64_t pts_us, int64_t duration)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    assert(vo->config_ok && !in->frame_queued);
    in->hasframe = true;
    in->frame_queued = image;
    in->frame_pts = pts_us;
    in->frame_duration = duration;
    in->wakeup_pts = in->vsync_timed ? 0 : in->frame_pts + MPMAX(duration, 0);
    wakeup_locked(vo);
    pthread_mutex_unlock(&in->lock);
}

// If a frame is currently being rendered (or queued), wait until it's done.
// Otherwise, return immediately.
void vo_wait_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    while (in->frame_queued || in->rendering)
        pthread_cond_wait(&in->wakeup, &in->lock);
    pthread_mutex_unlock(&in->lock);
}

// needs lock
static int64_t prev_sync(struct vo *vo, int64_t ts)
{
    struct vo_internal *in = vo->in;

    int64_t diff = (int64_t)(ts - in->last_flip);
    int64_t offset = diff % in->vsync_interval;
    if (offset < 0)
        offset += in->vsync_interval;
    return ts - offset;
}

static bool render_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    update_display_fps(vo);

    pthread_mutex_lock(&in->lock);

    vo->in->vsync_interval = in->display_fps > 0 ? 1e6 / in->display_fps : 0;
    vo->in->vsync_interval = MPMAX(vo->in->vsync_interval, 1);

    int64_t pts = in->frame_pts;
    int64_t duration = in->frame_duration;
    struct mp_image *img = in->frame_queued;

    if (!img && (!in->vsync_timed || in->paused))
        goto nothing_done;

    if (in->vsync_timed && !in->hasframe)
        goto nothing_done;

    if (img)
        mp_image_setrefp(&in->current_frame, img);

    in->frame_queued = NULL;

    // The next time a flip (probably) happens.
    int64_t prev_vsync = prev_sync(vo, mp_time_us());
    int64_t next_vsync = prev_vsync + in->vsync_interval;
    int64_t end_time = pts + duration;

    // Time at which we should flip_page on the VO.
    int64_t target = pts - in->flip_queue_offset;

    if (!in->hasframe_rendered)
        duration = -1; // disable framedrop

    // If the clip and display have similar/identical fps, it's possible that
    // we'll be very slightly late frequently due to timing jitter, or if the
    // clip/container timestamps are not very accurate.
    // So if we dropped the previous frame, keep dropping until we're aligned
    // perfectly, else, allow some slack (1 vsync) to let it settle into a rhythm.
    // On low clip fps, we don't drop anyway and the slack logic doesn't matter.
    // If the clip fps is more than ~5% above screen fps, we remove this slack
    // and use "normal" logic to allow more regular drops of 1 frame at a time.
    bool use_slack = duration > (0.95 * in->vsync_interval);
    in->dropped_frame = duration >= 0 &&
                        use_slack ?
                            ((in->dropped_frame && end_time < next_vsync) ||
                            (end_time < prev_vsync)) // hard threshold - 1 vsync late
                            :
                            end_time < next_vsync; // normal frequent drops


    in->dropped_frame &= !(vo->driver->caps & VO_CAP_FRAMEDROP);
    in->dropped_frame &= (vo->global->opts->frame_dropping & 1);
    // Even if we're hopelessly behind, rather degrade to 10 FPS playback,
    // instead of just freezing the display forever.
    in->dropped_frame &= mp_time_us() - in->last_flip < 100 * 1000;

    if (in->vsync_timed) {
        // this is a heuristic that wakes the thread up some
        // time before the next vsync
        target = next_vsync - MPMIN(in->vsync_interval / 2, 8e3);

        // We are very late with the frame and using vsync timing: probably
        // no new frames are coming in. This must be done whether or not
        // framedrop is enabled. Also, if the frame is to be dropped, even
        // though it's an interpolated frame (img==NULL), exit early.
        if (!img && ((in->hasframe_rendered &&
                      prev_vsync > pts + duration + in->vsync_interval_approx)
                     || in->dropped_frame))
        {
            in->dropped_frame = false;
            goto nothing_done;
        }
    }

    if (in->dropped_frame) {
        talloc_free(img);
    } else {
        in->rendering = true;
        in->hasframe_rendered = true;
        pthread_mutex_unlock(&in->lock);
        mp_input_wakeup(vo->input_ctx); // core can queue new video now

        MP_STATS(vo, "start video");

        if (vo->driver->draw_image_timed) {
            struct frame_timing t = (struct frame_timing) {
                .pts        = pts,
                .next_vsync = next_vsync,
                .prev_vsync = prev_vsync,
            };
            vo->driver->draw_image_timed(vo, img, &t);
        } else {
            vo->driver->draw_image(vo, img);
        }

        while (1) {
            int64_t now = mp_time_us();
            if (target <= now)
                break;
            mp_sleep_us(target - now);
        }

        bool drop = false;
        if (vo->driver->flip_page_timed)
            drop = vo->driver->flip_page_timed(vo, pts, duration) < 1;
        else
            vo->driver->flip_page(vo);

        int64_t prev_flip = in->last_flip;

        in->last_flip = -1;

        vo->driver->control(vo, VOCTRL_GET_RECENT_FLIP_TIME, &in->last_flip);

        if (in->last_flip < 0)
            in->last_flip = mp_time_us();

        in->vsync_interval_approx = in->last_flip - prev_flip;

        MP_STATS(vo, "end video");

        pthread_mutex_lock(&in->lock);
        in->dropped_frame = drop;
        in->rendering = false;
    }

    if (in->dropped_frame) {
        in->drop_count += 1;
    } else {
        vo->want_redraw = false;
        in->want_redraw = false;
        in->request_redraw = false;
    }

    pthread_cond_signal(&in->wakeup); // for vo_wait_frame()
    mp_input_wakeup(vo->input_ctx);

    pthread_mutex_unlock(&in->lock);
    return true;

nothing_done:
    pthread_mutex_unlock(&in->lock);
    return false;
}

static void do_redraw(struct vo *vo)
{
    struct vo_internal *in = vo->in;

    vo->want_redraw = false;

    pthread_mutex_lock(&in->lock);
    in->request_redraw = false;
    in->want_redraw = false;
    bool full_redraw = in->dropped_frame;
    struct mp_image *img = NULL;
    if (vo->config_ok && !(vo->driver->untimed))
        img = mp_image_new_ref(in->current_frame);
    if (img)
        in->dropped_frame = false;
    pthread_mutex_unlock(&in->lock);

    if (full_redraw || vo->driver->control(vo, VOCTRL_REDRAW_FRAME, NULL) < 1) {
        if (img)
            vo->driver->draw_image(vo, img);
    } else {
        talloc_free(img);
    }

    if (vo->driver->flip_page_timed)
        vo->driver->flip_page_timed(vo, 0, -1);
    else
        vo->driver->flip_page(vo);
}

static void *vo_thread(void *ptr)
{
    struct vo *vo = ptr;
    struct vo_internal *in = vo->in;

    mpthread_set_name("vo");

    int r = vo->driver->preinit(vo) ? -1 : 0;
    mp_rendezvous(vo, r); // init barrier
    if (r < 0)
        return NULL;

    update_display_fps(vo);
    vo_event(vo, VO_EVENT_WIN_STATE);

    while (1) {
        mp_dispatch_queue_process(vo->in->dispatch, 0);
        if (in->terminate)
            break;
        vo->driver->control(vo, VOCTRL_CHECK_EVENTS, NULL);
        bool frame_shown = render_frame(vo);
        int64_t now = mp_time_us();
        int64_t wait_until = now + (frame_shown ? 0 : (int64_t)1e9);
        pthread_mutex_lock(&in->lock);
        if (in->wakeup_pts) {
            if (in->wakeup_pts > now) {
                wait_until = MPMIN(wait_until, in->wakeup_pts);
            } else {
                in->wakeup_pts = 0;
                mp_input_wakeup(vo->input_ctx);
            }
        }
        if (vo->want_redraw && !in->want_redraw) {
            in->want_redraw = true;
            mp_input_wakeup(vo->input_ctx);
        }
        bool redraw = in->request_redraw;
        bool send_reset = in->send_reset;
        in->send_reset = false;
        pthread_mutex_unlock(&in->lock);
        if (send_reset)
            vo->driver->control(vo, VOCTRL_RESET, NULL);
        if (wait_until > now && redraw) {
            do_redraw(vo); // now is a good time
            continue;
        }
        wait_vo(vo, wait_until);
    }
    forget_frames(vo); // implicitly synchronized
    mp_image_unrefp(&in->current_frame);
    vo->driver->uninit(vo);
    return NULL;
}

void vo_set_paused(struct vo *vo, bool paused)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    if (in->paused != paused) {
        in->paused = paused;
        if (in->paused && in->dropped_frame)
            in->request_redraw = true;
    }
    pthread_mutex_unlock(&in->lock);
    vo_control(vo, paused ? VOCTRL_PAUSE : VOCTRL_RESUME, NULL);
}

int64_t vo_get_drop_count(struct vo *vo)
{
    pthread_mutex_lock(&vo->in->lock);
    int64_t r = vo->in->drop_count;
    pthread_mutex_unlock(&vo->in->lock);
    return r;
}

void vo_increment_drop_count(struct vo *vo, int64_t n)
{
    pthread_mutex_lock(&vo->in->lock);
    vo->in->drop_count += n;
    pthread_mutex_unlock(&vo->in->lock);
}

// Make the VO redraw the OSD at some point in the future.
void vo_redraw(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    if (!in->request_redraw) {
        in->request_redraw = true;
        wakeup_locked(vo);
    }
    pthread_mutex_unlock(&in->lock);
}

bool vo_want_redraw(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    bool r = in->want_redraw;
    pthread_mutex_unlock(&in->lock);
    return r;
}

void vo_seek_reset(struct vo *vo)
{
    pthread_mutex_lock(&vo->in->lock);
    forget_frames(vo);
    vo->in->send_reset = true;
    wakeup_locked(vo);
    pthread_mutex_unlock(&vo->in->lock);
}

// Return true if there is still a frame being displayed (or queued).
// If this returns true, a wakeup some time in the future is guaranteed.
bool vo_still_displaying(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&vo->in->lock);
    int64_t now = mp_time_us();
    int64_t frame_end = in->frame_pts + MPMAX(in->frame_duration, 0);
    bool working = now < frame_end || in->rendering || in->frame_queued;
    pthread_mutex_unlock(&vo->in->lock);
    return working && in->hasframe;
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

// Return the window title the VO should set. Always returns a null terminated
// string. The string is valid until frontend code is invoked again. Copy it if
// you need to keep the string for an extended period of time.
// Must be called from the VO thread only.
// Don't use for new code.
const char *vo_get_window_title(struct vo *vo)
{
    if (!vo->in->window_title)
        vo->in->window_title = talloc_strdup(vo, "");
    return vo->in->window_title;
}

// flip_page[_timed] will be called offset_us microseconds too early.
// (For vo_vdpau, which does its own timing.)
// Setting vsync_timed to true redraws as fast as possible.
// (For vo_opengl smoothmotion.)
void vo_set_flip_queue_params(struct vo *vo, int64_t offset_us, bool vsync_timed)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    in->flip_queue_offset = offset_us;
    in->vsync_timed = vsync_timed;
    pthread_mutex_unlock(&in->lock);
}

// to be called from the VO thread only
int64_t vo_get_vsync_interval(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    int64_t res = vo->in->vsync_interval;
    pthread_mutex_unlock(&in->lock);
    return res;
}

double vo_get_display_fps(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    double res = vo->in->display_fps;
    pthread_mutex_unlock(&in->lock);
    return res;
}

// Set specific event flags, and wakeup the playback core if needed.
// vo_query_and_reset_events() can retrieve the events again.
void vo_event(struct vo *vo, int event)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    if ((in->queued_events & event & VO_EVENTS_USER) != (event & VO_EVENTS_USER))
        mp_input_wakeup(vo->input_ctx);
    if (event)
        wakeup_locked(vo);
    in->queued_events |= event;
    in->internal_events |= event;
    pthread_mutex_unlock(&in->lock);
}

// Check event flags set with vo_event(). Return the mask of events that was
// set and included in the events parameter. Clear the returned events.
int vo_query_and_reset_events(struct vo *vo, int events)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    int r = in->queued_events & events;
    in->queued_events &= ~(unsigned)r;
    pthread_mutex_unlock(&in->lock);
    return r;
}

struct mp_image *vo_get_current_frame(struct vo *vo)
{
    struct vo_internal *in = vo->in;
    pthread_mutex_lock(&in->lock);
    struct mp_image *r = mp_image_new_ref(vo->in->current_frame);
    pthread_mutex_unlock(&in->lock);
    return r;
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
