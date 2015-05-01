/*
 * Copyright (C) Aaron Holtzman - Aug 1999
 *
 * Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 * (C) MPlayer developers
 *
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

#ifndef MPLAYER_VIDEO_OUT_H
#define MPLAYER_VIDEO_OUT_H

#include <inttypes.h>
#include <stdbool.h>

#include "video/img_format.h"
#include "common/common.h"
#include "options/options.h"

// VO needs to redraw
#define VO_EVENT_EXPOSE 1
// VO needs to update state to a new window size
#define VO_EVENT_RESIZE 2
// The ICC profile needs to be reloaded
#define VO_EVENT_ICC_PROFILE_CHANGED 4
// Some other window state changed (position, window state, fps)
#define VO_EVENT_WIN_STATE 8
// The ambient light conditions changed and need to be reloaded
#define VO_EVENT_AMBIENT_LIGHTING_CHANGED 16

// Set of events the player core may be interested in.
#define VO_EVENTS_USER (VO_EVENT_RESIZE | VO_EVENT_WIN_STATE)

enum mp_voctrl {
    /* signal a device reset seek */
    VOCTRL_RESET = 1,
    /* Handle input and redraw events, called by vo_check_events() */
    VOCTRL_CHECK_EVENTS,
    /* signal a device pause */
    VOCTRL_PAUSE,
    /* start/resume playback */
    VOCTRL_RESUME,

    VOCTRL_GET_PANSCAN,
    VOCTRL_SET_PANSCAN,
    VOCTRL_SET_EQUALIZER,               // struct voctrl_set_equalizer_args*
    VOCTRL_GET_EQUALIZER,               // struct voctrl_get_equalizer_args*

    /* for hardware decoding */
    VOCTRL_GET_HWDEC_INFO,              // struct mp_hwdec_info**
    VOCTRL_LOAD_HWDEC_API,              // private to vo_opengl

    // Redraw the image previously passed to draw_image() (basically, repeat
    // the previous draw_image call). If this is handled, the OSD should also
    // be updated and redrawn. Optional; emulated if not available.
    VOCTRL_REDRAW_FRAME,

    VOCTRL_FULLSCREEN,
    VOCTRL_ONTOP,
    VOCTRL_BORDER,
    VOCTRL_ALL_WORKSPACES,

    VOCTRL_UPDATE_WINDOW_TITLE,         // char*

    VOCTRL_SET_CURSOR_VISIBILITY,       // bool*

    VOCTRL_KILL_SCREENSAVER,
    VOCTRL_RESTORE_SCREENSAVER,

    VOCTRL_SET_DEINTERLACE,
    VOCTRL_GET_DEINTERLACE,

    // Return or set window size (not-fullscreen mode only - if fullscreened,
    // these must access the not-fullscreened window size only).
    VOCTRL_GET_UNFS_WINDOW_SIZE,        // int[2] (w/h)
    VOCTRL_SET_UNFS_WINDOW_SIZE,        // int[2] (w/h)

    VOCTRL_GET_WIN_STATE,               // int* (VO_WIN_STATE_* flags)

    // char *** (NULL terminated array compatible with CONF_TYPE_STRING_LIST)
    // names for displays the window is on
    VOCTRL_GET_DISPLAY_NAMES,

    // Retrieve window contents. (Normal screenshots use vo_get_current_frame().)
    VOCTRL_SCREENSHOT_WIN,              // struct mp_image**

    VOCTRL_SET_COMMAND_LINE,            // char**

    VOCTRL_GET_ICC_PROFILE,             // bstr*
    VOCTRL_GET_AMBIENT_LUX,             // int*
    VOCTRL_GET_DISPLAY_FPS,             // double*
    VOCTRL_GET_RECENT_FLIP_TIME,        // int64_t* (using mp_time_us())

    VOCTRL_GET_PREF_DEINT,              // int*
};

// VOCTRL_SET_EQUALIZER
struct voctrl_set_equalizer_args {
    const char *name;
    int value;
};

// VOCTRL_GET_EQUALIZER
struct voctrl_get_equalizer_args {
    const char *name;
    int *valueptr;
};

// VOCTRL_GET_WIN_STATE
#define VO_WIN_STATE_MINIMIZED 1

#define VO_TRUE         true
#define VO_FALSE        false
#define VO_ERROR        -1
#define VO_NOTAVAIL     -2
#define VO_NOTIMPL      -3

#define VOFLAG_HIDDEN           0x10  //< Use to create a hidden window
#define VOFLAG_GL_DEBUG         0x40  // Hint to request debug OpenGL context
#define VOFLAG_ALPHA            0x80  // Hint to request alpha framebuffer

// VO does handle mp_image_params.rotate in 90 degree steps
#define VO_CAP_ROTATE90 1
// VO does framedrop itself (vo_vdpau). Untimed/encoding VOs never drop.
#define VO_CAP_FRAMEDROP 2

struct vo;
struct osd_state;
struct mp_image;
struct mp_image_params;

struct vo_extra {
    struct input_ctx *input_ctx;
    struct osd_state *osd;
    struct encode_lavc_context *encode_lavc_ctx;
    struct mpv_opengl_cb_context *opengl_cb_context;
};

struct frame_timing {
    int64_t pts;
    int64_t next_vsync;
    int64_t prev_vsync;
};

struct vo_driver {
    // Encoding functionality, which can be invoked via --o only.
    bool encode;

