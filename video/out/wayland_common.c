/*
 * This file is part of mpv video player.
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

#include <errno.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "common/msg.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "options/m_config.h"
#include "osdep/io.h"
#include "osdep/poll_wrapper.h"
#include "osdep/timer.h"
#include "present_sync.h"
#include "video/out/gpu/video.h"
#include "wayland_common.h"
#include "win_state.h"

// Generated from wayland-protocols
#include "idle-inhibit-unstable-v1.h"
#include "text-input-unstable-v3.h"
#include "linux-dmabuf-unstable-v1.h"
#include "presentation-time.h"
#include "xdg-activation-v1.h"
#include "xdg-decoration-unstable-v1.h"
#include "xdg-shell.h"
#include "viewporter.h"
#include "content-type-v1.h"
#include "single-pixel-buffer-v1.h"
#include "fractional-scale-v1.h"
#include "tablet-unstable-v2.h"

#if HAVE_WAYLAND_PROTOCOLS_1_32
#include "cursor-shape-v1.h"
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_38
#include "fifo-v1.h"
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_41
#include "color-management-v1.h"
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_44
#include "color-representation-v1.h"
#endif

#if WAYLAND_VERSION_MAJOR > 1 || WAYLAND_VERSION_MINOR >= 22
#define HAVE_WAYLAND_1_22
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

#ifndef XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION
#define XDG_TOPLEVEL_STATE_SUSPENDED 9
#endif

// From the fractional scale protocol
#define WAYLAND_SCALE_FACTOR 120.0

// From the color management protocol
#define WAYLAND_COLOR_FACTOR 1000000
#define WAYLAND_MIN_LUM_FACTOR 10000

enum resizing_constraint {
    MP_WIDTH_CONSTRAINT = 1,
    MP_HEIGHT_CONSTRAINT = 2,
};

static const struct mp_keymap keymap[] = {
    /* Special keys */
    {XKB_KEY_Pause,     MP_KEY_PAUSE}, {XKB_KEY_Escape,       MP_KEY_ESC},
    {XKB_KEY_BackSpace, MP_KEY_BS},    {XKB_KEY_Tab,          MP_KEY_TAB},
    {XKB_KEY_Return,    MP_KEY_ENTER}, {XKB_KEY_Menu,         MP_KEY_MENU},
    {XKB_KEY_Print,     MP_KEY_PRINT}, {XKB_KEY_ISO_Left_Tab, MP_KEY_TAB},

    /* Cursor keys */
    {XKB_KEY_Left, MP_KEY_LEFT}, {XKB_KEY_Right, MP_KEY_RIGHT},
    {XKB_KEY_Up,   MP_KEY_UP},   {XKB_KEY_Down,  MP_KEY_DOWN},

    /* Navigation keys */
    {XKB_KEY_Insert,  MP_KEY_INSERT},  {XKB_KEY_Delete,    MP_KEY_DELETE},
    {XKB_KEY_Home,    MP_KEY_HOME},    {XKB_KEY_End,       MP_KEY_END},
    {XKB_KEY_Page_Up, MP_KEY_PAGE_UP}, {XKB_KEY_Page_Down, MP_KEY_PAGE_DOWN},

    /* F-keys */
    {XKB_KEY_F1,  MP_KEY_F + 1},  {XKB_KEY_F2,  MP_KEY_F + 2},
    {XKB_KEY_F3,  MP_KEY_F + 3},  {XKB_KEY_F4,  MP_KEY_F + 4},
    {XKB_KEY_F5,  MP_KEY_F + 5},  {XKB_KEY_F6,  MP_KEY_F + 6},
    {XKB_KEY_F7,  MP_KEY_F + 7},  {XKB_KEY_F8,  MP_KEY_F + 8},
    {XKB_KEY_F9,  MP_KEY_F + 9},  {XKB_KEY_F10, MP_KEY_F +10},
    {XKB_KEY_F11, MP_KEY_F +11},  {XKB_KEY_F12, MP_KEY_F +12},
    {XKB_KEY_F13, MP_KEY_F +13},  {XKB_KEY_F14, MP_KEY_F +14},
    {XKB_KEY_F15, MP_KEY_F +15},  {XKB_KEY_F16, MP_KEY_F +16},
    {XKB_KEY_F17, MP_KEY_F +17},  {XKB_KEY_F18, MP_KEY_F +18},
    {XKB_KEY_F19, MP_KEY_F +19},  {XKB_KEY_F20, MP_KEY_F +20},
    {XKB_KEY_F21, MP_KEY_F +21},  {XKB_KEY_F22, MP_KEY_F +22},
    {XKB_KEY_F23, MP_KEY_F +23},  {XKB_KEY_F24, MP_KEY_F +24},

    /* Numpad independent of numlock */
    {XKB_KEY_KP_Subtract, MP_KEY_KPSUBTRACT}, {XKB_KEY_KP_Add,    MP_KEY_KPADD},
    {XKB_KEY_KP_Multiply, MP_KEY_KPMULTIPLY}, {XKB_KEY_KP_Divide, MP_KEY_KPDIVIDE},
    {XKB_KEY_KP_Enter, MP_KEY_KPENTER},

    /* Numpad with numlock */
    {XKB_KEY_KP_0, MP_KEY_KP0}, {XKB_KEY_KP_1, MP_KEY_KP1},
    {XKB_KEY_KP_2, MP_KEY_KP2}, {XKB_KEY_KP_3, MP_KEY_KP3},
    {XKB_KEY_KP_4, MP_KEY_KP4}, {XKB_KEY_KP_5, MP_KEY_KP5},
    {XKB_KEY_KP_6, MP_KEY_KP6}, {XKB_KEY_KP_7, MP_KEY_KP7},
    {XKB_KEY_KP_8, MP_KEY_KP8}, {XKB_KEY_KP_9, MP_KEY_KP9},
    {XKB_KEY_KP_Decimal, MP_KEY_KPDEC}, {XKB_KEY_KP_Separator, MP_KEY_KPDEC},

    /* Numpad without numlock */
    {XKB_KEY_KP_Insert, MP_KEY_KPINS},   {XKB_KEY_KP_End,       MP_KEY_KPEND},
    {XKB_KEY_KP_Down,   MP_KEY_KPDOWN},  {XKB_KEY_KP_Page_Down, MP_KEY_KPPGDOWN},
    {XKB_KEY_KP_Left,   MP_KEY_KPLEFT},  {XKB_KEY_KP_Begin,     MP_KEY_KPBEGIN},
    {XKB_KEY_KP_Right,  MP_KEY_KPRIGHT}, {XKB_KEY_KP_Home,      MP_KEY_KPHOME},
    {XKB_KEY_KP_Up,     MP_KEY_KPUP},    {XKB_KEY_KP_Page_Up,   MP_KEY_KPPGUP},
    {XKB_KEY_KP_Delete, MP_KEY_KPDEL},

    /* Multimedia keys */
    {XKB_KEY_XF86MenuKB, MP_KEY_MENU},
    {XKB_KEY_XF86AudioPlay, MP_KEY_PLAY}, {XKB_KEY_XF86AudioPause, MP_KEY_PAUSE},
    {XKB_KEY_XF86AudioStop, MP_KEY_STOP},
    {XKB_KEY_XF86AudioPrev, MP_KEY_PREV}, {XKB_KEY_XF86AudioNext, MP_KEY_NEXT},
    {XKB_KEY_XF86AudioRewind, MP_KEY_REWIND},
    {XKB_KEY_XF86AudioForward, MP_KEY_FORWARD},
    {XKB_KEY_XF86AudioMute, MP_KEY_MUTE},
    {XKB_KEY_XF86AudioLowerVolume, MP_KEY_VOLUME_DOWN},
    {XKB_KEY_XF86AudioRaiseVolume, MP_KEY_VOLUME_UP},
    {XKB_KEY_XF86HomePage, MP_KEY_HOMEPAGE}, {XKB_KEY_XF86WWW, MP_KEY_WWW},
    {XKB_KEY_XF86Mail, MP_KEY_MAIL}, {XKB_KEY_XF86Favorites, MP_KEY_FAVORITES},
    {XKB_KEY_XF86Search, MP_KEY_SEARCH}, {XKB_KEY_XF86Sleep, MP_KEY_SLEEP},
    {XKB_KEY_XF86Back, MP_KEY_GO_BACK}, {XKB_KEY_XF86Forward, MP_KEY_GO_FORWARD},
    {XKB_KEY_XF86Tools, MP_KEY_TOOLS},
    {XKB_KEY_XF86ZoomIn, MP_KEY_ZOOMIN}, {XKB_KEY_XF86ZoomOut, MP_KEY_ZOOMOUT},

    {0, 0}
};

struct compositor_format {
    uint32_t format;
    uint32_t padding;
    uint64_t modifier;
};

struct vo_wayland_feedback_pool {
    struct wp_presentation_feedback **fback;
    struct vo_wayland_state *wl;
    int len;
};

struct vo_wayland_output {
    struct vo_wayland_state *wl;
    struct wl_output *output;
    struct mp_rect geometry;
    bool has_surface;
    uint32_t id;
    uint32_t flags;
    int phys_width;
    int phys_height;
    int scale;
    double refresh_rate;
    char *make;
    char *model;
    char *name;
    struct wl_list link;
};

struct vo_wayland_seat {
    struct vo_wayland_state *wl;
    struct wl_seat *seat;
    uint32_t id;
    struct wl_keyboard *keyboard;
    struct wl_pointer  *pointer;
    struct wl_touch    *touch;
    struct zwp_tablet_seat_v2 *tablet_seat;
    struct wl_list tablet_list;
    struct wl_list tablet_tool_list;
    struct wl_list tablet_pad_list;
    struct wl_data_device *data_device;
    struct vo_wayland_data_offer *pending_offer;
    struct vo_wayland_data_offer *dnd_offer;
    struct vo_wayland_data_offer *selection_offer;
    struct wl_data_source *data_source;
    struct vo_wayland_text_input *text_input;
    struct wp_cursor_shape_device_v1 *cursor_shape_device;
    uint32_t pointer_enter_serial;
    uint32_t pointer_button_serial;
    uint32_t last_serial;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;
    uint32_t keyboard_code;
    int mpkey;
    int mpmod;
    double axis_value_vertical;
    int32_t axis_value120_vertical;
    double axis_value_horizontal;
    int32_t axis_value120_horizontal;
    bool axis_value120_scroll;
    bool has_keyboard_input;
    struct wl_list link;
    bool keyboard_entering;
    uint32_t *keyboard_entering_keys;
    int num_keyboard_entering_keys;
};

struct vo_wayland_tablet {
    struct vo_wayland_state *wl;
    struct vo_wayland_seat *seat;
    struct zwp_tablet_v2 *tablet;
    struct wl_list link;
};

struct vo_wayland_tablet_tool {
    struct vo_wayland_state *wl;
    struct vo_wayland_seat *seat;
    struct zwp_tablet_tool_v2 *tablet_tool;
    struct wp_cursor_shape_device_v1 *cursor_shape_device;
    uint32_t proximity_serial;
    struct wl_list link;
};

struct vo_wayland_tablet_pad {
    struct vo_wayland_state *wl;
    struct vo_wayland_seat *seat;
    struct zwp_tablet_pad_v2 *tablet_pad;
    uint32_t buttons; // number of buttons on pad
    struct wl_list tablet_pad_group_list;
    struct wl_list link;
};

struct vo_wayland_tablet_pad_group {
    struct vo_wayland_state *wl;
    struct vo_wayland_seat *seat;
    struct zwp_tablet_pad_group_v2 *tablet_pad_group;
    struct wl_list tablet_pad_ring_list;
    struct wl_list tablet_pad_strip_list;
    struct wl_list link;
};

struct vo_wayland_tablet_pad_ring {
    struct vo_wayland_state *wl;
    struct vo_wayland_seat *seat;
    struct zwp_tablet_pad_ring_v2 *tablet_pad_ring;
    struct wl_list link;
};

struct vo_wayland_tablet_pad_strip {
    struct vo_wayland_state *wl;
    struct vo_wayland_seat *seat;
    struct zwp_tablet_pad_strip_v2 *tablet_pad_strip;
    struct wl_list link;
};

struct vo_wayland_tranche {
    struct drm_format *compositor_formats;
    int num_compositor_formats;
    dev_t device_id;
    struct wl_list link;
};

struct vo_wayland_data_offer {
    struct wl_data_offer *offer;
    int action; // actually enum mp_dnd_action
    char *mime_type;
    int fd;
    int mime_score;
    bool offered_plain_text;
};

struct vo_wayland_text_input {
    struct zwp_text_input_v3 *text_input;
    uint32_t serial;
    char *commit_string;
    bool has_focus;
};

struct vo_wayland_preferred_description_info {
    struct vo_wayland_state *wl;
    bool is_parametric;
    struct pl_color_space csp;
    void *icc_file;
    uint32_t icc_size;
};

static bool single_output_spanned(struct vo_wayland_state *wl);

static int check_for_resize(struct vo_wayland_state *wl, int edge_pixels,
                            enum xdg_toplevel_resize_edge *edges);
static int get_mods(struct vo_wayland_seat *seat);
static int greatest_common_divisor(int a, int b);
static int handle_round(int scale, int n);
static int set_cursor_visibility(struct vo_wayland_seat *s, bool on);
static int spawn_cursor(struct vo_wayland_state *wl);

static void add_feedback(struct vo_wayland_feedback_pool *fback_pool,
                         struct wp_presentation_feedback *fback);
static void apply_keepaspect(struct vo_wayland_state *wl, int *width, int *height);
static void get_compositor_preferred_description(struct vo_wayland_state *wl, bool parametric);
static void get_shape_device(struct vo_wayland_state *wl, struct vo_wayland_seat *s);
static void guess_focus(struct vo_wayland_state *wl);
static void handle_key_input(struct vo_wayland_seat *s, uint32_t key, uint32_t state, bool no_emit);
static void remove_feedback(struct vo_wayland_feedback_pool *fback_pool,
                            struct wp_presentation_feedback *fback);
static void remove_output(struct vo_wayland_output *out);
static void remove_tablet(struct vo_wayland_tablet *tablet);
static void remove_tablet_tool(struct vo_wayland_tablet_tool *tablet_tool);
static void remove_tablet_pad(struct vo_wayland_tablet_pad *tablet_pad);
static void remove_seat(struct vo_wayland_seat *seat);
static void seat_create_data_device(struct vo_wayland_seat *seat);
static void seat_create_tablet_seat(struct vo_wayland_state *wl, struct vo_wayland_seat *seat);
static void seat_create_text_input(struct vo_wayland_seat *seat);
static void request_decoration_mode(struct vo_wayland_state *wl, uint32_t mode);
static void rescale_geometry(struct vo_wayland_state *wl, double old_scale);
static void set_geometry(struct vo_wayland_state *wl, bool resize);
static void set_surface_scaling(struct vo_wayland_state *wl);
static void update_output_scaling(struct vo_wayland_state *wl);
static void update_output_geometry(struct vo_wayland_state *wl, struct mp_rect old_geometry,
                                   struct mp_rect old_output_geometry);
static void destroy_offer(struct vo_wayland_data_offer *o);

/* Wayland listener boilerplate */
static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;

    s->pointer_enter_serial = serial;
    set_cursor_visibility(s, wl->cursor_visible);
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_MOUSE_ENTER);

    wl->mouse_x = handle_round(wl->scaling, wl_fixed_to_int(sx));
    wl->mouse_y = handle_round(wl->scaling, wl_fixed_to_int(sy));

    mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y,
                           wl->toplevel_configured);
    wl->toplevel_configured = false;
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_MOUSE_LEAVE);
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    wl->mouse_x = handle_round(wl->scaling, wl_fixed_to_int(sx));
    wl->mouse_y = handle_round(wl->scaling, wl_fixed_to_int(sy));

    mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y,
                           wl->toplevel_configured);
    wl->toplevel_configured = false;
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;
    state = state == WL_POINTER_BUTTON_STATE_PRESSED ? MP_KEY_STATE_DOWN
                                                     : MP_KEY_STATE_UP;

    if (button >= BTN_MOUSE && button < BTN_JOYSTICK) {
        switch (button) {
        case BTN_LEFT:
            button = MP_MBTN_LEFT;
            break;
        case BTN_MIDDLE:
            button = MP_MBTN_MID;
            break;
        case BTN_RIGHT:
            button = MP_MBTN_RIGHT;
            break;
        case BTN_SIDE:
            button = MP_MBTN_BACK;
            break;
        case BTN_EXTRA:
            button = MP_MBTN_FORWARD;
            break;
        default:
            button += MP_MBTN9 - BTN_FORWARD;
            break;
        }
    } else {
        button = 0;
    }
    enum xdg_toplevel_resize_edge edges;
    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y) &&
        !wl->locked_size && (button == MP_MBTN_LEFT) && (state == MP_KEY_STATE_DOWN) &&
        !wl->opts->border && check_for_resize(wl, wl->opts->wl_edge_pixels_pointer, &edges))
    {
        // Implement an edge resize zone if there are no decorations
        xdg_toplevel_resize(wl->xdg_toplevel, s->seat, serial, edges);
        return;
    } else if (state == MP_KEY_STATE_DOWN) {
        // Save the serial and seat for voctrl-initialized dragging requests.
        s->pointer_button_serial = serial;
        wl->last_button_seat = s;
    } else {
        wl->last_button_seat = NULL;
    }

    if (button)
        mp_input_put_key(wl->vo->input_ctx, button | state | s->mpmod);
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value)
{
    struct vo_wayland_seat *s = data;
    switch (axis) {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        s->axis_value_vertical += wl_fixed_to_double(value);
        break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        s->axis_value_horizontal += wl_fixed_to_double(value);
        break;
    }
}

static void pointer_handle_frame(void *data, struct wl_pointer *wl_pointer)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    double value_vertical, value_horizontal;
    if (s->axis_value120_scroll) {
        // Prefer axis_value120 if supported and the axis event is from mouse wheel.
        value_vertical = s->axis_value120_vertical / 120.0;
        value_horizontal = s->axis_value120_horizontal / 120.0;
    } else {
        // The axis value is specified in logical coordinates, but the exact value emitted
        // by one mouse wheel click is unspecified. In practice, most compositors use either
        // 10 (GNOME, Weston) or 15 (wlroots, same as libinput) as the value.
        // Divide the value by 10 and clamp it between -1 and 1 so that mouse wheel clicks
        // work as intended on all compositors while still allowing high resolution trackpads.
        value_vertical = MPCLAMP(s->axis_value_vertical / 10.0, -1, 1);
        value_horizontal = MPCLAMP(s->axis_value_horizontal / 10.0, -1, 1);
    }

    if (value_vertical > 0)
        mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_DOWN | s->mpmod, +value_vertical);
    if (value_vertical < 0)
        mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_UP | s->mpmod, -value_vertical);
    if (value_horizontal > 0)
        mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_RIGHT | s->mpmod, +value_horizontal);
    if (value_horizontal < 0)
        mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_LEFT | s->mpmod, -value_horizontal);

    s->axis_value120_scroll = false;
    s->axis_value_vertical = 0;
    s->axis_value_horizontal = 0;
    s->axis_value120_vertical = 0;
    s->axis_value120_horizontal = 0;
}

static void pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer,
                                       uint32_t axis_source)
{
}

static void pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer,
                                     uint32_t time, uint32_t axis)
{
}

static void pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer,
                                         uint32_t axis, int32_t discrete)
{
}

