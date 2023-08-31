/*
 * Copyright (C) Aaron Holtzman - Aug 1999
 *
 * Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 * (C) MPlayer developers
 *
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

#ifndef MPLAYER_VIDEO_OUT_H
#define MPLAYER_VIDEO_OUT_H

#include <inttypes.h>
#include <stdbool.h>

#include "video/img_format.h"
#include "common/common.h"
#include "options/options.h"
#include "osdep/threads.h"

enum {
    // VO needs to redraw
    VO_EVENT_EXPOSE                     = 1 << 0,
    // VO needs to update state to a new window size
    VO_EVENT_RESIZE                     = 1 << 1,
    // The ICC profile needs to be reloaded
    VO_EVENT_ICC_PROFILE_CHANGED        = 1 << 2,
    // Some other window state changed (position, window state, fps)
    VO_EVENT_WIN_STATE                  = 1 << 3,
    // The ambient light conditions changed and need to be reloaded
    VO_EVENT_AMBIENT_LIGHTING_CHANGED   = 1 << 4,
    // Special mechanism for making resizing with Cocoa react faster
    VO_EVENT_LIVE_RESIZING              = 1 << 5,
    // For VOCTRL_GET_HIDPI_SCALE changes.
    VO_EVENT_DPI                        = 1 << 6,
    // Special thing for encode mode (vo_driver.initially_blocked).
    // Part of VO_EVENTS_USER to make vo_is_ready_for_frame() work properly.
    VO_EVENT_INITIAL_UNBLOCK            = 1 << 7,
    VO_EVENT_FOCUS                      = 1 << 8,

    // Set of events the player core may be interested in.
    VO_EVENTS_USER = VO_EVENT_RESIZE | VO_EVENT_WIN_STATE | VO_EVENT_DPI |
                     VO_EVENT_INITIAL_UNBLOCK | VO_EVENT_FOCUS,
};

enum mp_voctrl {
    /* signal a device reset seek */
    VOCTRL_RESET = 1,
    /* Handle input and redraw events, called by vo_check_events() */
    VOCTRL_CHECK_EVENTS,
    /* signal a device pause */
    VOCTRL_PAUSE,
    /* start/resume playback */
    VOCTRL_RESUME,

    VOCTRL_SET_PANSCAN,

    // Triggered by any change to mp_vo_opts. This is for convenience. In theory,
    // you could install your own listener.
    VOCTRL_VO_OPTS_CHANGED,

    /* private to vo_gpu */
    VOCTRL_LOAD_HWDEC_API,

    // Only used internally in vo_libmpv
    VOCTRL_PREINIT,
    VOCTRL_UNINIT,
    VOCTRL_RECONFIG,

    VOCTRL_UPDATE_WINDOW_TITLE,         // char*
    VOCTRL_UPDATE_PLAYBACK_STATE,       // struct voctrl_playback_state*

    VOCTRL_PERFORMANCE_DATA,            // struct voctrl_performance_data*

    VOCTRL_SET_CURSOR_VISIBILITY,       // bool*

    VOCTRL_CONTENT_TYPE,                // enum mp_content_type*

    VOCTRL_KILL_SCREENSAVER,
    VOCTRL_RESTORE_SCREENSAVER,

    // Return or set window size (not-fullscreen mode only - if fullscreened,
    // these must access the not-fullscreened window size only).
    VOCTRL_GET_UNFS_WINDOW_SIZE,        // int[2] (w/h)
    VOCTRL_SET_UNFS_WINDOW_SIZE,        // int[2] (w/h)

    VOCTRL_GET_FOCUSED,                 // bool*

    // char *** (NULL terminated array compatible with CONF_TYPE_STRING_LIST)
    // names for displays the window is on
    VOCTRL_GET_DISPLAY_NAMES,

    // Retrieve window contents. (Normal screenshots use vo_get_current_frame().)
    // Deprecated for VOCTRL_SCREENSHOT with corresponding flags.
    VOCTRL_SCREENSHOT_WIN,              // struct mp_image**

    // A normal screenshot - VOs can react to this if vo_get_current_frame() is
    // not sufficient.
    VOCTRL_SCREENSHOT,                  // struct voctrl_screenshot*

    VOCTRL_UPDATE_RENDER_OPTS,

    VOCTRL_GET_ICC_PROFILE,             // bstr*
    VOCTRL_GET_AMBIENT_LUX,             // int*
    VOCTRL_GET_DISPLAY_FPS,             // double*
    VOCTRL_GET_HIDPI_SCALE,             // double*
    VOCTRL_GET_DISPLAY_RES,             // int[2]
    VOCTRL_GET_WINDOW_ID,               // int64_t*

    /* private to vo_gpu and vo_gpu_next */
    VOCTRL_EXTERNAL_RESIZE,
};