    // VO_CAP_* bits
    int caps;

    // Disable video timing, push frames as quickly as possible, never redraw.
    bool untimed;

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
     *   flags: combination of VOFLAG_ values
     * returns: < 0 on error, >= 0 on success
     */
    int (*reconfig)(struct vo *vo, struct mp_image_params *params, int flags);

    /*
     * Control interface
     */
    int (*control)(struct vo *vo, uint32_t request, void *data);

    /*
     * Render the given frame to the VO's backbuffer. This operation will be
     * followed by a draw_osd and a flip_page[_timed] call.
     * mpi belongs to the VO; the VO must free it eventually.
     *
     * This also should draw the OSD.
     */
    void (*draw_image)(struct vo *vo, struct mp_image *mpi);

    /* Like draw image, but is called before every vsync with timing
     * information
     */
    void (*draw_image_timed)(struct vo *vo, struct mp_image *mpi,
                             struct frame_timing *t);

    /*
     * Blit/Flip buffer to the screen. Must be called after each frame!
     */
    void (*flip_page)(struct vo *vo);

    /*
     * Timed version of flip_page (optional).
     * pts_us is the frame presentation time, linked to mp_time_us().
     * pts_us is 0 if the frame should be presented immediately.
     * duration is estimated time in us until the next frame is shown.
     * duration is -1 if it is unknown or unset (also: disable framedrop).
     * If the VO does manual framedropping, VO_CAP_FRAMEDROP should be set.
     * Returns 1 on display, or 0 if the frame was dropped.
     */
    int (*flip_page_timed)(struct vo *vo, int64_t pts_us, int duration);

    /* These optional callbacks can be provided if the GUI framework used by
     * the VO requires entering a message loop for receiving events, does not
     * provide event_fd, and does not call vo_wakeup() from a separate thread
     * when there are new events.
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
    int (*wait_events)(struct vo *vo, int64_t until_time_us);

    /*
     * Closes driver. Should restore the original state of the system.
     */
    void (*uninit)(struct vo *vo);

    // Size of private struct for automatic allocation (0 doesn't allocate)
    int priv_size;

    // If not NULL, it's copied into the newly allocated private struct.
    const void *priv_defaults;

    // List of options to parse into priv struct (requires priv_size to be set)
    const struct m_option *options;
};

struct vo {
    const struct vo_driver *driver;
    struct mp_log *log; // Using e.g. "[vo/vdpau]" as prefix
    void *priv;
    struct mpv_global *global;
    struct vo_x11_state *x11;
    struct vo_w32_state *w32;
    struct vo_cocoa_state *cocoa;
    struct vo_wayland_state *wayland;
    struct input_ctx *input_ctx;
    struct osd_state *osd;
    struct encode_lavc_context *encode_lavc_ctx;
    struct vo_internal *in;
    struct mp_vo_opts *opts;
    struct vo_extra extra;

    // --- The following fields are generally only changed during initialization.

    int event_fd;  // check_events() should be called when this has input
    bool probing;

    // --- The following fields are only changed with vo_reconfig(), and can
    //     be accessed unsynchronized (read-only).

    int config_ok;      // Last config call was successful?
    struct mp_image_params *params; // Configured parameters (as in vo_reconfig)

    // --- The following fields can be accessed only by the VO thread, or from
    //     anywhere _if_ the VO thread is suspended (use vo->dispatch).

    bool want_redraw;   // redraw as soon as possible

    // current window state
    int dwidth;
    int dheight;
    float monitor_par;
};

struct mpv_global;
struct vo *init_best_video_out(struct mpv_global *global, struct vo_extra *ex);
int vo_reconfig(struct vo *vo, struct mp_image_params *p, int flags);

int vo_control(struct vo *vo, uint32_t request, void *data);
bool vo_is_ready_for_frame(struct vo *vo, int64_t next_pts);
void vo_queue_frame(struct vo *vo, struct mp_image *image,
                    int64_t pts_us, int64_t duration);
void vo_wait_frame(struct vo *vo);
bool vo_still_displaying(struct vo *vo);
bool vo_has_frame(struct vo *vo);
void vo_redraw(struct vo *vo);
bool vo_want_redraw(struct vo *vo);
void vo_seek_reset(struct vo *vo);
void vo_destroy(struct vo *vo);
void vo_set_paused(struct vo *vo, bool paused);
int64_t vo_get_drop_count(struct vo *vo);
void vo_increment_drop_count(struct vo *vo, int64_t n);
void vo_query_formats(struct vo *vo, uint8_t *list);
void vo_event(struct vo *vo, int event);
int vo_query_and_reset_events(struct vo *vo, int events);
struct mp_image *vo_get_current_frame(struct vo *vo);

void vo_set_flip_queue_params(struct vo *vo, int64_t offset_us, bool vsync_timed);
int64_t vo_get_vsync_interval(struct vo *vo);
double vo_get_display_fps(struct vo *vo);

void vo_wakeup(struct vo *vo);

const char *vo_get_window_title(struct vo *vo);

struct mp_keymap {
  int from;
  int to;
};
int lookup_keymap_table(const struct mp_keymap *map, int key);

struct mp_osd_res;
void vo_get_src_dst_rects(struct vo *vo, struct mp_rect *out_src,
                          struct mp_rect *out_dst, struct mp_osd_res *out_osd);

#endif /* MPLAYER_VIDEO_OUT_H */