static void pointer_handle_axis_value120(void *data, struct wl_pointer *wl_pointer,
                                         uint32_t axis, int32_t value120)
{
    struct vo_wayland_seat *s = data;
    s->axis_value120_scroll = true;
    switch (axis) {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        s->axis_value120_vertical += value120;
        break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        s->axis_value120_horizontal += value120;
        break;
    }
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
    pointer_handle_frame,
    pointer_handle_axis_source,
    pointer_handle_axis_stop,
    pointer_handle_axis_discrete,
    pointer_handle_axis_value120,
};

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
                              uint32_t serial, uint32_t time, struct wl_surface *surface,
                              int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;
    // Note: the position should still be saved here for VO dragging handling.
    wl->mouse_x = handle_round(wl->scaling, wl_fixed_to_int(x_w));
    wl->mouse_y = handle_round(wl->scaling, wl_fixed_to_int(y_w));

    enum xdg_toplevel_resize_edge edge;
    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y) &&
        !wl->locked_size && check_for_resize(wl, wl->opts->wl_edge_pixels_touch, &edge))
    {
        xdg_toplevel_resize(wl->xdg_toplevel, s->seat, serial, edge);
        return;
    } else {
        // Save the serial and seat for voctrl-initialized dragging requests.
        s->pointer_button_serial = serial;
        wl->last_button_seat = s;
    }

    mp_input_add_touch_point(wl->vo->input_ctx, id, wl->mouse_x, wl->mouse_y);
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
                            uint32_t serial, uint32_t time, int32_t id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;
    mp_input_remove_touch_point(wl->vo->input_ctx, id);
    wl->last_button_seat = NULL;
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
                                uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    wl->mouse_x = handle_round(wl->scaling, wl_fixed_to_int(x_w));
    wl->mouse_y = handle_round(wl->scaling, wl_fixed_to_int(y_w));

    mp_input_update_touch_point(wl->vo->input_ctx, id, wl->mouse_x, wl->mouse_y);
}

static void touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    mp_input_put_key(wl->vo->input_ctx, MP_TOUCH_RELEASE_ALL);
}

static void touch_handle_shape(void *data, struct wl_touch *wl_touch,
                               int32_t id, wl_fixed_t major, wl_fixed_t minor)
{
}

static void touch_handle_orientation(void *data, struct wl_touch *wl_touch,
                                     int32_t id, wl_fixed_t orientation)
{
}

static const struct wl_touch_listener touch_listener = {
    touch_handle_down,
    touch_handle_up,
    touch_handle_motion,
    touch_handle_frame,
    touch_handle_cancel,
    touch_handle_shape,
    touch_handle_orientation,
};

static void tablet_handle_name(void *data,
                               struct zwp_tablet_v2 *zwp_tablet_v2,
                               const char *name)
{
}

static void tablet_handle_id(void *data,
                             struct zwp_tablet_v2 *zwp_tablet_v2,
                             uint32_t vid,
                             uint32_t pid)
{
}

static void tablet_handle_path(void *data,
                               struct zwp_tablet_v2 *zwp_tablet_v2,
                               const char *path)
{
}

static void tablet_handle_done(void *data,
                               struct zwp_tablet_v2 *zwp_tablet_v2)
{
}

static void tablet_handle_removed(void *data,
                                  struct zwp_tablet_v2 *zwp_tablet_v2)
{
    struct vo_wayland_tablet *tablet = data;
    remove_tablet(tablet);
}

static const struct zwp_tablet_v2_listener tablet_listener = {
    tablet_handle_name,
    tablet_handle_id,
    tablet_handle_path,
    tablet_handle_done,
    tablet_handle_removed,
};

static void tablet_tool_handle_type(void *data,
                                    struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                    uint32_t tool_type)
{
}

static void tablet_tool_handle_hardware_serial(void *data,
                                               struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                               uint32_t hardware_serial_hi,
                                               uint32_t hardware_serial_lo)
{
}

static void tablet_tool_handle_hardware_id_wacom(void *data,
                                                 struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                                 uint32_t hardware_id_hi,
                                                 uint32_t hardware_id_lo)
{
}

static void tablet_tool_handle_capability(void *data,
                                          struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                          uint32_t capability)
{
}

static void tablet_tool_handle_done(void *data,
                                    struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2)
{
}

static void tablet_tool_handle_removed(void *data,
                                       struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    remove_tablet_tool(tablet_tool);
}

static void tablet_tool_handle_proximity_in(void *data,
                                            struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                            uint32_t serial,
                                            struct zwp_tablet_v2 *tablet,
                                            struct wl_surface *surface)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    tablet_tool->proximity_serial = serial;
    set_cursor_visibility(tablet_tool->seat, true);
    mp_input_set_tablet_tool_in_proximity(tablet_tool->wl->vo->input_ctx, true);
}

static void tablet_tool_handle_proximity_out(void *data,
                                             struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    mp_input_set_tablet_tool_in_proximity(tablet_tool->wl->vo->input_ctx, false);
}

static void tablet_tool_handle_down(void *data,
                                    struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                    uint32_t serial)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    struct vo_wayland_state *wl = tablet_tool->wl;
    tablet_tool->seat->last_serial = serial;

    enum xdg_toplevel_resize_edge edge;
    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y) &&
        !wl->locked_size && !wl->opts->border &&
        check_for_resize(wl, wl->opts->wl_edge_pixels_touch, &edge))
    {
        xdg_toplevel_resize(wl->xdg_toplevel, tablet_tool->seat->seat, serial, edge);
        return;
    }

    tablet_tool->seat->pointer_button_serial = serial;
    wl->last_button_seat = tablet_tool->seat;
    mp_input_tablet_tool_down(tablet_tool->wl->vo->input_ctx);
}

static void tablet_tool_handle_up(void *data,
                                  struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    tablet_tool->seat->wl->last_button_seat = NULL;
    mp_input_tablet_tool_up(tablet_tool->wl->vo->input_ctx);
}

static void tablet_tool_handle_motion(void *data,
                                      struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                      wl_fixed_t x,
                                      wl_fixed_t y)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    struct vo_wayland_state *wl = tablet_tool->wl;

    wl->mouse_x = handle_round(wl->scaling, wl_fixed_to_int(x));
    wl->mouse_y = handle_round(wl->scaling, wl_fixed_to_int(y));

    mp_input_set_tablet_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y,
                           wl->toplevel_configured);
    wl->toplevel_configured = false;
}

static void tablet_tool_handle_pressure(void *data,
                                        struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                        uint32_t pressure)
{
}

static void tablet_tool_handle_distance(void *data,
                                        struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                        uint32_t distance)
{
}

static void tablet_tool_handle_tilt(void *data,
                                    struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                    wl_fixed_t tilt_x,
                                    wl_fixed_t tilt_y)
{
}

static void tablet_tool_handle_rotation(void *data,
                                        struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                        wl_fixed_t degrees)
{
}

static void tablet_tool_handle_slider(void *data,
                                      struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                      int32_t position)
{
}

static void tablet_tool_handle_wheel(void *data,
                                     struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                     wl_fixed_t degrees,
                                     int32_t clicks)
{
}

static void tablet_tool_handle_button(void *data,
                                      struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                      uint32_t serial,
                                      uint32_t button,
                                      uint32_t state)
{
    struct vo_wayland_tablet_tool *tablet_tool = data;
    tablet_tool->seat->last_serial = serial;

    switch (button) {
    case BTN_STYLUS:
        button = MP_KEY_TABLET_TOOL_STYLUS_BTN1;
        break;
    case BTN_STYLUS2:
        button = MP_KEY_TABLET_TOOL_STYLUS_BTN2;
        break;
    case BTN_STYLUS3:
        button = MP_KEY_TABLET_TOOL_STYLUS_BTN3;
        break;
    default:
        button = 0;
        break;
    }

    state = state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED
        ? MP_KEY_STATE_DOWN
        : MP_KEY_STATE_UP;

    mp_input_tablet_tool_button(tablet_tool->wl->vo->input_ctx, button, state);
}

static void tablet_tool_handle_frame(void *data,
                                     struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2,
                                     uint32_t time)
{
}

static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
    tablet_tool_handle_type,
    tablet_tool_handle_hardware_serial,
    tablet_tool_handle_hardware_id_wacom,
    tablet_tool_handle_capability,
    tablet_tool_handle_done,
    tablet_tool_handle_removed,
    tablet_tool_handle_proximity_in,
    tablet_tool_handle_proximity_out,
    tablet_tool_handle_down,
    tablet_tool_handle_up,
    tablet_tool_handle_motion,
    tablet_tool_handle_pressure,
    tablet_tool_handle_distance,
    tablet_tool_handle_tilt,
    tablet_tool_handle_rotation,
    tablet_tool_handle_slider,
    tablet_tool_handle_wheel,
    tablet_tool_handle_button,
    tablet_tool_handle_frame,
};

static void tablet_tool_pad_group_handle_buttons(void *data,
                                                 struct zwp_tablet_pad_group_v2 *zwp_tablet_pad_group_v2,
                                                 struct wl_array *buttons)
{
}

static void tablet_tool_pad_group_handle_ring(void *data,
                                              struct zwp_tablet_pad_group_v2 *zwp_tablet_pad_group_v2,
                                              struct zwp_tablet_pad_ring_v2 *ring)
{
    struct vo_wayland_tablet_pad_group *tablet_pad_group = data;
    struct vo_wayland_state *wl = tablet_pad_group->wl;
    struct vo_wayland_seat *seat = tablet_pad_group->seat;

    struct vo_wayland_tablet_pad_ring *tablet_pad_ring = talloc_zero(seat, struct vo_wayland_tablet_pad_ring);
    tablet_pad_ring->wl = wl;
    tablet_pad_ring->seat = seat;
    tablet_pad_ring->tablet_pad_ring = ring;
    wl_list_insert(&tablet_pad_group->tablet_pad_ring_list, &tablet_pad_ring->link);
}

static void tablet_tool_pad_group_handle_strip(void *data,
                                               struct zwp_tablet_pad_group_v2 *zwp_tablet_pad_group_v2,
                                               struct zwp_tablet_pad_strip_v2 *strip)
{
    struct vo_wayland_tablet_pad_group *tablet_pad_group = data;
    struct vo_wayland_state *wl = tablet_pad_group->wl;
    struct vo_wayland_seat *seat = tablet_pad_group->seat;

    struct vo_wayland_tablet_pad_strip *tablet_pad_strip = talloc_zero(seat, struct vo_wayland_tablet_pad_strip);
    tablet_pad_strip->wl = wl;
    tablet_pad_strip->seat = seat;
    tablet_pad_strip->tablet_pad_strip = strip;
    wl_list_insert(&tablet_pad_group->tablet_pad_strip_list, &tablet_pad_strip->link);
}

static void tablet_tool_pad_group_handle_modes(void *data,
                                               struct zwp_tablet_pad_group_v2 *zwp_tablet_pad_group_v2,
                                               uint32_t modes)
{
}

static void tablet_tool_pad_group_handle_done(void *data,
                                              struct zwp_tablet_pad_group_v2 *zwp_tablet_pad_group_v2)
{
}

static void tablet_tool_pad_group_handle_mode_switch(void *data,
                                                     struct zwp_tablet_pad_group_v2 *zwp_tablet_pad_group_v2,
                                                     uint32_t time,
                                                     uint32_t serial,
                                                     uint32_t mode)
{
}

static const struct zwp_tablet_pad_group_v2_listener tablet_pad_group_listener = {
    tablet_tool_pad_group_handle_buttons,
    tablet_tool_pad_group_handle_ring,
    tablet_tool_pad_group_handle_strip,
    tablet_tool_pad_group_handle_modes,
    tablet_tool_pad_group_handle_done,
    tablet_tool_pad_group_handle_mode_switch
};

static void tablet_pad_handle_group(void *data,
                                    struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
                                    struct zwp_tablet_pad_group_v2 *pad_group)
{
    struct vo_wayland_tablet_pad *tablet_pad = data;
    struct vo_wayland_state *wl = tablet_pad->wl;
    struct vo_wayland_seat *seat = tablet_pad->seat;

    struct vo_wayland_tablet_pad_group *tablet_pad_group = talloc_zero(seat, struct vo_wayland_tablet_pad_group);
    tablet_pad_group->wl = wl;
    tablet_pad_group->seat = seat;
    tablet_pad_group->tablet_pad_group = pad_group;
    wl_list_init(&tablet_pad_group->tablet_pad_ring_list);
    wl_list_init(&tablet_pad_group->tablet_pad_strip_list);
    zwp_tablet_pad_group_v2_add_listener(pad_group, &tablet_pad_group_listener, tablet_pad_group);
    wl_list_insert(&tablet_pad->tablet_pad_group_list, &tablet_pad_group->link);
}

static void tablet_pad_handle_path(void *data,
                                   struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
                                   const char *path)
{
}

static void tablet_pad_handle_buttons(void *data,
                                      struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
                                      uint32_t buttons)
{
    struct vo_wayland_tablet_pad *tablet_pad = data;
    struct vo_wayland_state *wl = tablet_pad->wl;

    MP_VERBOSE(wl, "       %i buttons\n", buttons);
    tablet_pad->buttons = buttons;
}

static void tablet_pad_handle_done(void *data,
                                   struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2)
{
}

static void tablet_pad_handle_button(void *data,
                                     struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
                                     uint32_t time,
                                     uint32_t button,
                                     uint32_t state)
{
    struct vo_wayland_tablet_pad *tablet_pad = data;
    struct vo_wayland_state *wl = tablet_pad->wl;

    state = state == ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED
        ? MP_KEY_STATE_DOWN
        : MP_KEY_STATE_UP;

    mp_input_tablet_pad_button(wl->vo->input_ctx, button, state);
}

static void tablet_pad_handle_enter(void *data,
                                    struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
                                    uint32_t serial,
                                    struct zwp_tablet_v2 *tablet,
                                    struct wl_surface *surface)
{
    struct vo_wayland_tablet_pad *tablet_pad = data;
    struct vo_wayland_state *wl = tablet_pad->wl;
    mp_input_set_tablet_pad_focus(wl->vo->input_ctx, true, tablet_pad->buttons);
}

static void tablet_pad_handle_leave(void *data,
                                    struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
                                    uint32_t serial,
                                    struct wl_surface *surface)
{
    struct vo_wayland_tablet_pad *tablet_pad = data;
    struct vo_wayland_state *wl = tablet_pad->wl;
    mp_input_set_tablet_pad_focus(wl->vo->input_ctx, false, 0);
}

static void tablet_pad_handle_removed(void *data,
                                      struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2)
{
    struct vo_wayland_tablet_pad *tablet_pad = data;
    remove_tablet_pad(tablet_pad);
}

static const struct zwp_tablet_pad_v2_listener tablet_pad_listener = {
    tablet_pad_handle_group,
    tablet_pad_handle_path,
    tablet_pad_handle_buttons,
    tablet_pad_handle_done,
    tablet_pad_handle_button,
    tablet_pad_handle_enter,
    tablet_pad_handle_leave,
    tablet_pad_handle_removed,
};

static void tablet_handle_added(void *data,
                                struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2,
                                struct zwp_tablet_v2 *id)
{
    struct vo_wayland_seat *seat = data;
    struct vo_wayland_state *wl = seat->wl;

    MP_VERBOSE(wl, "Adding tablet %p\n", id);

    struct vo_wayland_tablet *tablet = talloc_zero(seat, struct vo_wayland_tablet);
    tablet->wl = wl;
    tablet->seat = seat;
    tablet->tablet = id;
    zwp_tablet_v2_add_listener(id, &tablet_listener, tablet);
    wl_list_insert(&seat->tablet_list, &tablet->link);
}

static void tablet_tool_handle_added(void *data,
                                     struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2,
                                     struct zwp_tablet_tool_v2 *id)
{
    struct vo_wayland_seat *seat = data;
    struct vo_wayland_state *wl = seat->wl;

    MP_VERBOSE(wl, "Adding tablet tool %p\n", id);

    struct vo_wayland_tablet_tool *tablet_tool = talloc_zero(seat, struct vo_wayland_tablet_tool);
    tablet_tool->wl = wl;
    tablet_tool->seat = seat;
    tablet_tool->tablet_tool = id;
#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (wl->cursor_shape_manager)
        tablet_tool->cursor_shape_device = wp_cursor_shape_manager_v1_get_tablet_tool_v2(
            wl->cursor_shape_manager, tablet_tool->tablet_tool);
#endif
    zwp_tablet_tool_v2_add_listener(tablet_tool->tablet_tool, &tablet_tool_listener, tablet_tool);
    wl_list_insert(&seat->tablet_tool_list, &tablet_tool->link);
}

static void tablet_pad_handle_added(void *data,
                                    struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2,
                                    struct zwp_tablet_pad_v2 *id)
{
    struct vo_wayland_seat *seat = data;
    struct vo_wayland_state *wl = seat->wl;

    MP_VERBOSE(wl, "Adding tablet pad %p\n", id);

    struct vo_wayland_tablet_pad *tablet_pad = talloc_zero(seat, struct vo_wayland_tablet_pad);
    tablet_pad->wl = wl;
    tablet_pad->seat = seat;
    tablet_pad->tablet_pad = id;
    wl_list_init(&tablet_pad->tablet_pad_group_list);
    zwp_tablet_pad_v2_add_listener(id, &tablet_pad_listener, tablet_pad);
    wl_list_insert(&seat->tablet_pad_list, &tablet_pad->link);
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
    tablet_handle_added,
    tablet_tool_handle_added,
    tablet_pad_handle_added,
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t format, int32_t fd, uint32_t size)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    char *map_str;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    if (s->xkb_keymap)
        xkb_keymap_unref(s->xkb_keymap);

    s->xkb_keymap = xkb_keymap_new_from_buffer(wl->xkb_context, map_str,
                                               strnlen(map_str, size),
                                               XKB_KEYMAP_FORMAT_TEXT_V1, 0);

    munmap(map_str, size);
    close(fd);

    if (!s->xkb_keymap) {
        MP_ERR(wl, "failed to compile keymap\n");
        return;
    }
    if (s->xkb_state)
        xkb_state_unref(s->xkb_state);

    s->xkb_state = xkb_state_new(s->xkb_keymap);
    if (!s->xkb_state) {
        MP_ERR(wl, "failed to create XKB state\n");
        xkb_keymap_unref(s->xkb_keymap);
        s->xkb_keymap = NULL;
        return;
    }
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;
    s->has_keyboard_input = true;
    s->keyboard_entering = true;
    guess_focus(wl);

    uint32_t *key;
    wl_array_for_each(key, keys)
        MP_TARRAY_APPEND(s, s->keyboard_entering_keys, s->num_keyboard_entering_keys, *key);
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;
    s->has_keyboard_input = false;
    s->keyboard_code = 0;
    s->mpkey = 0;
    s->mpmod = 0;
    mp_input_put_key(wl->vo->input_ctx, MP_INPUT_RELEASE_ALL);
    guess_focus(wl);
}