// Helper to expose what kind of content is currently playing to the VO.
enum mp_content_type {
    MP_CONTENT_NONE, // used for force-window
    MP_CONTENT_IMAGE,
    MP_CONTENT_VIDEO,
};

#define VO_TRUE         true
#define VO_FALSE        false
#define VO_ERROR        -1
#define VO_NOTAVAIL     -2
#define VO_NOTIMPL      -3

// VOCTRL_UPDATE_PLAYBACK_STATE
struct voctrl_playback_state {
    bool taskbar_progress;
    bool playing;
    bool paused;
    int percent_pos;
};

// VOCTRL_PERFORMANCE_DATA
#define VO_PERF_SAMPLE_COUNT 256

struct mp_pass_perf {
    // times are all in nanoseconds
    uint64_t last, avg, peak;
    uint64_t samples[VO_PERF_SAMPLE_COUNT];
    uint64_t count;
};

#define VO_PASS_PERF_MAX 64
#define VO_PASS_DESC_MAX_LEN 128

struct mp_frame_perf {
    int count;
    struct mp_pass_perf perf[VO_PASS_PERF_MAX];
    char desc[VO_PASS_PERF_MAX][VO_PASS_DESC_MAX_LEN];
};

struct voctrl_performance_data {
    struct mp_frame_perf fresh, redraw;
};

struct voctrl_screenshot {
    bool scaled, subs, osd, high_bit_depth, native_csp;
    struct mp_image *res;
};

enum {
    // VO does handle mp_image_params.rotate in 90 degree steps
    VO_CAP_ROTATE90     = 1 << 0,
    // VO does framedrop itself (vo_vdpau). Untimed/encoding VOs never drop.
    VO_CAP_FRAMEDROP    = 1 << 1,
    // VO does not allow frames to be retained (vo_mediacodec_embed).
    VO_CAP_NORETAIN     = 1 << 2,
    // VO supports applying film grain
    VO_CAP_FILM_GRAIN   = 1 << 3,
};

enum {
    // Require DR buffers to be host-cached (i.e. fast readback)
    VO_DR_FLAG_HOST_CACHED = 1 << 0,
};

#define VO_MAX_REQ_FRAMES 10
#define VO_MAX_SWAPCHAIN_DEPTH 8

struct vo;
struct osd_state;
struct mp_image;
struct mp_image_params;

struct vo_extra {
    struct input_ctx *input_ctx;
    struct osd_state *osd;
    struct encode_lavc_context *encode_lavc_ctx;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;
};