static void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state)
{
    struct vo_wayland_seat *s = data;
    s->last_serial = serial;
    handle_key_input(s, key, state, false);
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    s->last_serial = serial;

    if (s->xkb_state) {
        xkb_state_update_mask(s->xkb_state, mods_depressed, mods_latched,
                              mods_locked, 0, 0, group);
        s->mpmod = get_mods(s);
    }
    // Handle keys pressed during the enter event.
    if (s->keyboard_entering) {
        s->keyboard_entering = false;
        // Only handle entering keys if only one key is pressed since
        // Wayland doesn't guarantee that these keys are in order.
        if (s->num_keyboard_entering_keys == 1)
            for (int n = 0; n < s->num_keyboard_entering_keys; n++)
                handle_key_input(s, s->keyboard_entering_keys[n], WL_KEYBOARD_KEY_STATE_PRESSED, true);
        s->num_keyboard_entering_keys = 0;
    } else if (s->xkb_state && s->mpkey) {
        mp_input_put_key(wl->vo->input_ctx, s->mpkey | MP_KEY_STATE_DOWN | s->mpmod);
    }
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                        int32_t rate, int32_t delay)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    if (wl->opts->native_keyrepeat)
        mp_input_set_repeat_info(wl->vo->input_ctx, rate, delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
    keyboard_handle_repeat_info,
};

static void seat_handle_caps(void *data, struct wl_seat *seat,
                             enum wl_seat_capability caps)
{
    struct vo_wayland_seat *s = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !s->pointer) {
        s->pointer = wl_seat_get_pointer(seat);
        get_shape_device(s->wl, s);
        wl_pointer_add_listener(s->pointer, &pointer_listener, s);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && s->pointer) {
        wl_pointer_destroy(s->pointer);
        s->pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !s->keyboard) {
        s->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(s->keyboard, &keyboard_listener, s);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && s->keyboard) {
        wl_keyboard_destroy(s->keyboard);
        s->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !s->touch) {
        s->touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(s->touch, s);
        wl_touch_add_listener(s->touch, &touch_listener, s);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && s->touch) {
        wl_touch_destroy(s->touch);
        s->touch = NULL;
    }
}

static void seat_handle_name(void *data, struct wl_seat *seat,
                             const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_caps,
    seat_handle_name,
};

static void data_offer_handle_offer(void *data, struct wl_data_offer *offer,
                                    const char *mime_type)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    struct vo_wayland_data_offer *o = s->pending_offer;
    int score = mp_event_get_mime_type_score(wl->vo->input_ctx, mime_type);
    if (o->offer && score > o->mime_score && wl->opts->drag_and_drop != -2) {
        o->mime_score = score;
        talloc_replace(wl, o->mime_type, mime_type);
        MP_VERBOSE(wl, "Given data offer with mime type %s\n", o->mime_type);
    }
    if (o->offer && !o->offered_plain_text)
        o->offered_plain_text = !strcmp(mime_type, "text/plain;charset=utf-8");
}

static void data_offer_source_actions(void *data, struct wl_data_offer *offer, uint32_t source_actions)
{
}

static void data_offer_action(void *data, struct wl_data_offer *wl_data_offer, uint32_t dnd_action)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    struct vo_wayland_data_offer *o = s->dnd_offer;
    if (dnd_action && wl->opts->drag_and_drop != -2) {
        if (wl->opts->drag_and_drop >= 0) {
            o->action = wl->opts->drag_and_drop;
        } else {
            o->action = dnd_action & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY ?
                            DND_REPLACE : DND_APPEND;
        }

        static const char * const dnd_action_names[] = {
            [DND_REPLACE] = "DND_REPLACE",
            [DND_APPEND] = "DND_APPEND",
            [DND_INSERT_NEXT] = "DND_INSERT_NEXT",
        };

        MP_VERBOSE(wl, "DND action is %s\n", dnd_action_names[o->action]);
    }
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_handle_offer,
    data_offer_source_actions,
    data_offer_action,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *wl_ddev,
                                          struct wl_data_offer *id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_data_offer *o = s->pending_offer;
    destroy_offer(o);

    o->offer = id;
    wl_data_offer_add_listener(id, &data_offer_listener, s);
}

static void data_device_handle_enter(void *data, struct wl_data_device *wl_ddev,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y,
                                     struct wl_data_offer *id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    struct vo_wayland_data_offer *o = s->pending_offer;
    if (o->offer != id) {
        MP_FATAL(wl, "DND offer ID mismatch!\n");
        return;
    }

    mp_assert(!s->dnd_offer->offer);
    int action = s->dnd_offer->action;
    *s->dnd_offer = *s->pending_offer;
    *s->pending_offer = (struct vo_wayland_data_offer){.fd = -1};
    o = s->dnd_offer;
    o->action = action;
    if (wl->opts->drag_and_drop != -2) {
        wl_data_offer_set_actions(id, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                                      WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                                      WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
        wl_data_offer_accept(id, serial, o->mime_type);
        MP_VERBOSE(wl, "Accepting DND offer with mime type %s\n", o->mime_type);
    }

}

static void data_device_handle_leave(void *data, struct wl_data_device *wl_ddev)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    struct vo_wayland_data_offer *o = s->dnd_offer;

    if (o->offer) {
        if (o->fd != -1)
            return;
        wl_data_offer_destroy(o->offer);
        o->offer = NULL;
    }

    if (wl->opts->drag_and_drop != -2) {
        MP_VERBOSE(wl, "Releasing DND offer with mime type %s\n", o->mime_type);
        if (o->mime_type)
            TA_FREEP(&o->mime_type);
        o->mime_score = 0;
    }
}

static void data_device_handle_motion(void *data, struct wl_data_device *wl_ddev,
                                      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_data_offer *o = s->dnd_offer;
    wl_data_offer_accept(o->offer, time, o->mime_type);
}

static void data_device_handle_drop(void *data, struct wl_data_device *wl_ddev)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    struct vo_wayland_data_offer *o = s->dnd_offer;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) == -1) {
        MP_ERR(wl, "Failed to create dnd pipe!\n");
        return;
    }

    if (wl->opts->drag_and_drop != -2) {
        MP_VERBOSE(wl, "Receiving DND offer with mime %s\n", o->mime_type);
        wl_data_offer_receive(o->offer, o->mime_type, pipefd[1]);
    }

    close(pipefd[1]);
    o->fd = pipefd[0];
}

static void data_device_handle_selection(void *data, struct wl_data_device *wl_ddev,
                                         struct wl_data_offer *id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    struct vo_wayland_data_offer *o = s->pending_offer;
    if (o->offer != id) {
        MP_FATAL(wl, "Selection offer ID mismatch!\n");
        return;
    }

    if (s->selection_offer->offer) {
        destroy_offer(s->selection_offer);
        MP_VERBOSE(wl, "Received a new selection offer. Releasing the previous offer.\n");
    }
    *s->selection_offer = *s->pending_offer;
    *s->pending_offer = (struct vo_wayland_data_offer){.fd = -1};
    if (!id)
        return;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) == -1) {
        MP_ERR(wl, "Failed to create selection pipe!\n");
        return;
    }

    o = s->selection_offer;
    // Only receive plain text for now, may expand later.
    if (o->offered_plain_text)
        wl_data_offer_receive(o->offer, "text/plain;charset=utf-8", pipefd[1]);
    close(pipefd[1]);
    o->fd = pipefd[0];
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_enter,
    data_device_handle_leave,
    data_device_handle_motion,
    data_device_handle_drop,
    data_device_handle_selection,
};

static void data_source_handle_target(void *data, struct wl_data_source *wl_data_source,
                                      const char *mime_type)
{
}

static void data_source_handle_send(void *data, struct wl_data_source *wl_data_source,
                                    const char *mime_type, int32_t fd)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    int score = mp_event_get_mime_type_score(wl->vo->input_ctx, mime_type);
    if (score >= 0) {
        struct pollfd fdp = { .fd = fd, .events = POLLOUT };
        if (poll(&fdp, 1, 0) <= 0)
            goto cleanup;
        if (fdp.revents & (POLLERR | POLLHUP)) {
            MP_VERBOSE(wl, "data source send aborted (write error)\n");
            goto cleanup;
        }

        if (fdp.revents & POLLOUT) {
            ssize_t data_written = write(fd, wl->selection_text.start, wl->selection_text.len);
            if (data_written == -1) {
                MP_VERBOSE(wl, "data source send aborted (write error: %s)\n", mp_strerror(errno));
            } else {
                MP_VERBOSE(wl, "%zu bytes written to the data source fd\n", data_written);
            }
        }
    }

cleanup:
    close(fd);
}

static void data_source_handle_cancelled(void *data, struct wl_data_source *wl_data_source)
{
    // This can happen when another client sets selection, which invalidates the current source.
    struct vo_wayland_seat *s = data;
    wl_data_source_destroy(wl_data_source);
    s->data_source = NULL;
}

static void data_source_handle_dnd_drop_performed(void *data, struct wl_data_source *wl_data_source)
{
}

static void data_source_handle_dnd_finished(void *data, struct wl_data_source *wl_data_source)
{
}

static void data_source_handle_action(void *data, struct wl_data_source *wl_data_source,
                                      uint32_t dnd_action)
{
}

static const struct wl_data_source_listener data_source_listener = {
    data_source_handle_target,
    data_source_handle_send,
    data_source_handle_cancelled,
    data_source_handle_dnd_drop_performed,
    data_source_handle_dnd_finished,
    data_source_handle_action,
};

static void enable_ime(struct vo_wayland_text_input *ti)
{
    if (!ti->has_focus)
        return;

    zwp_text_input_v3_enable(ti->text_input);
    zwp_text_input_v3_set_content_type(
        ti->text_input,
        ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
        ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
    zwp_text_input_v3_set_cursor_rectangle(ti->text_input, 0, 0, 0, 0);
    zwp_text_input_v3_commit(ti->text_input);
    ti->serial++;
}

static void disable_ime(struct vo_wayland_text_input *ti)
{
    zwp_text_input_v3_disable(ti->text_input);
    zwp_text_input_v3_commit(ti->text_input);
    ti->serial++;
}

static void text_input_enter(void *data, struct zwp_text_input_v3 *zwp_text_input_v3,
                             struct wl_surface *surface)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_text_input *ti = s->text_input;
    struct vo_wayland_state *wl = s->wl;

    ti->has_focus = true;

    if (!wl->opts->input_ime)
        return;

    enable_ime(ti);
}

static void text_input_leave(void *data, struct zwp_text_input_v3 *zwp_text_input_v3,
                             struct wl_surface *surface)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_text_input *ti = s->text_input;

    ti->has_focus = false;
    disable_ime(ti);
}

static void text_input_preedit_string(void *data, struct zwp_text_input_v3 *zwp_text_input_v3,
                                      const char *text, int32_t cursor_begin, int32_t cursor_end)
{
}

static void text_input_commit_string(void *data, struct zwp_text_input_v3 *zwp_text_input_v3,
                                     const char *text)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_text_input *ti = s->text_input;

    talloc_replace(ti, ti->commit_string, text);
}

static void text_input_delete_surrounding_text(void *data,
                                               struct zwp_text_input_v3 *zwp_text_input_v3,
                                               uint32_t before_length, uint32_t after_length)
{
}

static void text_input_done(void *data, struct zwp_text_input_v3 *zwp_text_input_v3,
                            uint32_t serial)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_text_input *ti = s->text_input;
    struct vo_wayland_state *wl = s->wl;

    if (ti->serial == serial && ti->commit_string)
        mp_input_put_key_utf8(wl->vo->input_ctx, 0, bstr0(ti->commit_string));

    if (ti->commit_string)
        TA_FREEP(&ti->commit_string);
}

static const struct zwp_text_input_v3_listener text_input_listener = {
    text_input_enter,
    text_input_leave,
    text_input_preedit_string,
    text_input_commit_string,
    text_input_delete_surrounding_text,
    text_input_done,
};

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                                   int32_t x, int32_t y, int32_t phys_width,
                                   int32_t phys_height, int32_t subpixel,
                                   const char *make, const char *model,
                                   int32_t transform)
{
    struct vo_wayland_output *output = data;
    output->make = talloc_strdup(output->wl, make);
    output->model = talloc_strdup(output->wl, model);
    output->geometry.x0 = x;
    output->geometry.y0 = y;
    output->phys_width = phys_width;
    output->phys_height = phys_height;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
                               uint32_t flags, int32_t width,
                               int32_t height, int32_t refresh)
{
    struct vo_wayland_output *output = data;

    /* Only save current mode */
    if (!(flags & WL_OUTPUT_MODE_CURRENT))
        return;

    output->geometry.x1 = width;
    output->geometry.y1 = height;
    output->flags = flags;
    output->refresh_rate = (double)refresh * 0.001;
}

static void output_handle_done(void *data, struct wl_output *wl_output)
{
    struct vo_wayland_output *o = data;
    struct vo_wayland_state *wl = o->wl;

    o->geometry.x1 += o->geometry.x0;
    o->geometry.y1 += o->geometry.y0;

    MP_VERBOSE(o->wl, "Registered output %s %s (%s) (0x%x):\n"
               "\tx: %dpx, y: %dpx\n"
               "\tw: %dpx (%dmm), h: %dpx (%dmm)\n"
               "\tscale: %f\n"
               "\tHz: %f\n", o->make, o->model, o->name, o->id, o->geometry.x0,
               o->geometry.y0, mp_rect_w(o->geometry), o->phys_width,
               mp_rect_h(o->geometry), o->phys_height,
               o->scale / WAYLAND_SCALE_FACTOR, o->refresh_rate);

    /* If we satisfy this conditional, something about the current
     * output must have changed (resolution, scale, etc). All window
     * geometry and scaling should be recalculated. */
    if (wl->current_output && wl->current_output->output == wl_output) {
        set_surface_scaling(wl);
        set_geometry(wl, false);
        wl->pending_vo_events |= VO_EVENT_RESIZE;
    }

    wl->pending_vo_events |= VO_EVENT_WIN_STATE;
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                                int32_t factor)
{
    struct vo_wayland_output *output = data;
    if (!factor) {
        MP_ERR(output->wl, "Invalid output scale given by the compositor!\n");
        return;
    }
    output->scale = factor * WAYLAND_SCALE_FACTOR;
}

static void output_handle_name(void *data, struct wl_output *wl_output,
                               const char *name)
{
    struct vo_wayland_output *output = data;
    output->name = talloc_strdup(output->wl, name);
}

static void output_handle_description(void *data, struct wl_output *wl_output,
                                      const char *description)
{
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
    output_handle_done,
    output_handle_scale,
    output_handle_name,
    output_handle_description,
};

static void surface_handle_enter(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *output)
{
    struct vo_wayland_state *wl = data;
    if (!wl->current_output)
        return;

    struct mp_rect old_output_geometry = wl->current_output->geometry;
    struct mp_rect old_geometry = wl->geometry;
    wl->current_output = NULL;

    int outputs = 0;
    struct vo_wayland_output *o;
    wl_list_for_each(o, &wl->output_list, link) {
        if (o->output == output) {
            wl->current_output = o;
            wl->current_output->has_surface = true;
        }
        if (o->has_surface)
            ++outputs;
    }

    if (outputs == 1)
        update_output_geometry(wl, old_geometry, old_output_geometry);

    MP_VERBOSE(wl, "Surface entered output %s %s (0x%x), scale = %f, refresh rate = %f Hz\n",
               wl->current_output->make, wl->current_output->model,
               wl->current_output->id, wl->scaling_factor, wl->current_output->refresh_rate);

    wl->pending_vo_events |= VO_EVENT_WIN_STATE;
}

static void surface_handle_leave(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *output)
{
    struct vo_wayland_state *wl = data;
    if (!wl->current_output)
        return;

    struct mp_rect old_output_geometry = wl->current_output->geometry;
    struct mp_rect old_geometry = wl->geometry;

    int outputs = 0;
    struct vo_wayland_output *o;
    wl_list_for_each(o, &wl->output_list, link) {
        if (o->output == output)
            o->has_surface = false;
        if (o->has_surface)
            ++outputs;
        if (o->output != output && o->has_surface)
            wl->current_output = o;
    }

    if (outputs == 1)
        update_output_geometry(wl, old_geometry, old_output_geometry);

    wl->pending_vo_events |= VO_EVENT_WIN_STATE;
}

#ifdef HAVE_WAYLAND_1_22
static void surface_handle_preferred_buffer_scale(void *data,
                                                  struct wl_surface *wl_surface,
                                                  int32_t scale)
{
    struct vo_wayland_state *wl = data;

    if (wl->fractional_scale_manager ||
        (wl->scaling == scale * WAYLAND_SCALE_FACTOR &&
         wl->current_output && wl->current_output->has_surface))
        return;

    wl->pending_scaling = scale * WAYLAND_SCALE_FACTOR;
    wl->scale_configured = true;
    MP_VERBOSE(wl, "Obtained preferred scale, %f, from the compositor.\n",
               wl->scaling / WAYLAND_SCALE_FACTOR);
    wl->pending_vo_events |= VO_EVENT_DPI;
    wl->need_rescale = true;

    // Update scaling now.
    if (single_output_spanned(wl))
        update_output_scaling(wl);

    if (!wl->current_output) {
        wl->scaling = wl->pending_scaling;
        wl->scaling_factor = scale;
    }
}

static void surface_handle_preferred_buffer_transform(void *data,
                                                      struct wl_surface *wl_surface,
                                                      uint32_t transform)
{
}
#endif

static const struct wl_surface_listener surface_listener = {
    surface_handle_enter,
    surface_handle_leave,
#ifdef HAVE_WAYLAND_1_22
    surface_handle_preferred_buffer_scale,
    surface_handle_preferred_buffer_transform,
#endif
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping,
};

static void handle_surface_config(void *data, struct xdg_surface *surface,
                                  uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    handle_surface_config,
};