struct vo_frame {
    // If > 0, realtime when frame should be shown, in mp_time_ns() units.
    // If 0, present immediately.
    int64_t pts;
    // Approximate frame duration, in ns.
    double duration;
    // Realtime of estimated distance between 2 vsync events.
    double vsync_interval;
    // "ideal" display time within the vsync
    double vsync_offset;
    // "ideal" frame duration (can be different from num_vsyncs*vsync_interval
    // up to a vsync) - valid for the entire frame, i.e. not changed for repeats
    double ideal_frame_duration;
    // "ideal" frame vsync point relative to the pts
    double ideal_frame_vsync;
    // "ideal" frame duration relative to the pts
    double ideal_frame_vsync_duration;
    // how often the frame will be repeated (does not include OSD redraws)
    int num_vsyncs;
    // Set if the current frame is repeated from the previous. It's guaranteed
    // that the current is the same as the previous one, even if the image
    // pointer is different.
    // The repeat flag is set if exactly the same frame should be rendered
    // again (and the OSD does not need to be redrawn).
    // A repeat frame can be redrawn, in which case repeat==redraw==true, and
    // OSD should be updated.
    bool redraw, repeat;
    // The frame is not in movement - e.g. redrawing while paused.
    bool still;
    // Frames are output as fast as possible, with implied vsync blocking.
    bool display_synced;
    // Dropping the frame is allowed if the VO is behind.
    bool can_drop;
    // The current frame to be drawn.
    // Warning: When OSD should be redrawn in --force-window --idle mode, this
    //          can be NULL. The VO should draw a black background, OSD on top.
    struct mp_image *current;
    // List of future images, starting with the current one. This does not
    // care about repeated frames - it simply contains the next real frames.
    // vo_set_queue_params() sets how many future frames this should include.
    // The actual number of frames delivered to the VO can be lower.
    // frames[0] is current, frames[1] is the next frame.
    // Note that some future frames may never be sent as current frame to the
    // VO if frames are dropped.
    int num_frames;
    struct mp_image *frames[VO_MAX_REQ_FRAMES];
    // Speed unadjusted, approximate frame duration inferred from past frames
    double approx_duration;
    // ID for frames[0] (== current). If current==NULL, the number is
    // meaningless. Otherwise, it's an unique ID for the frame. The ID for
    // a frame is guaranteed not to change (instant redraws will use the same
    // ID). frames[n] has the ID frame_id+n, with the guarantee that frame
    // drops or reconfigs will keep the guarantee.
    // The ID is never 0 (unless num_frames==0). IDs are strictly monotonous.
    uint64_t frame_id;
};

// Presentation feedback. See get_vsync() for how backends should fill this
// struct.
struct vo_vsync_info {
    // mp_time_ns() timestamp at which the last queued frame will likely be
    // displayed (this is in the future, unless the frame is instantly output).
    // 0 or lower if unset or unsupported.
    // This implies the latency of the output.
    int64_t last_queue_display_time;

    // Time between 2 vsync events in nanoseconds. The difference should be the
    // from 2 times sampled from the same reference point (it should not be the
    // difference between e.g. the end of scanout and the start of the next one;
    // it must be continuous).
    // -1 if unsupported.
    //  0 if supported, but no value available yet. It is assumed that the value
    //    becomes available after enough swap_buffers() calls were done.
    // >0 values are taken for granted. Very bad things will happen if it's
    //    inaccurate.
    int64_t vsync_duration;

    // Number of skipped physical vsyncs at some point in time. Typically, this
    // value is some time in the past by an offset that equals to the latency.
    // This value is reset and newly sampled at every swap_buffers() call.
    // This can be used to detect delayed frames iff you try to call
    // swap_buffers() for every physical vsync.
    // -1 if unset or unsupported.
    int64_t skipped_vsyncs;
};

struct vo_driver {
    // Encoding functionality, which can be invoked via --o only.
    bool encode;

    // This requires waiting for a VO_EVENT_INITIAL_UNBLOCK event before the
    // first frame can be sent. Doing vo_reconfig*() calls is allowed though.
    // Encode mode uses this, the core uses vo_is_ready_for_frame() to
    // implicitly check for this.
    bool initially_blocked;

    // VO_CAP_* bits
    int caps;

    // Disable video timing, push frames as quickly as possible, never redraw.
    bool untimed;

    // The VO is responsible for freeing frames.
    bool frame_owner;

    const char *name;
    const char *description;

    /*
     *   returns: zero on successful initialization, non-zero on error.
     */
    int (*preinit)(struct vo *vo);

    /*
     * Whether the given image format is supported and config() will succeed.
     * format: one of IMGFMT_*
     * returns: 0 on not supported, otherwise 1
     */
    int (*query_format)(struct vo *vo, int format);

    /*
     * Initialize or reconfigure the display driver.
     *   params: video parameters, like pixel format and frame size
     * returns: < 0 on error, >= 0 on success
     */
    int (*reconfig)(struct vo *vo, struct mp_image_params *params);

    /*
     * Like reconfig(), but provides the whole mp_image for which the change is
     * required. (The image doesn't have to have real data.)
     */
    int (*reconfig2)(struct vo *vo, struct mp_image *img);

    /*
     * Control interface
     */
    int (*control)(struct vo *vo, uint32_t request, void *data);