static void handle_toplevel_config(void *data, struct xdg_toplevel *toplevel,
                                   int32_t width, int32_t height, struct wl_array *states)
{
    struct vo_wayland_state *wl = data;
    struct mp_vo_opts *opts = wl->opts;
    struct mp_rect old_geometry = wl->geometry;

    if (width < 0 || height < 0) {
        MP_WARN(wl, "Compositor sent negative width/height values. Treating them as zero.\n");
        width = height = 0;
    }

    if (!wl->geometry_configured) {
        /* Save initial window size if the compositor gives us a hint here. */
        bool autofit_or_geometry = opts->geometry.wh_valid || opts->autofit.wh_valid ||
                                   opts->autofit_larger.wh_valid || opts->autofit_smaller.wh_valid;
        if (width && height && !autofit_or_geometry) {
            wl->initial_size_hint = true;
            wl->window_size = (struct mp_rect){0, 0, width, height};
            wl->geometry = wl->window_size;
        }
        return;
    }

    bool is_maximized = false;
    bool is_fullscreen = false;
    bool is_activated = false;
    bool is_resizing = false;
    bool is_suspended = false;
    bool is_tiled = false;
    enum xdg_toplevel_state *state;
    wl_array_for_each(state, states) {
        switch (*state) {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            is_fullscreen = true;
            break;
        case XDG_TOPLEVEL_STATE_RESIZING:
            is_resizing = true;
            break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
            is_activated = true;
            /*
             * If we get an ACTIVATED state, we know it cannot be
             * minimized, but it may not have been minimized
             * previously, so we can't detect the exact state.
             */
            opts->window_minimized = false;
            m_config_cache_write_opt(wl->opts_cache,
                                     &opts->window_minimized);
            break;
        case XDG_TOPLEVEL_STATE_TILED_TOP:
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            is_tiled = true;
            break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            is_maximized = true;
            break;
        case XDG_TOPLEVEL_STATE_SUSPENDED:
            is_suspended = true;
            break;
        }
    }

    if (wl->hidden != is_suspended)
        wl->hidden = is_suspended;

    if (wl->resizing != is_resizing) {
        wl->resizing = is_resizing;
        wl->resizing_constraint = 0;
    }

    if (opts->fullscreen != is_fullscreen) {
        wl->state_change = wl->reconfigured;
        opts->fullscreen = is_fullscreen;
        m_config_cache_write_opt(wl->opts_cache, &opts->fullscreen);
    }

    if (opts->window_maximized != is_maximized) {
        wl->state_change = wl->reconfigured;
        opts->window_maximized = is_maximized;
        m_config_cache_write_opt(wl->opts_cache, &opts->window_maximized);
    }

    if (!is_tiled && wl->tiled)
        wl->state_change = wl->reconfigured;

    wl->tiled = is_tiled;

    wl->locked_size = is_fullscreen || is_maximized || is_tiled;
    wl->reconfigured = false;

    if (wl->requested_decoration)
        request_decoration_mode(wl, wl->requested_decoration);

    if (wl->activated != is_activated) {
        wl->activated = is_activated;
        guess_focus(wl);
        /* Just force a redraw to be on the safe side. */
        if (wl->activated) {
            wl->hidden = false;
            wl->pending_vo_events |= VO_EVENT_EXPOSE;
        }
    }

    if (wl->state_change) {
        if (!wl->locked_size) {
            wl->geometry = wl->window_size;
            wl->state_change = false;
            goto resize;
        }
    }

    /* Reuse old size if either of these are 0. */
    if (width == 0 || height == 0) {
        if (!wl->locked_size) {
            wl->geometry = wl->window_size;
        }
        goto resize;
    }

    if (!wl->locked_size) {
        apply_keepaspect(wl, &width, &height);
        wl->window_size.x0 = 0;
        wl->window_size.y0 = 0;
        wl->window_size.x1 = handle_round(wl->scaling, width);
        wl->window_size.y1 = handle_round(wl->scaling, height);
    }
    wl->geometry.x0 = 0;
    wl->geometry.y0 = 0;
    wl->geometry.x1 = handle_round(wl->scaling, width);
    wl->geometry.y1 = handle_round(wl->scaling, height);

    if (mp_rect_equals(&old_geometry, &wl->geometry))
        return;

resize:
    MP_VERBOSE(wl, "Resizing due to xdg from %ix%i to %ix%i\n",
               mp_rect_w(old_geometry), mp_rect_h(old_geometry),
               mp_rect_w(wl->geometry), mp_rect_h(wl->geometry));

    wl->pending_vo_events |= VO_EVENT_RESIZE;
    wl->toplevel_configured = true;
}

static void handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    struct vo_wayland_state *wl = data;
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_CLOSE_WIN);
}

static void handle_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel,
                                    int32_t width, int32_t height)
{
    struct vo_wayland_state *wl = data;
    wl->bounded_width = handle_round(wl->scaling, width);
    wl->bounded_height = handle_round(wl->scaling, height);
}

static void handle_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel,
                                   struct wl_array *capabilities)
{
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_toplevel_config,
    handle_toplevel_close,
    handle_configure_bounds,
    handle_wm_capabilities,
};

static void preferred_scale(void *data,
                            struct wp_fractional_scale_v1 *fractional_scale,
                            uint32_t scale)
{
    struct vo_wayland_state *wl = data;
    if (wl->scaling == scale && wl->current_output && wl->current_output->has_surface)
        return;

    wl->pending_scaling = scale;
    wl->scale_configured = true;
    MP_VERBOSE(wl, "Obtained preferred fractional scale, %f, from the compositor.\n",
               wl->pending_scaling / WAYLAND_SCALE_FACTOR);
    wl->need_rescale = true;

    // Update scaling now.
    if (single_output_spanned(wl))
        update_output_scaling(wl);

    if (!wl->current_output) {
        wl->scaling = wl->pending_scaling;
        wl->scaling_factor = wl->scaling / WAYLAND_SCALE_FACTOR;
    }
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    preferred_scale,
};

#if HAVE_WAYLAND_PROTOCOLS_1_41
static void supported_intent(void *data, struct wp_color_manager_v1 *color_manager,
                             uint32_t render_intent)
{
}

static void supported_feature(void *data, struct wp_color_manager_v1 *color_manager,
                              uint32_t feature)
{
    struct vo_wayland_state *wl = data;

    switch (feature) {
    case WP_COLOR_MANAGER_V1_FEATURE_ICC_V2_V4:
        MP_VERBOSE(wl, "Compositor supports ICC creator requests.\n");
        wl->supports_icc = true;
        break;
    case WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC:
        MP_VERBOSE(wl, "Compositor supports parametric image description creator.\n");
        wl->supports_parametric = true;
        break;
    case WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES:
        MP_VERBOSE(wl, "Compositor supports setting primaries.\n");
        wl->supports_primaries = true;
        break;
    case WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER:
        MP_VERBOSE(wl, "Compositor supports setting transfer functions.\n");
        wl->supports_tf_power = true;
        break;
    case WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES:
        MP_VERBOSE(wl, "Compositor supports setting luminances.\n");
        wl->supports_luminances = true;
        break;
    case WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES:
        MP_VERBOSE(wl, "Compositor supports setting mastering display primaries.\n");
        wl->supports_display_primaries = true;
        break;
    }
}

static enum pl_color_transfer map_tf(uint32_t tf)
{
    switch (tf) {
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886: return PL_COLOR_TRC_BT_1886;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB: return PL_COLOR_TRC_SRGB;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR: return PL_COLOR_TRC_LINEAR;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22: return PL_COLOR_TRC_GAMMA22;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28: return PL_COLOR_TRC_GAMMA28;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428: return PL_COLOR_TRC_ST428;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ: return PL_COLOR_TRC_PQ;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG: return PL_COLOR_TRC_HLG;
        default: return PL_COLOR_TRC_UNKNOWN;
    }
}

static void supported_tf_named(void *data, struct wp_color_manager_v1 *color_manager,
                               uint32_t tf)
{
    struct vo_wayland_state *wl = data;
    enum pl_color_transfer pl_tf = map_tf(tf);

    if (pl_tf == PL_COLOR_TRC_UNKNOWN)
        return;

    wl->transfer_map[pl_tf] = tf;
}

static enum pl_color_primaries map_primaries(uint32_t primaries)
{
    switch (primaries) {
        case WP_COLOR_MANAGER_V1_PRIMARIES_PAL: return PL_COLOR_PRIM_BT_601_525;
        case WP_COLOR_MANAGER_V1_PRIMARIES_NTSC: return PL_COLOR_PRIM_BT_601_625;
        case WP_COLOR_MANAGER_V1_PRIMARIES_SRGB: return PL_COLOR_PRIM_BT_709;
        case WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M: return PL_COLOR_PRIM_BT_470M;
        case WP_COLOR_MANAGER_V1_PRIMARIES_BT2020: return PL_COLOR_PRIM_BT_2020;
        case WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB: return PL_COLOR_PRIM_ADOBE;
        case WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3: return PL_COLOR_PRIM_DCI_P3;
        case WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3: return PL_COLOR_PRIM_DISPLAY_P3;
        case WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM: return PL_COLOR_PRIM_FILM_C;
        default: return PL_COLOR_PRIM_UNKNOWN;
    }
}

static void supported_primaries_named(void *data, struct wp_color_manager_v1 *color_manager,
                                      uint32_t primaries)
{
    struct vo_wayland_state *wl = data;
    enum pl_color_primaries pl_primaries = map_primaries(primaries);

    if (pl_primaries == PL_COLOR_PRIM_UNKNOWN)
        return;

    wl->primaries_map[pl_primaries] = primaries;
}

static void color_manager_done(void *data, struct wp_color_manager_v1 *color_manager)
{
}

static const struct wp_color_manager_v1_listener color_manager_listener = {
    supported_intent,
    supported_feature,
    supported_tf_named,
    supported_primaries_named,
    color_manager_done,
};

static void image_description_failed(void *data, struct wp_image_description_v1 *image_description,
                                     uint32_t cause, const char *msg)
{
    struct vo_wayland_state *wl = data;
    MP_VERBOSE(wl, "Image description failed: %d, %s\n", cause, msg);
    wp_image_description_v1_destroy(image_description);
}

static void image_description_ready(void *data, struct wp_image_description_v1 *image_description,
                                    uint32_t identity)
{
    struct vo_wayland_state *wl = data;
    wp_color_management_surface_v1_set_image_description(wl->color_surface, image_description, 0);
    MP_VERBOSE(wl, "Image description set on color surface.\n");
    wp_image_description_v1_destroy(image_description);
}

static const struct wp_image_description_v1_listener image_description_listener = {
    image_description_failed,
    image_description_ready,
};

static void info_done(void *data, struct wp_image_description_info_v1 *image_description_info)
{
    struct vo_wayland_preferred_description_info *wd = data;
    struct vo_wayland_state *wl = wd->wl;
    wp_image_description_info_v1_destroy(image_description_info);
    if (wd->is_parametric) {
        wl->preferred_csp = wd->csp;
    } else {
        if (wd->icc_file) {
            if (wl->icc_size) {
                munmap(wl->icc_file, wl->icc_size);
            }
            wl->icc_file = wd->icc_file;
            wl->icc_size = wd->icc_size;
            wl->pending_vo_events |= VO_EVENT_ICC_PROFILE_CHANGED;
        } else {
            MP_VERBOSE(wl, "No ICC profile retrieved from the compositor.\n");
        }
    }
    talloc_free(wd);
}

static void info_icc_file(void *data, struct wp_image_description_info_v1 *image_description_info,
                          int32_t icc, uint32_t icc_size)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (wd->is_parametric)
        return;

    void *icc_file = mmap(NULL, icc_size, PROT_READ, MAP_PRIVATE, icc, 0);
    close(icc);

    if (icc_file != MAP_FAILED) {
        wd->icc_file = icc_file;
        wd->icc_size = icc_size;
    }
}

static void info_primaries(void *data, struct wp_image_description_info_v1 *image_description_info,
                           int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y,
                           int32_t w_x, int32_t w_y)
{
}

static void info_primaries_named(void *data, struct wp_image_description_info_v1 *image_description_info,
                                 uint32_t primaries)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (!wd->is_parametric)
        return;
    wd->csp.primaries = map_primaries(primaries);
}

static void info_tf_power(void *data, struct wp_image_description_info_v1 *image_description_info,
                          uint32_t eexp)
{
}

static void info_tf_named(void *data, struct wp_image_description_info_v1 *image_description_info,
                          uint32_t tf)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (!wd->is_parametric)
        return;
    wd->csp.transfer = map_tf(tf);
}

static void info_luminances(void *data, struct wp_image_description_info_v1 *image_description_info,
                            uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum)
{
}

static void info_target_primaries(void *data, struct wp_image_description_info_v1 *image_description_info,
                                  int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y,
                                  int32_t w_x, int32_t w_y)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (!wd->is_parametric)
        return;
    wd->csp.hdr.prim.red.x = (float)r_x / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.red.y = (float)r_y / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.green.x = (float)g_x / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.green.y = (float)g_y / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.blue.x = (float)b_x / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.blue.y = (float)b_y / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.white.x = (float)w_x / WAYLAND_COLOR_FACTOR;
    wd->csp.hdr.prim.white.y = (float)w_y / WAYLAND_COLOR_FACTOR;
}

static void info_target_luminance(void *data, struct wp_image_description_info_v1 *image_description_info,
                                  uint32_t min_lum, uint32_t max_lum)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (!wd->is_parametric)
        return;
    wd->csp.hdr.min_luma = (float)min_lum / WAYLAND_MIN_LUM_FACTOR;
    wd->csp.hdr.max_luma = (float)max_lum;
}

static void info_target_max_cll(void *data, struct wp_image_description_info_v1 *image_description_info,
                                uint32_t max_cll)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (!wd->is_parametric)
        return;
    wd->csp.hdr.max_cll = (float)max_cll;
}

static void info_target_max_fall(void *data, struct wp_image_description_info_v1 *image_description_info,
                                 uint32_t max_fall)
{
    struct vo_wayland_preferred_description_info *wd = data;
    if (!wd->is_parametric)
        return;
    wd->csp.hdr.max_fall = (float)max_fall;
}

static const struct wp_image_description_info_v1_listener image_description_info_listener = {
    info_done,
    info_icc_file,
    info_primaries,
    info_primaries_named,
    info_tf_power,
    info_tf_named,
    info_luminances,
    info_target_primaries,
    info_target_luminance,
    info_target_max_cll,
    info_target_max_fall,
};

static void preferred_changed(void *data, struct wp_color_management_surface_feedback_v1 *color_surface_feedback,
                              uint32_t identity)
{
    struct vo_wayland_state *wl = data;
    if (wl->supports_icc)
        get_compositor_preferred_description(wl, false);
    if (wl->supports_parametric)
        get_compositor_preferred_description(wl, true);
}

static const struct wp_color_management_surface_feedback_v1_listener surface_feedback_listener = {
    preferred_changed,
};
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_44
static void supported_alpha_mode(void *data, struct wp_color_representation_manager_v1 *color_representation_manager,
                                 uint32_t alpha_mode)
{
    struct vo_wayland_state *wl = data;
    switch (alpha_mode) {
    case WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL:
#if PL_API_VER >= 344
        wl->alpha_map[PL_ALPHA_NONE] = alpha_mode;
#endif
        break;
    case WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_STRAIGHT:
        wl->alpha_map[PL_ALPHA_INDEPENDENT] = alpha_mode;
        break;
    }
}

static void supported_coefficients_and_ranges(void *data, struct wp_color_representation_manager_v1 *color_representation_manager,
                                              uint32_t coefficients, uint32_t range)
{
    struct vo_wayland_state *wl = data;
    int offset = range == WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL ? 0 : PL_COLOR_SYSTEM_COUNT;
    switch (coefficients) {
    case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709:
        wl->coefficients_map[PL_COLOR_SYSTEM_BT_709] = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709;
        wl->range_map[PL_COLOR_SYSTEM_BT_709 + offset] = range;
        break;
    case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601:
        wl->coefficients_map[PL_COLOR_SYSTEM_BT_601] = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601;
        wl->range_map[PL_COLOR_SYSTEM_BT_601 + offset] = range;
        break;
    case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_SMPTE240:
        wl->coefficients_map[PL_COLOR_SYSTEM_SMPTE_240M] = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_SMPTE240;
        wl->range_map[PL_COLOR_SYSTEM_SMPTE_240M + offset] = range;
        break;
    case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020:
        wl->coefficients_map[PL_COLOR_SYSTEM_BT_2020_NC] = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020;
        wl->range_map[PL_COLOR_SYSTEM_BT_2020_NC + offset] = range;
        break;
    case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020_CL:
        wl->coefficients_map[PL_COLOR_SYSTEM_BT_2020_C] = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020_CL;
        wl->range_map[PL_COLOR_SYSTEM_BT_2020_C + offset] = range;
        break;
    }
}

static int map_supported_chroma_location(enum pl_chroma_location chroma_location)
{
    switch (chroma_location) {
    case PL_CHROMA_LEFT:
        return WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_0;
    case PL_CHROMA_CENTER:
        return WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_1;
    case PL_CHROMA_TOP_LEFT:
        return WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_2;
    case PL_CHROMA_TOP_CENTER:
        return WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_3;
    case PL_CHROMA_BOTTOM_LEFT:
        return WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_4;
    case PL_CHROMA_BOTTOM_CENTER:
        return WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_5;
    default:
        return 0;
    }
}

static void color_representation_done(void *data, struct wp_color_representation_manager_v1 *color_representation_manager)
{
}

static const struct wp_color_representation_manager_v1_listener color_representation_listener = {
    supported_alpha_mode,
    supported_coefficients_and_ranges,
    color_representation_done,
};
#endif

static const char *zxdg_decoration_mode_to_str(const uint32_t mode)
{
    switch (mode) {
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
        return "server-side";
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
        return "client-side";
    default:
        return "<unknown>";
    }
}

static void configure_decorations(void *data,
                                  struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration,
                                  uint32_t mode)
{
    struct vo_wayland_state *wl = data;
    struct mp_vo_opts *opts = wl->opts;

    if (wl->requested_decoration && mode != wl->requested_decoration) {
        MP_DBG(wl,
               "Requested %s decorations but compositor responded with %s. "
               "It is likely that compositor wants us to stay in a given mode.\n",
               zxdg_decoration_mode_to_str(wl->requested_decoration),
               zxdg_decoration_mode_to_str(mode));
    }

    wl->requested_decoration = 0;

    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
        MP_VERBOSE(wl, "Enabling server decorations\n");
    } else {
        MP_VERBOSE(wl, "Disabling server decorations\n");
    }
    opts->border = mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    m_config_cache_write_opt(wl->opts_cache, &opts->border);
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
    configure_decorations,
};

static void presentation_set_clockid(void *data, struct wp_presentation *presentation,
                                     uint32_t clockid)
{
    struct vo_wayland_state *wl = data;

    if (clockid == CLOCK_MONOTONIC || clockid == CLOCK_MONOTONIC_RAW)
        wl->present_clock = true;
}

static const struct wp_presentation_listener presentation_listener = {
    presentation_set_clockid,
};

static void feedback_sync_output(void *data, struct wp_presentation_feedback *fback,
                               struct wl_output *output)
{
}

static void feedback_presented(void *data, struct wp_presentation_feedback *fback,
                              uint32_t tv_sec_hi, uint32_t tv_sec_lo,
                              uint32_t tv_nsec, uint32_t refresh_nsec,
                              uint32_t seq_hi, uint32_t seq_lo,
                              uint32_t flags)
{
    struct vo_wayland_feedback_pool *fback_pool = data;
    struct vo_wayland_state *wl = fback_pool->wl;

    bool current_zero_copy = flags & WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
    if (wl->last_zero_copy == -1 || wl->last_zero_copy != current_zero_copy) {
        MP_DBG(wl, "Presentation was done with %s.\n",
                 current_zero_copy ? "direct scanout" : "a copy");
        wl->last_zero_copy = current_zero_copy;
    }

    if (fback)
        remove_feedback(fback_pool, fback);

    wl->refresh_interval = (int64_t)refresh_nsec;

    // Very similar to oml_sync_control, in this case we assume that every
    // time the compositor receives feedback, a buffer swap has been already
    // been performed.
    //
    // Notes:
    //  - tv_sec_lo + tv_sec_hi is the equivalent of oml's ust
    //  - seq_lo + seq_hi is the equivalent of oml's msc
    //  - these values are updated every time the compositor receives feedback.

    int64_t sec = (uint64_t) tv_sec_lo + ((uint64_t) tv_sec_hi << 32);
    int64_t ust = MP_TIME_S_TO_NS(sec) + (uint64_t) tv_nsec;
    int64_t msc = (uint64_t) seq_lo + ((uint64_t) seq_hi << 32);
    present_sync_update_values(wl->present, ust, msc);
}

static void feedback_discarded(void *data, struct wp_presentation_feedback *fback)
{
    struct vo_wayland_feedback_pool *fback_pool = data;
    struct vo_wayland_state *wl = fback_pool->wl;
    wl->last_zero_copy = -1;
    if (fback)
        remove_feedback(fback_pool, fback);
}

static const struct wp_presentation_feedback_listener feedback_listener = {
    feedback_sync_output,
    feedback_presented,
    feedback_discarded,
};

static const struct wl_callback_listener frame_listener;

static void frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    struct vo_wayland_state *wl = data;

    if (callback)
        wl_callback_destroy(callback);

    wl->frame_callback = wl_surface_frame(wl->callback_surface);
    wl_callback_add_listener(wl->frame_callback, &frame_listener, wl);

    wl->use_present = wl->present_clock && wl->opts->wl_present;
    if (wl->use_present) {
        struct wp_presentation_feedback *fback = wp_presentation_feedback(wl->presentation, wl->callback_surface);
        add_feedback(wl->fback_pool, fback);
        wp_presentation_feedback_add_listener(fback, &feedback_listener, wl->fback_pool);
    }

    wl->frame_wait = false;
    wl->hidden = false;
}