    /*
     * lavc callback for direct rendering
     *
     * Optional. To make implementation easier, the callback is always run on
     * the VO thread. The returned mp_image's destructor callback is also called
     * on the VO thread, even if it's actually unref'ed from another thread.
     *
     * It is guaranteed that the last reference to an image is destroyed before
     * ->uninit is called (except it's not - libmpv screenshots can hold the
     * reference longer, fuck).
     *
     * The allocated image - or a part of it, can be passed to draw_frame(). The
     * point of this mechanism is that the decoder directly renders to GPU
     * staging memory, to avoid a memcpy on frame upload. But this is not a
     * guarantee. A filter could change the data pointers or return a newly
     * allocated image. It's even possible that only 1 plane uses the buffer
     * allocated by the get_image function. The VO has to check for this.
     *
     * stride_align is always a value >=1. The stride values of the returned
     * image must be divisible by this value. This may be a non power of two.
     *
     * flags is a combination of VO_DR_FLAG_* flags.
     *
     * Currently, the returned image must have exactly 1 AVBufferRef set, for
     * internal implementation simplicity.
     *
     * returns: an allocated, refcounted image; if NULL is returned, the caller
     * will silently fallback to a default allocator
     */
    struct mp_image *(*get_image)(struct vo *vo, int imgfmt, int w, int h,
                                  int stride_align, int flags);

    /*
     * Thread-safe variant of get_image. Set at most one of these callbacks.
     * This excludes _all_ synchronization magic. The only guarantee is that
     * vo_driver.uninit is not called before this function returns.
     */
    struct mp_image *(*get_image_ts)(struct vo *vo, int imgfmt, int w, int h,
                                     int stride_align, int flags);

    /* Render the given frame. Note that this is also called when repeating
     * or redrawing frames.
     *
     * frame is freed by the caller if the callee did not assume ownership
     * of the frames, but in any case the callee can still modify the
     * contained data and references.
     */
    void (*draw_frame)(struct vo *vo, struct vo_frame *frame);

    /*
     * Blit/Flip buffer to the screen. Must be called after each frame!
     */
    void (*flip_page)(struct vo *vo);

    /*
     * Return presentation feedback. The implementation should not touch fields
     * it doesn't support; the info fields are preinitialized to neutral values.
     * Usually called once after flip_page(), but can be called any time.
     * The values returned by this are always relative to the last flip_page()
     * call.
     */
    void (*get_vsync)(struct vo *vo, struct vo_vsync_info *info);

    /* These optional callbacks can be provided if the GUI framework used by
     * the VO requires entering a message loop for receiving events and does
     * not call vo_wakeup() from a separate thread when there are new events.
     *
     * wait_events() will wait for new events, until the timeout expires, or the
     * function is interrupted. wakeup() is used to possibly interrupt the
     * event loop (wakeup() itself must be thread-safe, and not call any other
     * VO functions; it's the only vo_driver function with this requirement).
     * wakeup() should behave like a binary semaphore; if wait_events() is not
     * being called while wakeup() is, the next wait_events() call should exit
     * immediately.
     */
    void (*wakeup)(struct vo *vo);
    void (*wait_events)(struct vo *vo, int64_t until_time_ns);

    /*
     * Closes driver. Should restore the original state of the system.
     */
    void (*uninit)(struct vo *vo);

    // Size of private struct for automatic allocation (0 doesn't allocate)
    int priv_size;

    // If not NULL, it's copied into the newly allocated private struct.
    const void *priv_defaults;

    // List of options to parse into priv struct (requires priv_size to be set)
    // This will register them as global options (with options_prefix), and
    // copy the current value at VO creation time to the priv struct.
    const struct m_option *options;

    // All options in the above array are prefixed with this string. (It's just
    // for convenience and makes no difference in semantics.)
    const char *options_prefix;

    // Registers global options that go to a separate options struct.
    const struct m_sub_options *global_opts;
};

struct vo {
    const struct vo_driver *driver;
    struct mp_log *log; // Using e.g. "[vo/vdpau]" as prefix
    void *priv;
    struct mpv_global *global;
    struct vo_x11_state *x11;
    struct vo_w32_state *w32;
    struct vo_wayland_state *wl;
    struct vo_android_state *android;
    struct vo_drm_state *drm;
    struct mp_hwdec_devices *hwdec_devs;
    struct input_ctx *input_ctx;
    struct osd_state *osd;
    struct encode_lavc_context *encode_lavc_ctx;
    struct vo_internal *in;
    struct vo_extra extra;