static const struct wl_callback_listener frame_listener = {
    frame_callback,
};

static void done(void *data,
                 struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
}

static void format_table(void *data,
                         struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                         int32_t fd,
                         uint32_t size)
{
    struct vo_wayland_state *wl = data;

    if (wl->compositor_format_size) {
        munmap(wl->compositor_format_map, wl->compositor_format_size);
        wl->compositor_format_map = NULL;
        wl->compositor_format_size = 0;
    }

    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (map != MAP_FAILED) {
        wl->compositor_format_map = map;
        wl->compositor_format_size = size;
    }
}

static void main_device(void *data,
                        struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                        struct wl_array *device)
{
    struct vo_wayland_state *wl = data;

    // Remove any old devices and tranches if we get this again.
    struct vo_wayland_tranche *tranche, *tranche_tmp;
    wl_list_for_each_safe(tranche, tranche_tmp, &wl->tranche_list, link) {
        wl_list_remove(&tranche->link);
        talloc_free(tranche);
    }
}

static void tranche_done(void *data,
                         struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    struct vo_wayland_state *wl = data;
    wl->current_tranche = NULL;
}

static void tranche_target_device(void *data,
                                  struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                  struct wl_array *device)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_tranche *tranche = talloc_zero(wl, struct vo_wayland_tranche);

    dev_t *id;
    wl_array_for_each(id, device) {
        memcpy(&tranche->device_id, id, sizeof(dev_t));
        break;
    }
    static_assert(sizeof(tranche->device_id) == sizeof(dev_t), "");

    wl->current_tranche = tranche;
    wl_list_insert(&wl->tranche_list, &tranche->link);
}

static void tranche_formats(void *data,
                            struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                            struct wl_array *indices)
{
    struct vo_wayland_state *wl = data;

    // Should never happen.
    if (!wl->compositor_format_map) {
        MP_WARN(wl, "Compositor did not send a format and modifier table!\n");
        return;
    }

    struct vo_wayland_tranche *tranche = wl->current_tranche;
    if (!tranche)
        return;

    const struct compositor_format *formats = wl->compositor_format_map;
    uint16_t *index;
    MP_DBG(wl, "Querying available drm format and modifier pairs from tranche on device '%lu'\n",
           tranche->device_id);
    wl_array_for_each(index, indices) {
        MP_TARRAY_APPEND(tranche, tranche->compositor_formats, tranche->num_compositor_formats,
                         (struct drm_format) {
                            formats[*index].format,
                            formats[*index].modifier,
                        });
        MP_DBG(wl, "Compositor supports drm format: '%s(%016" PRIx64 ")'\n",
               mp_tag_str(formats[*index].format), formats[*index].modifier);
    }
}

static void tranche_flags(void *data,
                          struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                          uint32_t flags)
{
    struct vo_wayland_state *wl = data;
    if (flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT)
        MP_DBG(wl, "Tranche has direct scanout.\n");
}

static const struct zwp_linux_dmabuf_feedback_v1_listener dmabuf_feedback_listener = {
    done,
    format_table,
    main_device,
    tranche_done,
    tranche_target_device,
    tranche_formats,
    tranche_flags,
};

static void registry_handle_add(void *data, struct wl_registry *reg, uint32_t id,
                                const char *interface, uint32_t ver)
{
    int found = 1;
    struct vo_wayland_state *wl = data;

    if (!strcmp(interface, wl_compositor_interface.name) && (ver >= 4) && found++) {
#ifdef HAVE_WAYLAND_1_22
        ver = MPMIN(ver, 6); /* Cap at 6 in case new events are added later. */
#else
        ver = 4;
#endif
        wl->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, ver);
        wl->surface = wl_compositor_create_surface(wl->compositor);
        wl->video_surface = wl_compositor_create_surface(wl->compositor);
        wl->osd_surface = wl_compositor_create_surface(wl->compositor);
        wl->callback_surface = !strcmp(wl->vo->driver->name, "dmabuf-wayland") ?
                               wl->video_surface : wl->surface;

        /* never accept input events on anything besides the main surface */
        struct wl_region *region = wl_compositor_create_region(wl->compositor);
        wl_surface_set_input_region(wl->osd_surface, region);
        wl_surface_set_input_region(wl->video_surface, region);
        wl_region_destroy(region);

        wl->cursor_surface = wl_compositor_create_surface(wl->compositor);
        wl_surface_add_listener(wl->surface, &surface_listener, wl);
    }

    if (!strcmp(interface, wl_subcompositor_interface.name) && found++) {
        ver = 1;
        wl->subcompositor = wl_registry_bind(reg, id, &wl_subcompositor_interface, ver);
    }

    if (!strcmp (interface, zwp_linux_dmabuf_v1_interface.name) && (ver >= 4) && found++) {
        ver = 4;
        wl->dmabuf = wl_registry_bind(reg, id, &zwp_linux_dmabuf_v1_interface, ver);
        wl->dmabuf_feedback = zwp_linux_dmabuf_v1_get_default_feedback(wl->dmabuf);
        zwp_linux_dmabuf_feedback_v1_add_listener(wl->dmabuf_feedback, &dmabuf_feedback_listener, wl);
    }

    if (!strcmp (interface, wp_viewporter_interface.name) && found++) {
        ver = 1;
        wl->viewporter = wl_registry_bind (reg, id, &wp_viewporter_interface, ver);
    }

    if (!strcmp(interface, wl_data_device_manager_interface.name) && (ver >= 3) && found++) {
        ver = 3;
        wl->devman = wl_registry_bind(reg, id, &wl_data_device_manager_interface, ver);
    }

    if (!strcmp(interface, wl_output_interface.name) && (ver >= 2) && found++) {
        struct vo_wayland_output *output = talloc_zero(wl, struct vo_wayland_output);

        output->wl     = wl;
        output->id     = id;
        output->scale  = 1;
        output->name   = "";

        ver = MPMIN(ver, 4); /* Cap at 4 in case new events are added later. */
        output->output = wl_registry_bind(reg, id, &wl_output_interface, ver);
        wl_output_add_listener(output->output, &output_listener, output);
        wl_list_insert(&wl->output_list, &output->link);
    }

    if (!strcmp(interface, wl_seat_interface.name) && found++) {
        if (ver < 5)
            MP_WARN(wl, "Scrolling won't work because the compositor doesn't "
                        "support version 5 of wl_seat protocol!\n");
        ver = MPMIN(ver, 8); /* Cap at 8 in case new events are added later. */
        struct vo_wayland_seat *seat = talloc_zero(wl, struct vo_wayland_seat);
        seat->wl   = wl;
        seat->id   = id;
        seat->pending_offer = talloc_zero(seat, struct vo_wayland_data_offer);
        seat->dnd_offer = talloc_zero(seat, struct vo_wayland_data_offer);
        seat->selection_offer = talloc_zero(seat, struct vo_wayland_data_offer);
        seat->pending_offer->fd = seat->dnd_offer->fd = seat->selection_offer->fd = -1;
        wl_list_init(&seat->tablet_list);
        wl_list_init(&seat->tablet_tool_list);
        wl_list_init(&seat->tablet_pad_list);
        seat->seat = wl_registry_bind(reg, id, &wl_seat_interface, ver);
        wl_seat_add_listener(seat->seat, &seat_listener, seat);
        wl_list_insert(&wl->seat_list, &seat->link);

        if (wl->devman)
            seat_create_data_device(seat);

        if (wl->text_input_manager)
            seat_create_text_input(seat);

        if (wl->wp_tablet_manager)
            seat_create_tablet_seat(wl, seat);
    }

    if (!strcmp(interface, wl_shm_interface.name) && found++) {
        ver = 1;
        wl->shm = wl_registry_bind(reg, id, &wl_shm_interface, ver);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_44
    if (!strcmp(interface, wp_color_representation_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->color_representation_manager = wl_registry_bind(reg, id, &wp_color_representation_manager_v1_interface, ver);
        wp_color_representation_manager_v1_add_listener(wl->color_representation_manager, &color_representation_listener, wl);
    }
#endif

    if (!strcmp(interface, wp_content_type_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->content_type_manager = wl_registry_bind(reg, id, &wp_content_type_manager_v1_interface, ver);
    }

    if (!strcmp(interface, wp_single_pixel_buffer_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->single_pixel_manager = wl_registry_bind(reg, id, &wp_single_pixel_buffer_manager_v1_interface, ver);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_38
    if (!strcmp(interface, wp_fifo_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->has_fifo = true;
    }
#endif

    if (!strcmp(interface, wp_fractional_scale_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->fractional_scale_manager = wl_registry_bind(reg, id, &wp_fractional_scale_manager_v1_interface, ver);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (!strcmp(interface, wp_cursor_shape_manager_v1_interface.name) && found++) {
        ver = MPMIN(ver, 2);
        wl->cursor_shape_manager = wl_registry_bind(reg, id, &wp_cursor_shape_manager_v1_interface, ver);
    }
#endif

    if (!strcmp(interface, wp_presentation_interface.name) && found++) {
        ver = MPMIN(ver, 2);
        wl->present_v2 = ver == 2;
        wl->presentation = wl_registry_bind(reg, id, &wp_presentation_interface, ver);
        wp_presentation_add_listener(wl->presentation, &presentation_listener, wl);
    }

    if (!strcmp(interface, xdg_wm_base_interface.name) && found++) {
        ver = MPMIN(ver, 6); /* Cap at 6 in case new events are added later. */
        wl->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, ver);
        xdg_wm_base_add_listener(wl->wm_base, &xdg_wm_base_listener, wl);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_41
    if (!strcmp(interface, wp_color_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->color_manager = wl_registry_bind(reg, id, &wp_color_manager_v1_interface, ver);
        wp_color_manager_v1_add_listener(wl->color_manager, &color_manager_listener, wl);
    }
#endif

    if (!strcmp(interface, xdg_activation_v1_interface.name) && found++) {
        ver = 1;
        wl->xdg_activation = wl_registry_bind(reg, id, &xdg_activation_v1_interface, ver);
    }

    if (!strcmp(interface, zxdg_decoration_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->xdg_decoration_manager = wl_registry_bind(reg, id, &zxdg_decoration_manager_v1_interface, ver);
    }

    if (!strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) && found++) {
        ver = 1;
        wl->idle_inhibit_manager = wl_registry_bind(reg, id, &zwp_idle_inhibit_manager_v1_interface, ver);
    }

    if (!strcmp(interface, zwp_text_input_manager_v3_interface.name) && found++) {
        ver = 1;
        wl->text_input_manager = wl_registry_bind(reg, id, &zwp_text_input_manager_v3_interface, ver);
    }

    if (!strcmp(interface, zwp_tablet_manager_v2_interface.name) && found++) {
        ver = 1;
        wl->wp_tablet_manager = wl_registry_bind(reg, id, &zwp_tablet_manager_v2_interface, ver);
    }

    if (found > 1)
        MP_VERBOSE(wl, "Registered interface %s at version %d\n", interface, ver);
}

static void registry_handle_remove(void *data, struct wl_registry *reg, uint32_t id)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_output *output, *output_tmp;
    wl_list_for_each_safe(output, output_tmp, &wl->output_list, link) {
        if (output->id == id) {
            remove_output(output);
            return;
        }
    }

    struct vo_wayland_seat *seat, *seat_tmp;
    wl_list_for_each_safe(seat, seat_tmp, &wl->seat_list, link) {
        if (seat->id == id) {
            remove_seat(seat);
            return;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_add,
    registry_handle_remove,
};

/* Static functions */
static void apply_keepaspect(struct vo_wayland_state *wl, int *width, int *height)
{
    if (!wl->opts->keepaspect)
        return;

    int phys_width = handle_round(wl->scaling, *width);
    int phys_height = handle_round(wl->scaling, *height);

    // Ensure that the size actually changes before we start trying to actually
    // calculate anything so the wrong constraint for the rezie isn't chosen.
    if (wl->resizing && !wl->resizing_constraint &&
        phys_width == mp_rect_w(wl->geometry) && phys_height == mp_rect_h(wl->geometry))
        return;

    // We are doing a continuous resize (e.g. dragging with mouse), constrain the
    // aspect ratio against the height if the change is only in the height
    // coordinate.
    if (wl->resizing && !wl->resizing_constraint && phys_width == mp_rect_w(wl->geometry) &&
        phys_height != mp_rect_h(wl->geometry)) {
        wl->resizing_constraint = MP_HEIGHT_CONSTRAINT;
    } else if (!wl->resizing_constraint) {
        wl->resizing_constraint = MP_WIDTH_CONSTRAINT;
    }

    if (wl->resizing_constraint == MP_HEIGHT_CONSTRAINT) {
        MPSWAP(int, *width, *height);
        MPSWAP(int, wl->reduced_width, wl->reduced_height);
    }

    double scale_factor = (double)*width / wl->reduced_width;
    *width = ceil(wl->reduced_width * scale_factor);
    if (wl->opts->keepaspect_window)
        *height = ceil(wl->reduced_height * scale_factor);

    if (wl->resizing_constraint == MP_HEIGHT_CONSTRAINT) {
        MPSWAP(int, *width, *height);
        MPSWAP(int, wl->reduced_width, wl->reduced_height);
    }
}

static void destroy_offer(struct vo_wayland_data_offer *o)
{
    TA_FREEP(&o->mime_type);
    if (o->fd != -1)
        close(o->fd);
    if (o->offer)
        wl_data_offer_destroy(o->offer);
    *o = (struct vo_wayland_data_offer){.fd = -1, .action = -1};
}

static void check_fd(struct vo_wayland_state *wl, struct vo_wayland_data_offer *o, bool is_dnd)
{
    if (o->fd == -1)
        return;

    struct pollfd fdp = { .fd = o->fd, .events = POLLIN };
    if (poll(&fdp, 1, 0) <= 0)
        return;

    if (fdp.revents & POLLIN) {
        ssize_t data_read = 0;
        const size_t chunk_size = 256;
        bstr content = {
            .start = talloc_zero_size(wl, chunk_size),
        };

        while (1) {
            data_read = read(o->fd, content.start + content.len, chunk_size);
            if (data_read == -1 && errno == EINTR)
                continue;
            else if (data_read <= 0)
                break;
            content.len += data_read;
            content.start = talloc_realloc_size(wl, content.start, content.len + chunk_size);
            memset(content.start + content.len, 0, chunk_size);
        }

        if (data_read == -1) {
            MP_VERBOSE(wl, "data offer aborted (read error: %s)\n", mp_strerror(errno));
        } else {
            MP_VERBOSE(wl, "Read %zu bytes from the data offer fd\n", content.len);

            if (is_dnd) {
                if (o->offer)
                    wl_data_offer_finish(o->offer);

                if (o->action >= 0) {
                    mp_event_drop_mime_data(wl->vo->input_ctx, o->mime_type,
                                            content, o->action);
                } else {
                    MP_WARN(wl, "Data offer did not have a valid action!\n");
                }
            } else {
                // Update clipboard text content
                talloc_free(wl->selection_text.start);
                wl->selection_text = content;
                content = (bstr){0};
                mp_cmd_t *cmd = mp_input_parse_cmd(
                    wl->vo->input_ctx, bstr0("notify-property clipboard"), "<internal>"
                );
                mp_input_queue_cmd(wl->vo->input_ctx, cmd);
            }
        }

        talloc_free(content.start);
        destroy_offer(o);
    }

    if (fdp.revents & (POLLIN | POLLERR | POLLHUP)) {
        if (o->action >= 0)
            MP_VERBOSE(wl, "data offer aborted (hang up or error)\n");
        destroy_offer(o);
    }
}

static int check_for_resize(struct vo_wayland_state *wl, int edge_pixels,
                            enum xdg_toplevel_resize_edge *edges)
{
    if (wl->opts->fullscreen || wl->opts->window_maximized)
        return 0;

    int pos[2] = { wl->mouse_x, wl->mouse_y };
    *edges = 0;

    if (pos[0] < edge_pixels)
        *edges |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    if (pos[0] > (mp_rect_w(wl->geometry) - edge_pixels))
        *edges |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    if (pos[1] < edge_pixels)
        *edges |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    if (pos[1] > (mp_rect_h(wl->geometry) - edge_pixels))
        *edges |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;

    return *edges;
}

static bool create_input(struct vo_wayland_state *wl)
{
    wl->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (!wl->xkb_context) {
        MP_ERR(wl, "failed to initialize input: check xkbcommon\n");
        return 1;
    }

    return 0;
}

static void xdg_activate(struct vo_wayland_state *wl)
{
    const char *token = getenv("XDG_ACTIVATION_TOKEN");
    if (token) {
        MP_VERBOSE(wl, "Activating window with token: '%s'\n", token);
        xdg_activation_v1_activate(wl->xdg_activation, token, wl->surface);
        unsetenv("XDG_ACTIVATION_TOKEN");
    }
}

static int create_viewports(struct vo_wayland_state *wl)
{
    wl->viewport = wp_viewporter_get_viewport(wl->viewporter, wl->surface);
    wl->cursor_viewport = wp_viewporter_get_viewport(wl->viewporter, wl->cursor_surface);
    wl->osd_viewport = wp_viewporter_get_viewport(wl->viewporter, wl->osd_surface);
    wl->video_viewport = wp_viewporter_get_viewport(wl->viewporter, wl->video_surface);

    if (!wl->viewport || !wl->osd_viewport || !wl->video_viewport) {
        MP_ERR(wl, "failed to create viewport interfaces!\n");
        return 1;
    }
    return 0;
}

static int create_xdg_surface(struct vo_wayland_state *wl)
{
    wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->wm_base, wl->surface);
    xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

    wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
    xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

    if (!wl->xdg_surface || !wl->xdg_toplevel) {
        MP_ERR(wl, "failed to create xdg_surface and xdg_toplevel!\n");
        return 1;
    }
    return 0;
}

static void add_feedback(struct vo_wayland_feedback_pool *fback_pool,
                         struct wp_presentation_feedback *fback)
{
    for (int i = 0; i < fback_pool->len; ++i) {
        if (!fback_pool->fback[i]) {
            fback_pool->fback[i] = fback;
            break;
        } else if (i == fback_pool->len - 1) {
            // Shouldn't happen in practice.
            wp_presentation_feedback_destroy(fback_pool->fback[i]);
            fback_pool->fback[i] = fback;
        }
    }
}

static void do_minimize(struct vo_wayland_state *wl)
{
    if (wl->opts->window_minimized)
        xdg_toplevel_set_minimized(wl->xdg_toplevel);
}

static char **get_displays_spanned(struct vo_wayland_state *wl)
{
    char **names = NULL;
    int displays_spanned = 0;
    struct vo_wayland_output *output;
    wl_list_for_each(output, &wl->output_list, link) {
        if (output->has_surface) {
            char *name = output->name ? output->name : output->model;
            MP_TARRAY_APPEND(NULL, names, displays_spanned,
                             talloc_strdup(NULL, name));
        }
    }
    MP_TARRAY_APPEND(NULL, names, displays_spanned, NULL);
    return names;
}

static int get_mods(struct vo_wayland_seat *s)
{
    static char* const mod_names[] = {
        XKB_MOD_NAME_SHIFT,
        XKB_MOD_NAME_CTRL,
        XKB_MOD_NAME_ALT,
        XKB_MOD_NAME_LOGO,
    };

    static const int mods[] = {
        MP_KEY_MODIFIER_SHIFT,
        MP_KEY_MODIFIER_CTRL,
        MP_KEY_MODIFIER_ALT,
        MP_KEY_MODIFIER_META,
    };

    int modifiers = 0;

    for (int n = 0; n < MP_ARRAY_SIZE(mods); n++) {
        xkb_mod_index_t index = xkb_keymap_mod_get_index(s->xkb_keymap, mod_names[n]);
        if (index != XKB_MOD_INVALID
            && xkb_state_mod_index_is_active(s->xkb_state, index,
                                             XKB_STATE_MODS_EFFECTIVE))
            modifiers |= mods[n];
    }
    return modifiers;
}

static void get_compositor_preferred_description(struct vo_wayland_state *wl, bool parametric)
{
#if HAVE_WAYLAND_PROTOCOLS_1_41
    struct vo_wayland_preferred_description_info *wd = talloc_zero(NULL, struct vo_wayland_preferred_description_info);
    wd->wl = wl;
    wd->is_parametric = parametric;

    struct wp_image_description_v1 *image_description;
    if (parametric) {
        image_description = wp_color_management_surface_feedback_v1_get_preferred_parametric(wl->color_surface_feedback);
    } else {
        image_description = wp_color_management_surface_feedback_v1_get_preferred(wl->color_surface_feedback);
    }
    struct wp_image_description_info_v1 *description_info =
        wp_image_description_v1_get_information(image_description);
    wp_image_description_info_v1_add_listener(description_info, &image_description_info_listener, wd);
    wp_image_description_v1_destroy(image_description);
#endif
}

static void get_shape_device(struct vo_wayland_state *wl, struct vo_wayland_seat *s)
{
#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (!s->cursor_shape_device && wl->cursor_shape_manager) {
        s->cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(wl->cursor_shape_manager,
                                                                        s->pointer);
    }
#endif
}

static int greatest_common_divisor(int a, int b)
{
    int rem = a % b;
    if (rem == 0)
        return b;
    return greatest_common_divisor(b, rem);
}

static void guess_focus(struct vo_wayland_state *wl)
{
    // We can't actually know if the window is focused or not in wayland,
    // so just guess it with some common sense. Obviously won't work if
    // the user has no keyboard. We flag has_keyboard_input if
    // at least one seat has it.
    bool has_keyboard_input = false;
    struct vo_wayland_seat *seat;
    wl_list_for_each(seat, &wl->seat_list, link) {
        if (seat->has_keyboard_input) {
            has_keyboard_input = true;
        }
    }

    if ((!wl->focused && wl->activated && has_keyboard_input) ||
        (wl->focused && !wl->activated))
    {
        wl->focused = !wl->focused;
        wl->pending_vo_events |= VO_EVENT_FOCUS;
    }
}

static struct vo_wayland_output *find_output(struct vo_wayland_state *wl)
{
    int index = 0;
    struct mp_vo_opts *opts = wl->opts;
    int screen_id = opts->fullscreen ? opts->fsscreen_id : opts->screen_id;
    char *screen_name = opts->fullscreen ? opts->fsscreen_name : opts->screen_name;
    struct vo_wayland_output *output = NULL;
    struct vo_wayland_output *fallback_output = NULL;
    wl_list_for_each(output, &wl->output_list, link) {
        if (index == 0)
            fallback_output = output;
        if (screen_id == -1 && !screen_name)
            return output;
        if (screen_id == -1 && screen_name && !strcmp(screen_name, output->name))
            return output;
        if (screen_id == -1 && screen_name && !strcmp(screen_name, output->model))
            return output;
        if (screen_id == index++)
            return output;
    }
    if (!fallback_output) {
        MP_ERR(wl, "No screens could be found!\n");
        return NULL;
    } else if (screen_id >= 0) {
        MP_WARN(wl, "Screen index %i not found/unavailable! Falling back to screen 0!\n", screen_id);
    } else if (screen_name && screen_name[0]) {
        MP_WARN(wl, "Screen name %s not found/unavailable! Falling back to screen 0!\n", screen_name);
    }
    return fallback_output;
}

static int lookupkey(int key)
{
    const char *passthrough_keys = " -+*/<>`~!@#$%^&()_{}:;\"\',.?\\|=[]";

    int mpkey = 0;
    if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') ||
        (key >= '0' && key <= '9') ||
        (key >  0   && key <  256 && strchr(passthrough_keys, key)))
        mpkey = key;

    if (!mpkey)
        mpkey = lookup_keymap_table(keymap, key);

    // XFree86 keysym range; typically contains obscure "extra" keys
    static_assert(MP_KEY_UNKNOWN_RESERVED_START + (0x1008FFFF - 0x10080000) <=
                  MP_KEY_UNKNOWN_RESERVED_LAST, "");
    if (!mpkey && key >= 0x10080001 && key <= 0x1008FFFF)
        mpkey = MP_KEY_UNKNOWN_RESERVED_START + (key - 0x10080000);

    return mpkey;
}

static void handle_key_input(struct vo_wayland_seat *s, uint32_t key,
                             uint32_t state, bool no_emit)
{
    struct vo_wayland_state *wl = s->wl;

    switch (state) {
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        state = MP_KEY_STATE_UP;
        break;
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        state = MP_KEY_STATE_DOWN;
        break;
    default:
        return;
    }

    if (no_emit)
        state = state | MP_KEY_STATE_SET_ONLY;

    s->keyboard_code = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(s->xkb_state, s->keyboard_code);
    int mpkey = lookupkey(sym);

    if (mpkey) {
        mp_input_put_key(wl->vo->input_ctx, mpkey | state | s->mpmod);
    } else {
        char str[128];
        if (xkb_keysym_to_utf8(sym, str, sizeof(str)) > 0) {
            mp_input_put_key_utf8(wl->vo->input_ctx, state | s->mpmod, bstr0(str));
        } else {
            // Assume a modifier was pressed and handle it in the mod event instead.
            // If a modifier is released before a regular key, also release that
            // key to not activate it again by accident.
            if (state & MP_KEY_STATE_UP) {
                s->mpkey = 0;
                mp_input_put_key(wl->vo->input_ctx, MP_INPUT_RELEASE_ALL);
            }
            return;
        }
    }
    if (state & MP_KEY_STATE_DOWN)
        s->mpkey = mpkey;
    if (mpkey && (state & MP_KEY_STATE_UP))
        s->mpkey = 0;
}

// Avoid possible floating point errors.
static int handle_round(int scale, int n)
{
    return (scale * n + WAYLAND_SCALE_FACTOR / 2) / WAYLAND_SCALE_FACTOR;
}

static bool hdr_metadata_valid(struct pl_hdr_metadata *hdr)
{
    // Always return a hard failure if this condition fails.
    if (hdr->min_luma >= hdr->max_luma)
        return false;

    // If max_cll or max_fall are invalid, set them to zero.
    if (hdr->max_cll && (hdr->max_cll <= hdr->min_luma || hdr->max_cll > hdr->max_luma))
        hdr->max_cll = 0;

    if (hdr->max_fall && (hdr->max_fall <= hdr->min_luma || hdr->max_fall > hdr->max_luma))
        hdr->max_fall = 0;

    if (hdr->max_cll && hdr->max_fall && hdr->max_fall > hdr->max_cll) {
        hdr->max_cll = 0;
        hdr->max_fall = 0;
    }

    return true;
}

static void request_decoration_mode(struct vo_wayland_state *wl, uint32_t mode)
{
    wl->requested_decoration = mode;
    zxdg_toplevel_decoration_v1_set_mode(wl->xdg_toplevel_decoration, mode);
}

static void rescale_geometry(struct vo_wayland_state *wl, double old_scale)
{
    if (!wl->opts->hidpi_window_scale && !wl->locked_size)
        return;

    double factor = old_scale / wl->scaling;
    wl->window_size.x1 /= factor;
    wl->window_size.y1 /= factor;
    wl->geometry.x1 /= factor;
    wl->geometry.y1 /= factor;
}

static void clean_feedback_pool(struct vo_wayland_feedback_pool *fback_pool)
{
    for (int i = 0; i < fback_pool->len; ++i) {
        if (fback_pool->fback[i]) {
            wp_presentation_feedback_destroy(fback_pool->fback[i]);
            fback_pool->fback[i] = NULL;
        }
    }
}

static void remove_feedback(struct vo_wayland_feedback_pool *fback_pool,
                            struct wp_presentation_feedback *fback)
{
    for (int i = 0; i < fback_pool->len; ++i) {
        if (fback_pool->fback[i] == fback) {
            wp_presentation_feedback_destroy(fback);
            fback_pool->fback[i] = NULL;
            break;
        }
    }
}

static void remove_output(struct vo_wayland_output *out)
{
    if (!out)
        return;

    MP_VERBOSE(out->wl, "Deregistering output %s %s (0x%x)\n", out->make,
               out->model, out->id);
    wl_list_remove(&out->link);
    wl_output_destroy(out->output);
    talloc_free(out->make);
    talloc_free(out->model);
    talloc_free(out);
}

static void remove_tablet(struct vo_wayland_tablet *tablet)
{
    struct vo_wayland_state *wl = tablet->wl;
    MP_VERBOSE(wl, "Removing tablet %p\n", tablet->tablet);

    wl_list_remove(&tablet->link);
    zwp_tablet_v2_destroy(tablet->tablet);
    talloc_free(tablet);
}

static void remove_tablet_tool(struct vo_wayland_tablet_tool *tablet_tool)
{
    struct vo_wayland_state *wl = tablet_tool->wl;
    struct vo_wayland_seat *seat = tablet_tool->seat;
    MP_VERBOSE(wl, "Removing tablet tool %p\n", tablet_tool->tablet_tool);

    wl_list_remove(&tablet_tool->link);
#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (seat->cursor_shape_device)
        wp_cursor_shape_device_v1_destroy(tablet_tool->cursor_shape_device);
#endif
    zwp_tablet_tool_v2_destroy(tablet_tool->tablet_tool);
    talloc_free(tablet_tool);
}

static void remove_tablet_pad(struct vo_wayland_tablet_pad *tablet_pad)
{
    struct vo_wayland_state *wl = tablet_pad->wl;
    MP_VERBOSE(wl, "Removing tablet pad %p\n", tablet_pad->tablet_pad);

    struct vo_wayland_tablet_pad_group *tablet_pad_group, *tablet_pad_group_tmp;
    wl_list_for_each_safe(tablet_pad_group, tablet_pad_group_tmp, &tablet_pad->tablet_pad_group_list, link) {
        struct vo_wayland_tablet_pad_ring *tablet_pad_ring, *tablet_pad_ring_tmp;
        wl_list_for_each_safe(tablet_pad_ring, tablet_pad_ring_tmp, &tablet_pad_group->tablet_pad_ring_list, link) {
            wl_list_remove(&tablet_pad_ring->link);
            zwp_tablet_pad_ring_v2_destroy(tablet_pad_ring->tablet_pad_ring);
            talloc_free(tablet_pad_ring);
        }
        struct vo_wayland_tablet_pad_strip *tablet_pad_strip, *tablet_pad_strip_tmp;
        wl_list_for_each_safe(tablet_pad_strip, tablet_pad_strip_tmp, &tablet_pad_group->tablet_pad_strip_list, link) {
            wl_list_remove(&tablet_pad_strip->link);
            zwp_tablet_pad_strip_v2_destroy(tablet_pad_strip->tablet_pad_strip);
            talloc_free(tablet_pad_strip);
        }

        wl_list_remove(&tablet_pad_group->link);
        zwp_tablet_pad_group_v2_destroy(tablet_pad_group->tablet_pad_group);
        talloc_free(tablet_pad_group);
    }

    wl_list_remove(&tablet_pad->link);
    zwp_tablet_pad_v2_destroy(tablet_pad->tablet_pad);
    talloc_free(tablet_pad);
}

static void remove_seat(struct vo_wayland_seat *seat)
{
    if (!seat)
        return;

    MP_VERBOSE(seat->wl, "Deregistering seat 0x%x\n", seat->id);
    wl_list_remove(&seat->link);
    if (seat == seat->wl->last_button_seat)
        seat->wl->last_button_seat = NULL;
    if (seat->keyboard)
        wl_keyboard_destroy(seat->keyboard);
    if (seat->pointer)
        wl_pointer_destroy(seat->pointer);
    if (seat->touch)
        wl_touch_destroy(seat->touch);
    if (seat->data_device)
        wl_data_device_destroy(seat->data_device);
    if (seat->text_input)
        zwp_text_input_v3_destroy(seat->text_input->text_input);
#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (seat->cursor_shape_device)
        wp_cursor_shape_device_v1_destroy(seat->cursor_shape_device);
#endif
    if (seat->xkb_keymap)
        xkb_keymap_unref(seat->xkb_keymap);
    if (seat->xkb_state)
        xkb_state_unref(seat->xkb_state);

    struct vo_wayland_tablet_pad *tablet_pad, *tablet_pad_tmp;
    wl_list_for_each_safe(tablet_pad, tablet_pad_tmp, &seat->tablet_pad_list, link)
        remove_tablet_pad(tablet_pad);

    struct vo_wayland_tablet_tool *tablet_tool, *tablet_tool_tmp;
    wl_list_for_each_safe(tablet_tool, tablet_tool_tmp, &seat->tablet_tool_list, link)
        remove_tablet_tool(tablet_tool);

    struct vo_wayland_tablet *tablet, *tablet_tmp;
    wl_list_for_each_safe(tablet, tablet_tmp, &seat->tablet_list, link)
        remove_tablet(tablet);

    if (seat->tablet_seat)
        zwp_tablet_seat_v2_destroy(seat->tablet_seat);

    destroy_offer(seat->pending_offer);
    destroy_offer(seat->dnd_offer);
    destroy_offer(seat->selection_offer);

    if (seat->data_source)
        wl_data_source_destroy(seat->data_source);

    wl_seat_destroy(seat->seat);
    talloc_free(seat);
}

static void seat_create_data_source(struct vo_wayland_seat *seat)
{
    seat->data_source = wl_data_device_manager_create_data_source(seat->wl->devman);
    wl_data_source_offer(seat->data_source, "text/plain;charset=utf-8");
    wl_data_source_offer(seat->data_source, "text/plain");
    wl_data_source_offer(seat->data_source, "text");
    wl_data_source_add_listener(seat->data_source, &data_source_listener, seat);
    wl_data_device_set_selection(seat->data_device, seat->data_source, seat->last_serial);
}

static void seat_create_data_device(struct vo_wayland_seat *seat)
{
    seat->data_device = wl_data_device_manager_get_data_device(seat->wl->devman, seat->seat);
    wl_data_device_add_listener(seat->data_device, &data_device_listener, seat);
}

static void seat_create_tablet_seat(struct vo_wayland_state *wl, struct vo_wayland_seat *seat)
{
    seat->tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(wl->wp_tablet_manager, seat->seat);
    zwp_tablet_seat_v2_add_listener(seat->tablet_seat, &tablet_seat_listener, seat);
}

static void seat_create_text_input(struct vo_wayland_seat *seat)
{
    seat->text_input = talloc_zero(seat, struct vo_wayland_text_input);
    seat->text_input->text_input = zwp_text_input_manager_v3_get_text_input(seat->wl->text_input_manager, seat->seat);
    zwp_text_input_v3_add_listener(seat->text_input->text_input, &text_input_listener, seat);
}

static void set_color_management(struct vo_wayland_state *wl)
{
#if HAVE_WAYLAND_PROTOCOLS_1_41
    if (!wl->color_surface)
        return;

    wp_color_management_surface_v1_unset_image_description(wl->color_surface);

    struct pl_color_space color = wl->target_params.color;
    int primaries = wl->primaries_map[color.primaries];
    int transfer = wl->transfer_map[color.transfer];
    if (!primaries)
        MP_VERBOSE(wl, "Compositor does not support color primary: %s\n", m_opt_choice_str(pl_csp_prim_names, color.primaries));
    if (!transfer)
        MP_VERBOSE(wl, "Compositor does not support transfer function: %s\n", m_opt_choice_str(pl_csp_trc_names, color.transfer));
    if (!primaries || !transfer)
        return;

    struct wp_image_description_creator_params_v1 *image_creator_params =
        wp_color_manager_v1_create_parametric_creator(wl->color_manager);
    wp_image_description_creator_params_v1_set_primaries_named(image_creator_params, primaries);
    wp_image_description_creator_params_v1_set_tf_named(image_creator_params, transfer);

    pl_color_space_infer(&wl->target_params.color);
    struct pl_hdr_metadata hdr = wl->target_params.color.hdr;
    bool is_hdr = pl_color_transfer_is_hdr(color.transfer);
    bool use_metadata = hdr_metadata_valid(&hdr);
    if (!use_metadata)
        MP_VERBOSE(wl, "supplied HDR metadata does not conform to the wayland color management protocol. It will not be used.\n");
    if (wl->supports_display_primaries && is_hdr && use_metadata) {
        wp_image_description_creator_params_v1_set_mastering_display_primaries(image_creator_params,
                lrintf(hdr.prim.red.x * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.red.y * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.green.x * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.green.y * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.blue.x * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.blue.y * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.white.x * WAYLAND_COLOR_FACTOR),
                lrintf(hdr.prim.white.y * WAYLAND_COLOR_FACTOR));

        wp_image_description_creator_params_v1_set_mastering_luminance(image_creator_params,
            lrintf(hdr.min_luma * WAYLAND_MIN_LUM_FACTOR), lrintf(hdr.max_luma));
        wp_image_description_creator_params_v1_set_max_cll(image_creator_params, lrintf(hdr.max_cll));
        wp_image_description_creator_params_v1_set_max_fall(image_creator_params, lrintf(hdr.max_fall));
    }
    struct wp_image_description_v1 *image_description = wp_image_description_creator_params_v1_create(image_creator_params);
    wp_image_description_v1_add_listener(image_description, &image_description_listener, wl);
#endif
}

static void set_color_representation(struct vo_wayland_state *wl)
{
#if HAVE_WAYLAND_PROTOCOLS_1_44
    if (!wl->color_representation_manager)
        return;

    if (wl->color_representation_surface)
        wp_color_representation_surface_v1_destroy(wl->color_representation_surface);

    wl->color_representation_surface =
        wp_color_representation_manager_v1_get_surface(wl->color_representation_manager, wl->callback_surface);

    struct pl_color_repr repr = wl->target_params.repr;
    int alpha = wl->alpha_map[repr.alpha];
    int coefficients = wl->coefficients_map[repr.sys];
    int range = repr.levels == PL_COLOR_LEVELS_FULL ? wl->range_map[repr.sys] :
                                wl->range_map[repr.sys + PL_COLOR_SYSTEM_COUNT];
    int chroma_location = map_supported_chroma_location(wl->target_params.chroma_location);

    if (coefficients && range)
        wp_color_representation_surface_v1_set_coefficients_and_range(wl->color_representation_surface, coefficients, range);

    if (alpha)
        wp_color_representation_surface_v1_set_alpha_mode(wl->color_representation_surface, alpha);

    if (chroma_location)
        wp_color_representation_surface_v1_set_chroma_location(wl->color_representation_surface, chroma_location);
#endif
}

static void set_content_type(struct vo_wayland_state *wl)
{
    if (!wl->content_type_manager)
        return;
    // handle auto;
    if (wl->opts->wl_content_type == -1) {
        wp_content_type_v1_set_content_type(wl->content_type, wl->current_content_type);
    } else {
        wp_content_type_v1_set_content_type(wl->content_type, wl->opts->wl_content_type);
    }
}

static void set_cursor_shape(struct vo_wayland_seat *s)
{
#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (s->cursor_shape_device)
        wp_cursor_shape_device_v1_set_shape(s->cursor_shape_device, s->pointer_enter_serial,
                                            WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);

    struct vo_wayland_tablet_tool *tablet_tool;
    wl_list_for_each(tablet_tool, &s->tablet_tool_list, link) {
        if (tablet_tool->cursor_shape_device)
            wp_cursor_shape_device_v1_set_shape(tablet_tool->cursor_shape_device,
                                                tablet_tool->proximity_serial,
                                                WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    }
#endif
}

static void set_cursor(struct vo_wayland_seat *s, struct wl_surface *cursor_surface,
                       int32_t hotspot_x, int32_t hotspot_y)
{
    wl_pointer_set_cursor(s->pointer, s->pointer_enter_serial,
                          cursor_surface, hotspot_x, hotspot_y);

    struct vo_wayland_tablet_tool *tablet_tool;
    wl_list_for_each(tablet_tool, &s->tablet_tool_list, link) {
        zwp_tablet_tool_v2_set_cursor(tablet_tool->tablet_tool, tablet_tool->proximity_serial,
                                      cursor_surface, hotspot_x, hotspot_y);
    }
}

static int set_cursor_visibility(struct vo_wayland_seat *s, bool on)
{
    if (!s)
        return VO_FALSE;
    struct vo_wayland_state *wl = s->wl;
    wl->cursor_visible = on;
    if (on) {
        if (s->wl->cursor_shape_manager) {
            set_cursor_shape(s);
        } else {
            if (spawn_cursor(wl))
                return VO_FALSE;
            struct wl_cursor_image *img = wl->default_cursor->images[0];
            struct wl_buffer *buffer = wl_cursor_image_get_buffer(img);
            if (!buffer)
                return VO_FALSE;
            double scale = MPMAX(wl->scaling_factor, 1);
            set_cursor(s, wl->cursor_surface, img->hotspot_x / scale, img->hotspot_y / scale);
            wp_viewport_set_destination(wl->cursor_viewport, lround(img->width / scale),
                                        lround(img->height / scale));
            wl_surface_attach(wl->cursor_surface, buffer, 0, 0);
            wl_surface_damage_buffer(wl->cursor_surface, 0, 0, img->width, img->height);
        }
        wl_surface_commit(wl->cursor_surface);
    } else {
        set_cursor(s, NULL, 0, 0);
    }
    return VO_TRUE;
}

static int set_cursor_visibility_all_seats(struct vo_wayland_state *wl, bool on)
{
    bool unavailable = true;
    bool failed = false;
    struct vo_wayland_seat *seat;
    wl_list_for_each(seat, &wl->seat_list, link) {
        if (seat->pointer) {
            unavailable = false;
            if (set_cursor_visibility(seat, on) == VO_FALSE)
                failed = true;
        }
    }

    if (unavailable)
        return VO_NOTAVAIL;
    if (failed)
        return VO_FALSE;

    return VO_TRUE;
}

static void set_geometry(struct vo_wayland_state *wl, bool resize)
{
    struct vo *vo = wl->vo;
    if (!wl->current_output)
        return;

    struct vo_win_geometry geo;
    struct mp_rect screenrc = wl->current_output->geometry;
    vo_calc_window_geometry(vo, &screenrc, &screenrc, wl->scaling_factor, false, &geo);
    vo_apply_window_geometry(vo, &geo);

    int gcd = greatest_common_divisor(vo->dwidth, vo->dheight);
    wl->reduced_width = vo->dwidth / gcd;
    wl->reduced_height = vo->dheight / gcd;

    if (!wl->initial_size_hint)
        wl->window_size = (struct mp_rect){0, 0, vo->dwidth, vo->dheight};
    wl->initial_size_hint = false;

    if (resize) {
        if (!wl->locked_size)
            wl->geometry = wl->window_size;
        wl->pending_vo_events |= VO_EVENT_RESIZE;
    }
}

static void set_input_region(struct vo_wayland_state *wl, bool passthrough)
{
    if (passthrough) {
        struct wl_region *region = wl_compositor_create_region(wl->compositor);
        wl_surface_set_input_region(wl->surface, region);
        wl_region_destroy(region);
    } else {
        wl_surface_set_input_region(wl->surface, NULL);
    }
}

static int set_screensaver_inhibitor(struct vo_wayland_state *wl, int state)
{
    if (!wl->idle_inhibit_manager)
        return VO_NOTIMPL;
    if (state == (!!wl->idle_inhibitor))
        return VO_TRUE;
    if (state) {
        MP_VERBOSE(wl, "Enabling idle inhibitor\n");
        struct zwp_idle_inhibit_manager_v1 *mgr = wl->idle_inhibit_manager;
        wl->idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(mgr, wl->callback_surface);
    } else {
        MP_VERBOSE(wl, "Disabling the idle inhibitor\n");
        zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);
        wl->idle_inhibitor = NULL;
    }
    return VO_TRUE;
}

static void set_surface_scaling(struct vo_wayland_state *wl)
{
    if (wl->scale_configured && (wl->fractional_scale_manager ||
        wl_surface_get_version(wl->surface) >= 6))
    {
        return;
    }

    double old_scale = wl->scaling;
    wl->scaling = wl->current_output->scale;
    wl->scaling_factor = wl->scaling / WAYLAND_SCALE_FACTOR;
    rescale_geometry(wl, old_scale);
    wl->pending_vo_events |= VO_EVENT_DPI;
}

static void set_window_bounds(struct vo_wayland_state *wl)
{
    // If the user has set geometry/autofit and the option is auto,
    // don't use these.
    if (wl->opts->wl_configure_bounds == -1 && (wl->opts->geometry.wh_valid ||
        wl->opts->autofit.wh_valid || wl->opts->autofit_larger.wh_valid ||
        wl->opts->autofit_smaller.wh_valid))
    {
        return;
    }

    apply_keepaspect(wl, &wl->bounded_width, &wl->bounded_height);

    if (wl->bounded_width && wl->bounded_width < wl->window_size.x1)
        wl->window_size.x1 = wl->bounded_width;
    if (wl->bounded_height && wl->bounded_height < wl->window_size.y1)
        wl->window_size.y1 = wl->bounded_height;
}

static bool single_output_spanned(struct vo_wayland_state *wl)
{
    int outputs = 0;
    struct vo_wayland_output *output;
    wl_list_for_each(output, &wl->output_list, link) {
        if (output->has_surface)
            ++outputs;
        if (outputs > 1)
            return false;
    }
    return wl->current_output && outputs == 1;
}

static int spawn_cursor(struct vo_wayland_state *wl)
{
    if (wl->allocated_cursor_scale == wl->scaling) {
        return 0;
    } else if (wl->cursor_theme) {
        wl_cursor_theme_destroy(wl->cursor_theme);
    }

    const char *xcursor_theme = getenv("XCURSOR_THEME");
    const char *size_str = getenv("XCURSOR_SIZE");
    int size = 24;
    if (size_str != NULL) {
        errno = 0;
        char *end;
        long size_long = strtol(size_str, &end, 10);
        if (!*end && !errno && size_long > 0 && size_long <= INT_MAX)
            size = (int)size_long;
    }

    wl->cursor_theme = wl_cursor_theme_load(xcursor_theme, handle_round(wl->scaling, size),
                                            wl->shm);
    if (!wl->cursor_theme) {
        MP_ERR(wl, "Unable to load cursor theme!\n");
        return 1;
    }

    wl->default_cursor = wl_cursor_theme_get_cursor(wl->cursor_theme, "default");
    if (!wl->default_cursor)
        wl->default_cursor = wl_cursor_theme_get_cursor(wl->cursor_theme, "left_ptr");

    if (!wl->default_cursor) {
        MP_ERR(wl, "Unable to get default and left_ptr XCursor from theme!\n");
        return 1;
    }

    wl->allocated_cursor_scale = wl->scaling;

    return 0;
}

static void toggle_fullscreen(struct vo_wayland_state *wl)
{
    bool specific_screen = wl->opts->fsscreen_id >= 0 || wl->opts->fsscreen_name;
    if (wl->opts->fullscreen && !specific_screen) {
        xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
    } else if (wl->opts->fullscreen && specific_screen) {
        struct vo_wayland_output *output = find_output(wl);
        xdg_toplevel_set_fullscreen(wl->xdg_toplevel, output->output);
    } else {
        wl->state_change = wl->reconfigured;
        xdg_toplevel_unset_fullscreen(wl->xdg_toplevel);
    }
}

static void toggle_ime(struct vo_wayland_state *wl)
{
    struct vo_wayland_seat *seat;
    wl_list_for_each(seat, &wl->seat_list, link) {
        struct vo_wayland_text_input *ti = seat->text_input;
        if (!ti)
            continue;
        if (wl->opts->input_ime) {
            enable_ime(ti);
        } else {
            disable_ime(ti);
        }
    }
}

static void toggle_maximized(struct vo_wayland_state *wl)
{
    if (wl->opts->window_maximized) {
        xdg_toplevel_set_maximized(wl->xdg_toplevel);
    } else {
        wl->state_change = wl->reconfigured;
        xdg_toplevel_unset_maximized(wl->xdg_toplevel);
    }
}

static void update_app_id(struct vo_wayland_state *wl)
{
    xdg_toplevel_set_app_id(wl->xdg_toplevel, wl->opts->appid);
}

static void update_output_scaling(struct vo_wayland_state *wl)
{
    double old_scale = wl->scaling;
    wl->scaling = wl->pending_scaling;
    wl->scaling_factor = wl->scaling / WAYLAND_SCALE_FACTOR;
    rescale_geometry(wl, old_scale);
    set_geometry(wl, false);
    wl->need_rescale = false;
    wl->pending_vo_events |= VO_EVENT_DPI | VO_EVENT_RESIZE;
}

static void update_output_geometry(struct vo_wayland_state *wl, struct mp_rect old_geometry,
                                   struct mp_rect old_output_geometry)
{
    if (wl->need_rescale) {
        update_output_scaling(wl);
        return;
    }

    bool force_resize = false;
    bool use_output_scale = wl_surface_get_version(wl->surface) < 6 &&
                            !wl->fractional_scale_manager &&
                            wl->scaling != wl->current_output->scale;

    if (use_output_scale) {
        set_surface_scaling(wl);
        force_resize = true;
    }

    if (!mp_rect_equals(&old_output_geometry, &wl->current_output->geometry)) {
        set_geometry(wl, false);
        force_resize = true;
    }

    if (!mp_rect_equals(&old_geometry, &wl->geometry) || force_resize)
        wl->pending_vo_events |= VO_EVENT_RESIZE;
}

static int update_window_title(struct vo_wayland_state *wl, const char *title)
{
    /* The xdg-shell protocol requires that the title is UTF-8. */
    void *tmp = talloc_new(NULL);
    struct bstr b_title = bstr_sanitize_utf8_latin1(tmp, bstr0(title));
    xdg_toplevel_set_title(wl->xdg_toplevel, bstrto0(tmp, b_title));
    talloc_free(tmp);
    return VO_TRUE;
}

static void wayland_dispatch_events(struct vo_wayland_state *wl, int nfds, int64_t timeout_ns)
{
    if (wl->display_fd == -1)
        return;

    struct pollfd fds[2] = {
        {.fd = wl->display_fd,     .events = POLLIN },
        {.fd = wl->wakeup_pipe[0], .events = POLLIN },
    };

    while (wl_display_prepare_read(wl->display) != 0)
        wl_display_dispatch_pending(wl->display);
    wl_display_flush(wl->display);

    mp_poll(fds, nfds, timeout_ns);

    if (fds[0].revents & POLLIN) {
        wl_display_read_events(wl->display);
    } else {
        wl_display_cancel_read(wl->display);
    }

    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        MP_FATAL(wl, "Error occurred on the display fd\n");
        wl->display_fd = -1;
        mp_input_put_key(wl->vo->input_ctx, MP_KEY_CLOSE_WIN);
    }

    if (fds[1].revents & POLLIN)
        mp_flush_wakeup_pipe(wl->wakeup_pipe[0]);

    wl_display_dispatch_pending(wl->display);
}

static void begin_dragging(struct vo_wayland_state *wl)
{
    struct vo_wayland_seat *s = wl->last_button_seat;
    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y) &&
        !wl->opts->fullscreen && s)
    {
        xdg_toplevel_move(wl->xdg_toplevel, s->seat, s->pointer_button_serial);
        wl->last_button_seat = NULL;
        mp_input_put_key(wl->vo->input_ctx, MP_INPUT_RELEASE_ALL);
    }
}

/* Non-static */
int vo_wayland_allocate_memfd(struct vo *vo, size_t size)
{
#if !HAVE_MEMFD_CREATE
    return VO_ERROR;
#else
    int fd = memfd_create("mpv", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        MP_ERR(vo, "Failed to allocate memfd: %s\n", mp_strerror(errno));
        return VO_ERROR;
    }

    fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);

    if (posix_fallocate(fd, 0, size) == 0)
        return fd;

    close(fd);
    MP_ERR(vo, "Failed to allocate memfd: %s\n", mp_strerror(errno));

    return VO_ERROR;
#endif
}

bool vo_wayland_check_visible(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    bool render = !wl->hidden || wl->opts->force_render;
    wl->frame_wait = true;
    return render;
}

struct pl_color_space vo_wayland_preferred_csp(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    return wl->preferred_csp;
}

int vo_wayland_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_wayland_state *wl = vo->wl;
    struct mp_vo_opts *opts = wl->opts;
    wl_display_dispatch_pending(wl->display);

    switch (request) {
    case VOCTRL_CHECK_EVENTS: {
        wayland_dispatch_events(wl, 1, 0);
        struct vo_wayland_seat *seat;
        wl_list_for_each(seat, &wl->seat_list, link) {
            check_fd(wl, seat->dnd_offer, true);
            check_fd(wl, seat->selection_offer, false);
        }
        *events |= wl->pending_vo_events;
        if (*events & VO_EVENT_RESIZE) {
            *events |= VO_EVENT_EXPOSE;
            wl->frame_wait = false;
            wl->timeout_count = 0;
            wl->hidden = false;
        }
        wl->pending_vo_events = 0;
        return VO_TRUE;
    }
    case VOCTRL_VO_OPTS_CHANGED: {
        void *opt;
        while (m_config_cache_get_next_changed(wl->opts_cache, &opt)) {
            if (opt == &opts->appid)
                update_app_id(wl);
            if (opt == &opts->border)
            {
                // This is stupid but the value of border shouldn't be written
                // unless we get a configure event. Change it back to its old
                // value and let configure_decorations handle it after the request.
                if (wl->xdg_toplevel_decoration) {
                    int requested_border_mode = opts->border;
                    opts->border = !opts->border;
                    m_config_cache_write_opt(wl->opts_cache,
                                             &opts->border);
                    request_decoration_mode(
                        wl, requested_border_mode ?
                            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE :
                            ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                } else {
                    opts->border = false;
                    m_config_cache_write_opt(wl->opts_cache,
                                             &wl->opts->border);
                }
            }
            if (opt == &opts->wl_content_type)
                set_content_type(wl);
            if (opt == &opts->cursor_passthrough)
                set_input_region(wl, opts->cursor_passthrough);
            if (opt == &opts->input_ime)
                toggle_ime(wl);
            if (opt == &opts->fullscreen)
                toggle_fullscreen(wl);
            if (opt == &opts->window_maximized)
                toggle_maximized(wl);
            if (opt == &opts->window_minimized)
                do_minimize(wl);
            if (opt == &opts->geometry || opt == &opts->autofit ||
                opt == &opts->autofit_smaller || opt == &opts->autofit_larger)
            {
                wl->state_change = true;
                set_geometry(wl, true);
            }
        }
        return VO_TRUE;
    }
    case VOCTRL_CONTENT_TYPE: {
        wl->current_content_type = *(enum mp_content_type *)arg;
        set_content_type(wl);
        return VO_TRUE;
    }
    case VOCTRL_GET_FOCUSED: {
        *(bool *)arg = wl->focused;
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_NAMES: {
        *(char ***)arg = get_displays_spanned(wl);
        return VO_TRUE;
    }
    case VOCTRL_GET_ICC_PROFILE: {
        if (!wl->supports_icc)
            MP_WARN(wl, "Compositor does not support ICC profiles!\n");
        if (!wl->icc_file)
            return VO_FALSE;
        MP_VERBOSE(wl, "Retrieving ICC profile from compositor.\n");
        *(bstr *)arg = bstrdup(NULL, (bstr){wl->icc_file, wl->icc_size});
        return VO_TRUE;
    }
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        if (wl->opts->window_maximized || wl->tiled) {
            s[0] = mp_rect_w(wl->geometry);
            s[1] = mp_rect_h(wl->geometry);
        } else {
            s[0] = mp_rect_w(wl->window_size);
            s[1] = mp_rect_h(wl->window_size);
        }
        return VO_TRUE;
    }
    case VOCTRL_SET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        wl->window_size.x0 = 0;
        wl->window_size.y0 = 0;
        wl->window_size.x1 = s[0];
        wl->window_size.y1 = s[1];
        if (!wl->opts->fullscreen && !wl->tiled) {
            wl->state_change = true;
            if (wl->opts->window_maximized) {
                xdg_toplevel_unset_maximized(wl->xdg_toplevel);
                wl_display_dispatch_pending(wl->display);
                /* Make sure the compositor let us unmaximize */
                if (wl->opts->window_maximized)
                    return VO_TRUE;
            }
            wl->geometry = wl->window_size;
            wl->pending_vo_events |= VO_EVENT_RESIZE;
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_FPS: {
        struct vo_wayland_output *out;
        if (wl->current_output) {
            out = wl->current_output;
        } else {
            out = find_output(wl);
        }
        if (!out)
            return VO_NOTAVAIL;
        *(double *)arg = out->refresh_rate;
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_RES: {
        struct vo_wayland_output *out;
        if (wl->current_output) {
            out = wl->current_output;
        } else {
            out = find_output(wl);
        }
        if (!out)
            return VO_NOTAVAIL;
        ((int *)arg)[0] = out->geometry.x1;
        ((int *)arg)[1] = out->geometry.y1;
        return VO_TRUE;
    }
    case VOCTRL_GET_HIDPI_SCALE: {
        if (!wl->scaling_factor)
            return VO_NOTAVAIL;
        *(double *)arg = wl->scaling_factor;
        return VO_TRUE;
    }
    case VOCTRL_BEGIN_DRAGGING:
        begin_dragging(wl);
        return VO_TRUE;
    case VOCTRL_UPDATE_WINDOW_TITLE:
        return update_window_title(wl, (const char *)arg);
    case VOCTRL_SET_CURSOR_VISIBILITY:
        return set_cursor_visibility_all_seats(wl, *(bool *)arg);
    case VOCTRL_KILL_SCREENSAVER:
        return set_screensaver_inhibitor(wl, true);
    case VOCTRL_RESTORE_SCREENSAVER:
        return set_screensaver_inhibitor(wl, false);
    case VOCTRL_GET_CLIPBOARD: {
        struct voctrl_clipboard *vc = arg;
        // TODO: add primary selection support
        if (vc->params.target != CLIPBOARD_TARGET_CLIPBOARD || vc->params.type != CLIPBOARD_DATA_TEXT)
            return VO_NOTAVAIL;
        vc->data.type = CLIPBOARD_DATA_TEXT;
        vc->data.u.text = bstrto0(vc->talloc_ctx, wl->selection_text);
        return VO_TRUE;
    }
    case VOCTRL_SET_CLIPBOARD: {
        struct voctrl_clipboard *vc = arg;
        // TODO: add primary selection support
        if (vc->params.target != CLIPBOARD_TARGET_CLIPBOARD || vc->params.type != CLIPBOARD_DATA_TEXT)
            return VO_NOTIMPL;
        if (vc->data.type != CLIPBOARD_DATA_TEXT)
            return VO_NOTIMPL;
        talloc_free(wl->selection_text.start);
        wl->selection_text = bstrdup(wl, bstr0(vc->data.u.text));
        struct vo_wayland_seat *seat;
        wl_list_for_each(seat, &wl->seat_list, link) {
            if (seat->last_serial && !seat->data_source && wl->devman)
                seat_create_data_source(seat);
        }
        return VO_TRUE;
    }
    }

    return VO_NOTIMPL;
}

void vo_wayland_handle_color(struct vo_wayland_state *wl)
{
    if (!wl->vo->target_params)
        return;
    struct mp_image_params target_params = vo_get_target_params(wl->vo);
    if (pl_color_space_equal(&target_params.color, &wl->target_params.color) &&
        pl_color_repr_equal(&target_params.repr, &wl->target_params.repr) &&
        target_params.chroma_location == wl->target_params.chroma_location)
        return;
    wl->target_params = target_params;
    set_color_management(wl);
    set_color_representation(wl);
}


void vo_wayland_handle_scale(struct vo_wayland_state *wl)
{
    wp_viewport_set_destination(wl->viewport, lround(mp_rect_w(wl->geometry) / wl->scaling_factor),
                                lround(mp_rect_h(wl->geometry) / wl->scaling_factor));
}

bool vo_wayland_valid_format(struct vo_wayland_state *wl, uint32_t drm_format, uint64_t modifier)
{
    // Tranches are grouped by preference and the first tranche is at the end of
    // the list. It doesn't really matter for us since we search everything
    // anyways, but might as well start from the most preferred tranche.
    struct vo_wayland_tranche *tranche;
    wl_list_for_each_reverse(tranche, &wl->tranche_list, link) {
        struct drm_format *formats = tranche->compositor_formats;
        for (int i = 0; i < tranche->num_compositor_formats; ++i) {
            if (drm_format == formats[i].format && modifier == formats[i].modifier)
                return true;
        }
    }
    return false;
}

bool vo_wayland_init(struct vo *vo)
{
    if (!getenv("WAYLAND_DISPLAY") && !getenv("WAYLAND_SOCKET"))
        goto err;

    vo->wl = talloc_zero(NULL, struct vo_wayland_state);
    struct vo_wayland_state *wl = vo->wl;

    *wl = (struct vo_wayland_state) {
        .display = wl_display_connect(NULL),
        .vo = vo,
        .log = mp_log_new(wl, vo->log, "wayland"),
        .bounded_width = 0,
        .bounded_height = 0,
        .refresh_interval = 0,
        .scaling = WAYLAND_SCALE_FACTOR,
        .wakeup_pipe = {-1, -1},
        .display_fd = -1,
        .cursor_visible = true,
        .last_zero_copy = -1,
        .opts_cache = m_config_cache_alloc(wl, vo->global, &vo_sub_opts),
        .preferred_csp = (struct pl_color_space) { .transfer = PL_COLOR_TRC_SRGB, .primaries = PL_COLOR_PRIM_BT_709 },
    };
    wl->opts = wl->opts_cache->opts;

    wl_list_init(&wl->output_list);
    wl_list_init(&wl->seat_list);
    wl_list_init(&wl->tranche_list);

    if (!wl->display)
        goto err;

    if (create_input(wl))
        goto err;

    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);

    /* Do a roundtrip to run the registry */
    wl_display_roundtrip(wl->display);

    if (!wl->surface) {
        MP_FATAL(wl, "Compositor doesn't support %s (ver. 4)\n",
                 wl_compositor_interface.name);
        goto err;
    }

    if (!wl->wm_base) {
        MP_FATAL(wl, "Compositor doesn't support the required %s protocol!\n",
                 xdg_wm_base_interface.name);
        goto err;
    }

    if (wl_list_empty(&wl->output_list)) {
        MP_FATAL(wl, "No outputs found or compositor doesn't support %s (ver. 2)\n",
                 wl_output_interface.name);
        goto err;
    }

    if (!wl->viewporter) {
        MP_FATAL(wl, "Compositor doesn't support the required %s protocol!\n",
                 wp_viewporter_interface.name);
        goto err;
    }

    /* Can't be initialized during registry due to multi-protocol dependence */
    if (create_viewports(wl))
        goto err;

    if (create_xdg_surface(wl))
        goto err;

    if (wl->xdg_activation) {
        xdg_activate(wl);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
            xdg_activation_v1_interface.name);
    }

    if (wl->subcompositor) {
        wl->osd_subsurface = wl_subcompositor_get_subsurface(wl->subcompositor, wl->osd_surface, wl->video_surface);
        wl->video_subsurface = wl_subcompositor_get_subsurface(wl->subcompositor, wl->video_surface, wl->surface);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_41
    if (!wl->color_manager) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_color_manager_v1_interface.name);
    }
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_44
    if (!wl->color_representation_manager) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_color_representation_surface_v1_interface.name);
    }