    // --- The following fields are generally only changed during initialization.

    bool probing;

    // --- The following fields are only changed with vo_reconfig(), and can
    //     be accessed unsynchronized (read-only).

    int config_ok;      // Last config call was successful?

    // --- The following fields are synchronized by params_mutex, most of
    //     the params are set only in the vo_reconfig and safe to read
    //     unsynchronized. Some of the parameters are updated in draw_frame,
    //     which are still safe to read in the play loop, but for correctness
    //     generic getter is protected by params_mutex.
    mp_mutex params_mutex;
    struct mp_image_params *params; // Configured parameters (changed in vo_reconfig)
    struct mp_image_params *target_params; // Target display parameters

    // --- The following fields can be accessed only by the VO thread, or from
    //     anywhere _if_ the VO thread is suspended (use vo->dispatch).

    struct m_config_cache *opts_cache; // cache for ->opts
    struct mp_vo_opts *opts;
    struct m_config_cache *gl_opts_cache;
    struct m_config_cache *eq_opts_cache;

    bool want_redraw;   // redraw as soon as possible

    // current window state
    int dwidth;
    int dheight;
    float monitor_par;

    // current GPU context (--vo=gpu and --vo=gpu-next only)
    const char *context_name;
};

struct mpv_global;
struct vo *init_best_video_out(struct mpv_global *global, struct vo_extra *ex);
int vo_reconfig(struct vo *vo, struct mp_image_params *p);
int vo_reconfig2(struct vo *vo, struct mp_image *img);

int vo_control(struct vo *vo, int request, void *data);
void vo_control_async(struct vo *vo, int request, void *data);
bool vo_is_ready_for_frame(struct vo *vo, int64_t next_pts);
void vo_queue_frame(struct vo *vo, struct vo_frame *frame);
void vo_wait_frame(struct vo *vo);
bool vo_still_displaying(struct vo *vo);
void vo_request_wakeup_on_done(struct vo *vo);
bool vo_has_frame(struct vo *vo);
void vo_redraw(struct vo *vo);
bool vo_want_redraw(struct vo *vo);
void vo_seek_reset(struct vo *vo);
void vo_destroy(struct vo *vo);
void vo_set_paused(struct vo *vo, bool paused);
int64_t vo_get_drop_count(struct vo *vo);
void vo_increment_drop_count(struct vo *vo, int64_t n);
int64_t vo_get_delayed_count(struct vo *vo);
void vo_query_formats(struct vo *vo, uint8_t *list);
void vo_event(struct vo *vo, int event);
int vo_query_and_reset_events(struct vo *vo, int events);
struct mp_image *vo_get_current_frame(struct vo *vo);
void vo_set_queue_params(struct vo *vo, int64_t offset_ns, int num_req_frames);
int vo_get_num_req_frames(struct vo *vo);
double vo_get_vsync_interval(struct vo *vo);
double vo_get_estimated_vsync_interval(struct vo *vo);
double vo_get_estimated_vsync_jitter(struct vo *vo);
double vo_get_display_fps(struct vo *vo);
double vo_get_delay(struct vo *vo);
void vo_discard_timing_info(struct vo *vo);
struct vo_frame *vo_get_current_vo_frame(struct vo *vo);
struct mp_image *vo_get_image(struct vo *vo, int imgfmt, int w, int h,
                              int stride_align, int flags);

void vo_wakeup(struct vo *vo);
void vo_wait_default(struct vo *vo, int64_t until_time);

struct mp_keymap {
  int from;
  int to;
};
int lookup_keymap_table(const struct mp_keymap *map, int key);

struct mp_osd_res;
void vo_get_src_dst_rects(struct vo *vo, struct mp_rect *out_src,
                          struct mp_rect *out_dst, struct mp_osd_res *out_osd);

struct vo_frame *vo_frame_ref(struct vo_frame *frame);

struct mp_image_params vo_get_current_params(struct vo *vo);
struct mp_image_params vo_get_target_params(struct vo *vo);

#endif /* MPLAYER_VIDEO_OUT_H */