#endif

    if (wl->content_type_manager) {
        wl->content_type = wp_content_type_manager_v1_get_surface_content_type(wl->content_type_manager, wl->surface);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_content_type_manager_v1_interface.name);
    }

    if (!wl->single_pixel_manager) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_single_pixel_buffer_manager_v1_interface.name);
    }

    if (wl->fractional_scale_manager) {
        wl->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(wl->fractional_scale_manager, wl->surface);
        wp_fractional_scale_v1_add_listener(wl->fractional_scale, &fractional_scale_listener, wl);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_fractional_scale_manager_v1_interface.name);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (!wl->cursor_shape_manager) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_cursor_shape_manager_v1_interface.name);
    }
#endif

    if (wl->devman) {
        struct vo_wayland_seat *seat;
        wl_list_for_each(seat, &wl->seat_list, link) {
            if (!seat->data_device)
                seat_create_data_device(seat);
        }
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s (ver. 3) protocol!\n",
                   wl_data_device_manager_interface.name);
    }

    if (wl->presentation) {
        wl->fback_pool = talloc_zero(wl, struct vo_wayland_feedback_pool);
        wl->fback_pool->wl = wl;
        wl->fback_pool->len = VO_MAX_SWAPCHAIN_DEPTH;
        wl->fback_pool->fback = talloc_zero_array(wl->fback_pool, struct wp_presentation_feedback *,
                                                  wl->fback_pool->len);
        wl->present = mp_present_initialize(wl, wl->opts, VO_MAX_SWAPCHAIN_DEPTH);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_presentation_interface.name);
    }

    if (wl->xdg_decoration_manager) {
        wl->xdg_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wl->xdg_decoration_manager, wl->xdg_toplevel);
        zxdg_toplevel_decoration_v1_add_listener(wl->xdg_toplevel_decoration, &decoration_listener, wl);
        request_decoration_mode(
            wl, wl->opts->border ?
                ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE :
                ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    } else {
        wl->opts->border = false;
        m_config_cache_write_opt(wl->opts_cache,
                                 &wl->opts->border);
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   zxdg_decoration_manager_v1_interface.name);
    }

    if (!wl->idle_inhibit_manager) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   zwp_idle_inhibit_manager_v1_interface.name);
    }

    if (wl->text_input_manager) {
        struct vo_wayland_seat *seat;
        wl_list_for_each(seat, &wl->seat_list, link) {
            if (!seat->text_input)
                seat_create_text_input(seat);
        }
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                    zwp_text_input_manager_v3_interface.name);
    }

    if (wl->wp_tablet_manager) {
        struct vo_wayland_seat *seat;
        wl_list_for_each(seat, &wl->seat_list, link) {
            if (!seat->tablet_seat)
                seat_create_tablet_seat(wl, seat);
        }
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                    zwp_tablet_manager_v2_interface.name);
    }

    wl->display_fd = wl_display_get_fd(wl->display);

    update_app_id(wl);
    mp_make_wakeup_pipe(wl->wakeup_pipe);

    wl->frame_callback = wl_surface_frame(wl->callback_surface);
    wl_callback_add_listener(wl->frame_callback, &frame_listener, wl);
    wl_surface_commit(wl->surface);

    /* Do another roundtrip to ensure all of the above is initialized
     * before mpv does anything else. */
    wl_display_roundtrip(wl->display);

#if HAVE_WAYLAND_PROTOCOLS_1_41
    // Only bind color surface to vo_dmabuf_wayland for now to avoid conflicting with graphics drivers
    if (wl->color_manager && wl->supports_parametric && !strcmp(wl->vo->driver->name, "dmabuf-wayland"))
        wl->color_surface = wp_color_manager_v1_get_surface(wl->color_manager, wl->callback_surface);

    if (wl->color_manager && (wl->supports_parametric || wl->supports_icc)) {
        wl->color_surface_feedback = wp_color_manager_v1_get_surface_feedback(wl->color_manager, wl->callback_surface);
        wp_color_management_surface_feedback_v1_add_listener(wl->color_surface_feedback, &surface_feedback_listener, wl);
    }
#endif

    if (wl->supports_parametric)
        get_compositor_preferred_description(wl, true);
    else
        MP_VERBOSE(wl, "Compositor does not support parametric image descriptions!\n");


    struct gl_video_opts *gl_opts = mp_get_config_group(NULL, vo->global, &gl_video_conf);
    if (wl->supports_icc) {
        // dumb workaround for avoiding -Wunused-function
        get_compositor_preferred_description(wl, false);
    } else {
        int msg_level = gl_opts->icc_opts->profile_auto ? MSGL_WARN : MSGL_V;
        mp_msg(wl->log, msg_level, "Compositor does not support ICC profiles!\n");
    }
    talloc_free(gl_opts);

    return true;

err:
    vo_wayland_uninit(vo);
    return false;
}

bool vo_wayland_reconfig(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;

    MP_VERBOSE(wl, "Reconfiguring!\n");

    if (!wl->current_output) {
        wl->current_output = find_output(wl);
        if (!wl->current_output)
            return false;
        set_surface_scaling(wl);
        wl->scale_configured = true;
        wl->pending_vo_events |= VO_EVENT_DPI;
    }

    if (wl->opts->auto_window_resize || !wl->geometry_configured)
        set_geometry(wl, false);

    if (wl->geometry_configured && wl->opts->auto_window_resize)
        wl->reconfigured = true;

    if (wl->opts->wl_configure_bounds)
        set_window_bounds(wl);

    if (wl->opts->cursor_passthrough)
        set_input_region(wl, true);

    if (!wl->geometry_configured || !wl->locked_size)
        wl->geometry = wl->window_size;

    if (!wl->geometry_configured) {
        if (wl->opts->fullscreen)
            toggle_fullscreen(wl);

        if (wl->opts->window_maximized)
            toggle_maximized(wl);

        if (wl->opts->window_minimized)
            do_minimize(wl);
        wl->geometry_configured = true;
    }

    wl->pending_vo_events |= VO_EVENT_RESIZE;

    return true;
}

void vo_wayland_set_opaque_region(struct vo_wayland_state *wl, bool alpha)
{
    const int32_t width = lrint(mp_rect_w(wl->geometry) / wl->scaling_factor);
    const int32_t height = lrint(mp_rect_h(wl->geometry) / wl->scaling_factor);
    if (!alpha) {
        struct wl_region *region = wl_compositor_create_region(wl->compositor);
        wl_region_add(region, 0, 0, width, height);
        wl_surface_set_opaque_region(wl->surface, region);
        wl_region_destroy(region);
    } else {
        wl_surface_set_opaque_region(wl->surface, NULL);
    }
}

void vo_wayland_uninit(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    if (!wl)
        return;

    mp_input_put_key(wl->vo->input_ctx, MP_INPUT_RELEASE_ALL);

    // Ensure that any in-flight vo_wayland_preferred_description_info get deallocated.
    wl_display_roundtrip(wl->display);

    if (wl->compositor)
        wl_compositor_destroy(wl->compositor);

    if (wl->subcompositor)
        wl_subcompositor_destroy(wl->subcompositor);

#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (wl->cursor_shape_manager)
        wp_cursor_shape_manager_v1_destroy(wl->cursor_shape_manager);
#endif

    if (wl->cursor_surface)
        wl_surface_destroy(wl->cursor_surface);

    if (wl->cursor_theme)
        wl_cursor_theme_destroy(wl->cursor_theme);

#if HAVE_WAYLAND_PROTOCOLS_1_41
    if (wl->color_manager)
        wp_color_manager_v1_destroy(wl->color_manager);

    if (wl->color_surface)
        wp_color_management_surface_v1_destroy(wl->color_surface);

    if (wl->color_surface_feedback)
        wp_color_management_surface_feedback_v1_destroy(wl->color_surface_feedback);
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_44
    if (wl->color_representation_manager)
        wp_color_representation_manager_v1_destroy(wl->color_representation_manager);

    if (wl->color_representation_surface)
        wp_color_representation_surface_v1_destroy(wl->color_representation_surface);
#endif

    if (wl->content_type)
        wp_content_type_v1_destroy(wl->content_type);

    if (wl->content_type_manager)
        wp_content_type_manager_v1_destroy(wl->content_type_manager);

    if (wl->devman)
        wl_data_device_manager_destroy(wl->devman);

    if (wl->fback_pool)
        clean_feedback_pool(wl->fback_pool);

    if (wl->fractional_scale)
        wp_fractional_scale_v1_destroy(wl->fractional_scale);

    if (wl->fractional_scale_manager)
        wp_fractional_scale_manager_v1_destroy(wl->fractional_scale_manager);

    if (wl->frame_callback)
        wl_callback_destroy(wl->frame_callback);

    if (wl->idle_inhibitor)
        zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);

    if (wl->idle_inhibit_manager)
        zwp_idle_inhibit_manager_v1_destroy(wl->idle_inhibit_manager);

    if (wl->text_input_manager)
        zwp_text_input_manager_v3_destroy(wl->text_input_manager);

    if (wl->presentation)
        wp_presentation_destroy(wl->presentation);

    if (wl->registry)
        wl_registry_destroy(wl->registry);

    if (wl->viewporter)
        wp_viewporter_destroy(wl->viewporter);

    if (wl->viewport)
        wp_viewport_destroy(wl->viewport);

    if (wl->cursor_viewport)
        wp_viewport_destroy(wl->cursor_viewport);

    if (wl->osd_viewport)
        wp_viewport_destroy(wl->osd_viewport);

    if (wl->video_viewport)
        wp_viewport_destroy(wl->video_viewport);

    if (wl->dmabuf)
        zwp_linux_dmabuf_v1_destroy(wl->dmabuf);

    if (wl->dmabuf_feedback)
        zwp_linux_dmabuf_feedback_v1_destroy(wl->dmabuf_feedback);

    if (wl->shm)
        wl_shm_destroy(wl->shm);

    if (wl->single_pixel_manager)
        wp_single_pixel_buffer_manager_v1_destroy(wl->single_pixel_manager);

    if (wl->surface)
        wl_surface_destroy(wl->surface);

    if (wl->osd_surface)
        wl_surface_destroy(wl->osd_surface);

    if (wl->osd_subsurface)
        wl_subsurface_destroy(wl->osd_subsurface);

    if (wl->video_surface)
        wl_surface_destroy(wl->video_surface);

    if (wl->video_subsurface)
        wl_subsurface_destroy(wl->video_subsurface);

    if (wl->wm_base)
        xdg_wm_base_destroy(wl->wm_base);

    if (wl->xdg_activation)
        xdg_activation_v1_destroy(wl->xdg_activation);

    if (wl->xdg_decoration_manager)
        zxdg_decoration_manager_v1_destroy(wl->xdg_decoration_manager);

    if (wl->xdg_toplevel)
        xdg_toplevel_destroy(wl->xdg_toplevel);

    if (wl->xdg_toplevel_decoration)
        zxdg_toplevel_decoration_v1_destroy(wl->xdg_toplevel_decoration);

    if (wl->xdg_surface)
        xdg_surface_destroy(wl->xdg_surface);

    if (wl->xkb_context)
        xkb_context_unref(wl->xkb_context);

    struct vo_wayland_output *output, *output_tmp;
    wl_list_for_each_safe(output, output_tmp, &wl->output_list, link)
        remove_output(output);

    struct vo_wayland_seat *seat, *seat_tmp;
    wl_list_for_each_safe(seat, seat_tmp, &wl->seat_list, link)
        remove_seat(seat);

    if (wl->wp_tablet_manager)
        zwp_tablet_manager_v2_destroy(wl->wp_tablet_manager);

    if (wl->display)
        wl_display_disconnect(wl->display);

    munmap(wl->compositor_format_map, wl->compositor_format_size);

    for (int n = 0; n < 2; n++)
        close(wl->wakeup_pipe[n]);
    talloc_free(wl);
    vo->wl = NULL;
}

void vo_wayland_wait_frame(struct vo_wayland_state *wl)
{
    int64_t vblank_time = 0;
    /* We need some vblank interval to use for the timeout in
     * this function. The order of preference of values to use is:
     * 1. vsync duration from presentation time
     * 2. refresh interval reported by presentation time
     * 3. refresh rate of the output reported by the compositor
     * 4. make up crap if vblank_time is still <= 0 (better than nothing) */

    if (wl->use_present && wl->present->head)
        vblank_time = wl->present->head->vsync_duration;

    if (vblank_time <= 0 && wl->refresh_interval > 0)
        vblank_time = wl->refresh_interval;

    if (vblank_time <= 0 && wl->current_output->refresh_rate > 0)
        vblank_time = 1e9 / wl->current_output->refresh_rate;

    // Ideally you should never reach this point.
    if (vblank_time <= 0)
        vblank_time = 1e9 / 60;

    // Completely arbitrary amount of additional time to wait.
    vblank_time += 0.05 * vblank_time;
    int64_t finish_time = mp_time_ns() + vblank_time;

    while (wl->frame_wait && finish_time > mp_time_ns()) {
        int64_t poll_time = finish_time - mp_time_ns();
        if (poll_time < 0) {
            poll_time = 0;
        }
        wayland_dispatch_events(wl, 1, poll_time);
    }

    /* If the compositor does not have presentation time, we cannot be sure
     * that this wait is accurate. Do a hacky block with wl_display_roundtrip. */
    if (!wl->use_present && !wl_display_get_error(wl->display))
        wl_display_roundtrip(wl->display);

    /* Only use this heuristic if the compositor doesn't support the suspended state. */
    if (wl->frame_wait && xdg_toplevel_get_version(wl->xdg_toplevel) < 6) {
        // Only consider consecutive missed callbacks.
        if (wl->timeout_count > 1) {
            wl->hidden = true;
            return;
        } else {
            wl->timeout_count += 1;
            return;
        }
    }

    wl->timeout_count = 0;
}

void vo_wayland_wait_events(struct vo *vo, int64_t until_time_ns)
{
    struct vo_wayland_state *wl = vo->wl;

    int64_t wait_ns = until_time_ns - mp_time_ns();
    int64_t timeout_ns = MPCLAMP(wait_ns, 0, MP_TIME_S_TO_NS(10));

    wayland_dispatch_events(wl, 2, timeout_ns);
}

void vo_wayland_wakeup(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    (void)write(wl->wakeup_pipe[1], &(char){0}, 1);
}
