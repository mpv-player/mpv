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
#include "wayland_common.h"
#include "win_state.h"

// Generated from wayland-protocols
#include "idle-inhibit-unstable-v1.h"
#include "linux-dmabuf-unstable-v1.h"
#include "presentation-time.h"
#include "xdg-decoration-unstable-v1.h"
#include "xdg-shell.h"
#include "viewporter.h"
#include "surface-invalidation-v1.h"

#if HAVE_WAYLAND_PROTOCOLS_1_27
#include "content-type-v1.h"
#include "single-pixel-buffer-v1.h"
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_31
#include "fractional-scale-v1.h"
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_32
#include "cursor-shape-v1.h"
#endif

#if WAYLAND_VERSION_MAJOR > 1 || WAYLAND_VERSION_MINOR >= 21
#define HAVE_WAYLAND_1_21
#endif

#if WAYLAND_VERSION_MAJOR > 1 || WAYLAND_VERSION_MINOR >= 22
#define HAVE_WAYLAND_1_22
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

#ifndef XDG_TOPLEVEL_STATE_SUSPENDED
#define XDG_TOPLEVEL_STATE_SUSPENDED 9
#endif


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
    {XKB_KEY_KP_Subtract, '-'}, {XKB_KEY_KP_Add,    '+'},
    {XKB_KEY_KP_Multiply, '*'}, {XKB_KEY_KP_Divide, '/'},
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
    {XKB_KEY_KP_Left,   MP_KEY_KPLEFT},  {XKB_KEY_KP_Begin,     MP_KEY_KP5},
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

#define OPT_BASE_STRUCT struct wayland_opts
const struct m_sub_options wayland_conf = {
    .opts = (const struct m_option[]) {
        {"wayland-configure-bounds", OPT_CHOICE(configure_bounds,
            {"auto", -1}, {"no", 0}, {"yes", 1})},
        {"wayland-disable-vsync", OPT_BOOL(disable_vsync)},
        {"wayland-edge-pixels-pointer", OPT_INT(edge_pixels_pointer),
            M_RANGE(0, INT_MAX)},
        {"wayland-edge-pixels-touch", OPT_INT(edge_pixels_touch),
            M_RANGE(0, INT_MAX)},
        {0},
    },
    .size = sizeof(struct wayland_opts),
    .defaults = &(struct wayland_opts) {
        .configure_bounds = -1,
        .edge_pixels_pointer = 16,
        .edge_pixels_touch = 32,
    },
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
    struct wl_data_device *dnd_ddev;
    /* TODO: unvoid this if required wayland protocols is bumped to 1.32+ */
    void *cursor_shape_device;
    uint32_t pointer_serial;
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
};

static int check_for_resize(struct vo_wayland_state *wl, int edge_pixels,
                            enum xdg_toplevel_resize_edge *edge);
static int get_mods(struct vo_wayland_seat *seat);
static int lookupkey(int key);
static int set_cursor_visibility(struct vo_wayland_seat *s, bool on);
static int spawn_cursor(struct vo_wayland_state *wl);

static void add_feedback(struct vo_wayland_feedback_pool *fback_pool,
                         struct wp_presentation_feedback *fback);
static void get_shape_device(struct vo_wayland_state *wl, struct vo_wayland_seat *s);
static int greatest_common_divisor(int a, int b);
static void guess_focus(struct vo_wayland_state *wl);
static void prepare_resize(struct vo_wayland_state *wl, int width, int height);
static void remove_feedback(struct vo_wayland_feedback_pool *fback_pool,
                            struct wp_presentation_feedback *fback);
static void remove_output(struct vo_wayland_output *out);
static void remove_seat(struct vo_wayland_seat *seat);
static void request_decoration_mode(struct vo_wayland_state *wl, uint32_t mode);
static void rescale_geometry(struct vo_wayland_state *wl, double old_scale);
static void set_geometry(struct vo_wayland_state *wl, bool resize);
static void set_surface_scaling(struct vo_wayland_state *wl);

/* Wayland listener boilerplate */
static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    s->pointer_serial = serial;
    set_cursor_visibility(s, wl->cursor_visible);
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_MOUSE_ENTER);
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_MOUSE_LEAVE);
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    wl->mouse_x = wl_fixed_to_int(sx) * wl->scaling;
    wl->mouse_y = wl_fixed_to_int(sy) * wl->scaling;

    if (!wl->toplevel_configured)
        mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y);
    wl->toplevel_configured = false;
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
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

    if (button)
        mp_input_put_key(wl->vo->input_ctx, button | state | s->mpmod);

    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y) &&
        !wl->locked_size && (button == MP_MBTN_LEFT) && (state == MP_KEY_STATE_DOWN))
    {
        uint32_t edges;
        // Implement an edge resize zone if there are no decorations
        if (!wl->vo_opts->border && check_for_resize(wl, wl->opts->edge_pixels_pointer, &edges)) {
            xdg_toplevel_resize(wl->xdg_toplevel, s->seat, serial, edges);
        } else {
            xdg_toplevel_move(wl->xdg_toplevel, s->seat, serial);
        }
        // Explicitly send an UP event after the client finishes a move/resize
        mp_input_put_key(wl->vo->input_ctx, button | MP_KEY_STATE_UP);
    }
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

#ifdef HAVE_WAYLAND_1_21
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
#endif

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
#ifdef HAVE_WAYLAND_1_21
    pointer_handle_axis_value120,
#endif
};

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
                              uint32_t serial, uint32_t time, struct wl_surface *surface,
                              int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    wl->mouse_x = wl_fixed_to_int(x_w) * wl->scaling;
    wl->mouse_y = wl_fixed_to_int(y_w) * wl->scaling;

    enum xdg_toplevel_resize_edge edge;
    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y)) {
        if (check_for_resize(wl, wl->opts->edge_pixels_touch, &edge)) {
            xdg_toplevel_resize(wl->xdg_toplevel, s->seat, serial, edge);
        } else  {
            xdg_toplevel_move(wl->xdg_toplevel, s->seat, serial);
        }
    }

    mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y);
    mp_input_put_key(wl->vo->input_ctx, MP_MBTN_LEFT | MP_KEY_STATE_DOWN);
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
                            uint32_t serial, uint32_t time, int32_t id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    mp_input_put_key(wl->vo->input_ctx, MP_MBTN_LEFT | MP_KEY_STATE_UP);
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
                                uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    wl->mouse_x = wl_fixed_to_int(x_w) * wl->scaling;
    wl->mouse_y = wl_fixed_to_int(y_w) * wl->scaling;

    mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y);
}

static void touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
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

    if (!s->xkb_keymap)
        s->xkb_keymap = xkb_keymap_new_from_buffer(wl->xkb_context, map_str,
                                                   strnlen(map_str, size),
                                                   XKB_KEYMAP_FORMAT_TEXT_V1, 0);

    munmap(map_str, size);
    close(fd);

    if (!s->xkb_keymap) {
        MP_ERR(wl, "failed to compile keymap\n");
        return;
    }

    if (!s->xkb_state)
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
    s->has_keyboard_input = true;
    guess_focus(wl);
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
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
    struct vo_wayland_state *wl = s->wl;

    s->keyboard_code = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(s->xkb_state, s->keyboard_code);
    int mpkey = lookupkey(sym);

    state = state == WL_KEYBOARD_KEY_STATE_PRESSED ? MP_KEY_STATE_DOWN
                                                   : MP_KEY_STATE_UP;

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
            if (state == MP_KEY_STATE_UP) {
                s->mpkey = 0;
                mp_input_put_key(wl->vo->input_ctx, MP_INPUT_RELEASE_ALL);
            }
            return;
        }
    }
    if (state == MP_KEY_STATE_DOWN)
        s->mpkey = mpkey;
    if (mpkey && state == MP_KEY_STATE_UP)
        s->mpkey = 0;
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    if (s->xkb_state) {
        xkb_state_update_mask(s->xkb_state, mods_depressed, mods_latched,
                              mods_locked, 0, 0, group);
        s->mpmod = get_mods(s);
        if (s->mpkey)
            mp_input_put_key(wl->vo->input_ctx, s->mpkey | MP_KEY_STATE_DOWN | s->mpmod);
    }
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                        int32_t rate, int32_t delay)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    if (wl->vo_opts->native_keyrepeat)
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
    int score = mp_event_get_mime_type_score(wl->vo->input_ctx, mime_type);
    if (score > wl->dnd_mime_score && wl->vo_opts->drag_and_drop != -2) {
        wl->dnd_mime_score = score;
        if (wl->dnd_mime_type)
            talloc_free(wl->dnd_mime_type);
        wl->dnd_mime_type = talloc_strdup(wl, mime_type);
        MP_VERBOSE(wl, "Given DND offer with mime type %s\n", wl->dnd_mime_type);
    }
}

static void data_offer_source_actions(void *data, struct wl_data_offer *offer, uint32_t source_actions)
{
}

static void data_offer_action(void *data, struct wl_data_offer *wl_data_offer, uint32_t dnd_action)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    if (dnd_action && wl->vo_opts->drag_and_drop != -2) {
        if (wl->vo_opts->drag_and_drop >= 0) {
            wl->dnd_action = wl->vo_opts->drag_and_drop;
        } else {
            wl->dnd_action = dnd_action & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY ?
                             DND_REPLACE : DND_APPEND;
        }
        MP_VERBOSE(wl, "DND action is %s\n",
                   wl->dnd_action == DND_REPLACE ? "DND_REPLACE" : "DND_APPEND");
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
    struct vo_wayland_state *wl = s->wl;
    if (wl->dnd_offer)
        wl_data_offer_destroy(wl->dnd_offer);

    wl->dnd_offer = id;
    wl_data_offer_add_listener(id, &data_offer_listener, s);
}

static void data_device_handle_enter(void *data, struct wl_data_device *wl_ddev,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y,
                                     struct wl_data_offer *id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    if (wl->dnd_offer != id) {
        MP_FATAL(wl, "DND offer ID mismatch!\n");
        return;
    }

    if (wl->vo_opts->drag_and_drop != -2) {
        wl_data_offer_set_actions(id, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                                      WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                                      WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
        wl_data_offer_accept(id, serial, wl->dnd_mime_type);
        MP_VERBOSE(wl, "Accepting DND offer with mime type %s\n", wl->dnd_mime_type);
    }

}

static void data_device_handle_leave(void *data, struct wl_data_device *wl_ddev)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    if (wl->dnd_offer) {
        if (wl->dnd_fd != -1)
            return;
        wl_data_offer_destroy(wl->dnd_offer);
        wl->dnd_offer = NULL;
    }

    if (wl->vo_opts->drag_and_drop != -2) {
        MP_VERBOSE(wl, "Releasing DND offer with mime type %s\n", wl->dnd_mime_type);
        if (wl->dnd_mime_type)
            TA_FREEP(&wl->dnd_mime_type);
        wl->dnd_mime_score = 0;
    }
}

static void data_device_handle_motion(void *data, struct wl_data_device *wl_ddev,
                                      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;
    wl_data_offer_accept(wl->dnd_offer, time, wl->dnd_mime_type);
}

static void data_device_handle_drop(void *data, struct wl_data_device *wl_ddev)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) == -1) {
        MP_ERR(wl, "Failed to create dnd pipe!\n");
        return;
    }

    if (wl->vo_opts->drag_and_drop != -2) {
        MP_VERBOSE(wl, "Receiving DND offer with mime %s\n", wl->dnd_mime_type);
        wl_data_offer_receive(wl->dnd_offer, wl->dnd_mime_type, pipefd[1]);
    }

    close(pipefd[1]);
    wl->dnd_fd = pipefd[0];
}

static void data_device_handle_selection(void *data, struct wl_data_device *wl_ddev,
                                         struct wl_data_offer *id)
{
    struct vo_wayland_seat *s = data;
    struct vo_wayland_state *wl = s->wl;

    if (wl->dnd_offer) {
        wl_data_offer_destroy(wl->dnd_offer);
        wl->dnd_offer = NULL;
        MP_VERBOSE(wl, "Received a new DND offer. Releasing the previous offer.\n");
    }

}

static const struct wl_data_device_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_enter,
    data_device_handle_leave,
    data_device_handle_motion,
    data_device_handle_drop,
    data_device_handle_selection,
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

    MP_VERBOSE(o->wl, "Registered output %s %s (0x%x):\n"
               "\tx: %dpx, y: %dpx\n"
               "\tw: %dpx (%dmm), h: %dpx (%dmm)\n"
               "\tscale: %d\n"
               "\tHz: %f\n", o->make, o->model, o->id, o->geometry.x0,
               o->geometry.y0, mp_rect_w(o->geometry), o->phys_width,
               mp_rect_h(o->geometry), o->phys_height, o->scale, o->refresh_rate);

    /* If we satisfy this conditional, something about the current
     * output must have changed (resolution, scale, etc). All window
     * geometry and scaling should be recalculated. */
    if (wl->current_output && wl->current_output->output == wl_output) {
        set_surface_scaling(wl);
        spawn_cursor(wl);
        set_geometry(wl, false);
        prepare_resize(wl, 0, 0);
        wl->pending_vo_events |= VO_EVENT_DPI;
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
    output->scale = factor;
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

static void surface_invalidation_handle_invalidated(void *data,
                                                    struct wp_surface_invalidation_v1 *wp_surface_invalidation_v1,
                                                    uint32_t serial)
{
	struct vo_wayland_state *wl = data;

	wp_surface_invalidation_v1_ack(wp_surface_invalidation_v1, serial);
    MP_WARN(wl, "Surface invalidated, attempting to reinitialize\n");
    wl->vo->wants_reinit = true;
}

static struct wp_surface_invalidation_v1_listener surface_invalidation_listener = {
	.invalidated = surface_invalidation_handle_invalidated,
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

    struct vo_wayland_output *o;
    wl_list_for_each(o, &wl->output_list, link) {
        if (o->output == output) {
            wl->current_output = o;
            break;
        }
    }

    wl->current_output->has_surface = true;
    bool force_resize = false;

    if (!wl->fractional_scale_manager && wl_surface_get_version(wl_surface) < 6 &&
        wl->scaling != wl->current_output->scale)
    {
        set_surface_scaling(wl);
        spawn_cursor(wl);
        force_resize = true;
        wl->pending_vo_events |= VO_EVENT_DPI;
    }

    if (!mp_rect_equals(&old_output_geometry, &wl->current_output->geometry)) {
        set_geometry(wl, false);
        force_resize = true;
    }

    if (!mp_rect_equals(&old_geometry, &wl->geometry) || force_resize)
        prepare_resize(wl, 0, 0);

    MP_VERBOSE(wl, "Surface entered output %s %s (0x%x), scale = %f, refresh rate = %f Hz\n",
               o->make, o->model, o->id, wl->scaling, o->refresh_rate);

    wl->pending_vo_events |= VO_EVENT_WIN_STATE;
}

static void surface_handle_leave(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *output)
{
    struct vo_wayland_state *wl = data;

    struct vo_wayland_output *o;
    wl_list_for_each(o, &wl->output_list, link) {
        if (o->output == output)
            o->has_surface = false;
        if (o->output != output && o->has_surface)
            wl->current_output = o;
    }
    wl->pending_vo_events |= VO_EVENT_WIN_STATE;
}

#ifdef HAVE_WAYLAND_1_22
static void surface_handle_preferred_buffer_scale(void *data,
                                                  struct wl_surface *wl_surface,
                                                  int32_t scale)
{
    struct vo_wayland_state *wl = data;
    double old_scale = wl->scaling;

    if (wl->fractional_scale_manager)
        return;

    wl->scaling = scale;
    MP_VERBOSE(wl, "Obtained preferred scale, %f, from the compositor.\n",
               wl->scaling);
    wl->pending_vo_events |= VO_EVENT_DPI;
    if (wl->current_output) {
        rescale_geometry(wl, old_scale);
        set_geometry(wl, false);
        prepare_resize(wl, 0, 0);
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
    struct mp_vo_opts *vo_opts = wl->vo_opts;
    struct mp_rect old_geometry = wl->geometry;

    if (width < 0 || height < 0) {
        MP_WARN(wl, "Compositor sent negative width/height values. Treating them as zero.\n");
        width = height = 0;
    }

    int old_toplevel_width = wl->toplevel_width;
    int old_toplevel_height = wl->toplevel_height;
    wl->toplevel_width = width;
    wl->toplevel_height = height;

    if (!wl->configured) {
        /* Save initial window size if the compositor gives us a hint here. */
        bool autofit_or_geometry = vo_opts->geometry.wh_valid || vo_opts->autofit.wh_valid ||
                                   vo_opts->autofit_larger.wh_valid || vo_opts->autofit_smaller.wh_valid;
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
    bool is_suspended = false;
    bool is_tiled = false;
    enum xdg_toplevel_state *state;
    wl_array_for_each(state, states) {
        switch (*state) {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            is_fullscreen = true;
            break;
        case XDG_TOPLEVEL_STATE_RESIZING:
            break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
            is_activated = true;
            /*
             * If we get an ACTIVATED state, we know it cannot be
             * minimized, but it may not have been minimized
             * previously, so we can't detect the exact state.
             */
            vo_opts->window_minimized = false;
            m_config_cache_write_opt(wl->vo_opts_cache,
                                     &vo_opts->window_minimized);
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

    if (vo_opts->fullscreen != is_fullscreen) {
        wl->state_change = true;
        vo_opts->fullscreen = is_fullscreen;
        m_config_cache_write_opt(wl->vo_opts_cache, &vo_opts->fullscreen);
    }

    if (vo_opts->window_maximized != is_maximized) {
        wl->state_change = true;
        vo_opts->window_maximized = is_maximized;
        m_config_cache_write_opt(wl->vo_opts_cache, &vo_opts->window_maximized);
    }

    wl->tiled = is_tiled;

    wl->locked_size = is_fullscreen || is_maximized || is_tiled;

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

    if (old_toplevel_width == wl->toplevel_width &&
        old_toplevel_height == wl->toplevel_height)
        return;

    if (!wl->locked_size) {
        if (vo_opts->keepaspect) {
            double scale_factor = (double)width / wl->reduced_width;
            width = ceil(wl->reduced_width * scale_factor);
            if (vo_opts->keepaspect_window)
                height = ceil(wl->reduced_height * scale_factor);
        }
        wl->window_size.x0 = 0;
        wl->window_size.y0 = 0;
        wl->window_size.x1 = lround(width * wl->scaling);
        wl->window_size.y1 = lround(height * wl->scaling);
    }
    wl->geometry.x0 = 0;
    wl->geometry.y0 = 0;
    wl->geometry.x1 = lround(width * wl->scaling);
    wl->geometry.y1 = lround(height * wl->scaling);

    if (mp_rect_equals(&old_geometry, &wl->geometry))
        return;

resize:
    MP_VERBOSE(wl, "Resizing due to xdg from %ix%i to %ix%i\n",
               mp_rect_w(old_geometry), mp_rect_h(old_geometry),
               mp_rect_w(wl->geometry), mp_rect_h(wl->geometry));

    prepare_resize(wl, width, height);
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
    wl->bounded_width = width * wl->scaling;
    wl->bounded_height = height * wl->scaling;
}

#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
static void handle_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel,
                                   struct wl_array *capabilities)
{
}
#endif

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_toplevel_config,
    handle_toplevel_close,
    handle_configure_bounds,
#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
    handle_wm_capabilities,
#endif
};

#if HAVE_WAYLAND_PROTOCOLS_1_31
static void preferred_scale(void *data,
                            struct wp_fractional_scale_v1 *fractional_scale,
                            uint32_t scale)
{
    struct vo_wayland_state *wl = data;
    double old_scale = wl->scaling;

    wl->scaling = (double)scale / 120;
    MP_VERBOSE(wl, "Obtained preferred scale, %f, from the compositor.\n",
               wl->scaling);
    wl->pending_vo_events |= VO_EVENT_DPI;
    if (wl->current_output) {
        rescale_geometry(wl, old_scale);
        set_geometry(wl, false);
        prepare_resize(wl, 0, 0);
    }
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    preferred_scale,
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
    struct mp_vo_opts *opts = wl->vo_opts;

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
    m_config_cache_write_opt(wl->vo_opts_cache, &opts->border);
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
    configure_decorations,
};

static void pres_set_clockid(void *data, struct wp_presentation *pres,
                             uint32_t clockid)
{
    struct vo_wayland_state *wl = data;

    if (clockid == CLOCK_MONOTONIC || clockid == CLOCK_MONOTONIC_RAW)
        wl->use_present = true;
}

static const struct wp_presentation_listener pres_listener = {
    pres_set_clockid,
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

    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (map != MAP_FAILED) {
        wl->format_map = map;
        wl->format_size = size;
    }
}

static void main_device(void *data,
                        struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                        struct wl_array *device)
{
}

static void tranche_done(void *data,
                         struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
}

static void tranche_target_device(void *data,
                                  struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                  struct wl_array *device)
{
}

static void tranche_formats(void *data,
                            struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                            struct wl_array *indices)
{
}

static void tranche_flags(void *data,
                          struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                          uint32_t flags)
{
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

        /* never accept input events on anything besides the main surface */
        struct wl_region *region = wl_compositor_create_region(wl->compositor);
        wl_surface_set_input_region(wl->osd_surface, region);
        wl_surface_set_input_region(wl->video_surface, region);
        wl_region_destroy(region);

        wl->cursor_surface = wl_compositor_create_surface(wl->compositor);
        wl_surface_add_listener(wl->surface, &surface_listener, wl);
    }

    if (!strcmp(interface, wl_subcompositor_interface.name) && (ver >= 1) && found++) {
        wl->subcompositor = wl_registry_bind(reg, id, &wl_subcompositor_interface, 1);
    }

    if (!strcmp (interface, zwp_linux_dmabuf_v1_interface.name) && (ver >= 4) && found++) {
        wl->dmabuf = wl_registry_bind(reg, id, &zwp_linux_dmabuf_v1_interface, 4);
        wl->dmabuf_feedback = zwp_linux_dmabuf_v1_get_default_feedback(wl->dmabuf);
        zwp_linux_dmabuf_feedback_v1_add_listener(wl->dmabuf_feedback, &dmabuf_feedback_listener, wl);
    }

    if (!strcmp (interface, wp_viewporter_interface.name) && (ver >= 1) && found++) {
        wl->viewporter = wl_registry_bind (reg, id, &wp_viewporter_interface, 1);
    }

    if (!strcmp(interface, wl_data_device_manager_interface.name) && (ver >= 3) && found++) {
        wl->dnd_devman = wl_registry_bind(reg, id, &wl_data_device_manager_interface, 3);
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
#ifdef HAVE_WAYLAND_1_21
        ver = MPMIN(ver, 8); /* Cap at 8 in case new events are added later. */
#else
        ver = MPMIN(ver, 7);
#endif
        struct vo_wayland_seat *seat = talloc_zero(wl, struct vo_wayland_seat);
        seat->wl   = wl;
        seat->id   = id;
        seat->seat = wl_registry_bind(reg, id, &wl_seat_interface, ver);
        wl_seat_add_listener(seat->seat, &seat_listener, seat);
        if (wl->dnd_devman) {
            seat->dnd_ddev = wl_data_device_manager_get_data_device(wl->dnd_devman, seat->seat);
            wl_data_device_add_listener(seat->dnd_ddev, &data_device_listener, seat);
        }
        wl_list_insert(&wl->seat_list, &seat->link);
    }

    if (!strcmp(interface, wl_shm_interface.name) && found++) {
        wl->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
    }

    if (!strcmp(interface, wp_surface_invalidation_manager_v1_interface.name) && found++) {
        wl->surface_invalidation_manager = wl_registry_bind(reg, id, &wp_surface_invalidation_manager_v1_interface, 1);
        wl->surface_invalidation = wp_surface_invalidation_manager_v1_get_surface_invalidation(wl->surface_invalidation_manager, wl->surface);
        wp_surface_invalidation_v1_add_listener(wl->surface_invalidation, &surface_invalidation_listener, wl);
	}

#if HAVE_WAYLAND_PROTOCOLS_1_27
    if (!strcmp(interface, wp_content_type_manager_v1_interface.name) && found++) {
        wl->content_type_manager = wl_registry_bind(reg, id, &wp_content_type_manager_v1_interface, 1);
    }

    if (!strcmp(interface, wp_single_pixel_buffer_manager_v1_interface.name) && found++) {
        wl->single_pixel_manager = wl_registry_bind(reg, id, &wp_single_pixel_buffer_manager_v1_interface, 1);
    }
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_31
    if (!strcmp(interface, wp_fractional_scale_manager_v1_interface.name) && found++) {
        wl->fractional_scale_manager = wl_registry_bind(reg, id, &wp_fractional_scale_manager_v1_interface, 1);
    }
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (!strcmp(interface, wp_cursor_shape_manager_v1_interface.name) && found++) {
        wl->cursor_shape_manager = wl_registry_bind(reg, id, &wp_cursor_shape_manager_v1_interface, 1);
    }
#endif

    if (!strcmp(interface, wp_presentation_interface.name) && found++) {
        wl->presentation = wl_registry_bind(reg, id, &wp_presentation_interface, 1);
        wp_presentation_add_listener(wl->presentation, &pres_listener, wl);
    }

    if (!strcmp(interface, xdg_wm_base_interface.name) && found++) {
        ver = MPMIN(ver, 6); /* Cap at 6 in case new events are added later. */
        wl->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, ver);
        xdg_wm_base_add_listener(wl->wm_base, &xdg_wm_base_listener, wl);
    }

    if (!strcmp(interface, zxdg_decoration_manager_v1_interface.name) && found++) {
        wl->xdg_decoration_manager = wl_registry_bind(reg, id, &zxdg_decoration_manager_v1_interface, 1);
    }

    if (!strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) && found++) {
        wl->idle_inhibit_manager = wl_registry_bind(reg, id, &zwp_idle_inhibit_manager_v1_interface, 1);
    }

    if (found > 1)
        MP_VERBOSE(wl, "Registered for protocol %s\n", interface);
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
static void check_dnd_fd(struct vo_wayland_state *wl)
{
    if (wl->dnd_fd == -1)
        return;

    struct pollfd fdp = { wl->dnd_fd, POLLIN | POLLHUP, 0 };
    if (poll(&fdp, 1, 0) <= 0)
        return;

    if (fdp.revents & POLLIN) {
        size_t data_read = 0;
        const size_t chunk_size = 1;
        bstr file_list = {
            .start = talloc_zero_size(NULL, chunk_size),
        };

        while ((data_read = read(wl->dnd_fd, file_list.start + file_list.len, chunk_size)) > 0) {
            file_list.len += data_read;
            file_list.start = talloc_realloc_size(NULL, file_list.start, file_list.len + chunk_size);
            memset(file_list.start + file_list.len, 0, chunk_size);
        }

        MP_VERBOSE(wl, "Read %zu bytes from the DND fd\n", file_list.len);

        mp_event_drop_mime_data(wl->vo->input_ctx, wl->dnd_mime_type,
                                file_list, wl->dnd_action);
        talloc_free(file_list.start);
        if (wl->dnd_mime_type)
            talloc_free(wl->dnd_mime_type);

        if (wl->dnd_action >= 0 && wl->dnd_offer)
            wl_data_offer_finish(wl->dnd_offer);

        wl->dnd_action = -1;
        wl->dnd_mime_type = NULL;
        wl->dnd_mime_score = 0;
    }

    if (fdp.revents & (POLLIN | POLLERR | POLLHUP)) {
        close(wl->dnd_fd);
        wl->dnd_fd = -1;
    }
}

static int check_for_resize(struct vo_wayland_state *wl, int edge_pixels,
                            enum xdg_toplevel_resize_edge *edge)
{
    if (wl->vo_opts->fullscreen || wl->vo_opts->window_maximized)
        return 0;

    int pos[2] = { wl->mouse_x, wl->mouse_y };
    int left_edge   = pos[0] < edge_pixels;
    int top_edge    = pos[1] < edge_pixels;
    int right_edge  = pos[0] > (mp_rect_w(wl->geometry) - edge_pixels);
    int bottom_edge = pos[1] > (mp_rect_h(wl->geometry) - edge_pixels);

    if (left_edge) {
        *edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
        if (top_edge)
            *edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
        else if (bottom_edge)
            *edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
    } else if (right_edge) {
        *edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
        if (top_edge)
            *edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
        else if (bottom_edge)
            *edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
    } else if (top_edge) {
        *edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    } else if (bottom_edge) {
        *edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    } else {
        *edge = 0;
        return 0;
    }

    return 1;
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

static int create_viewports(struct vo_wayland_state *wl)
{
    wl->viewport = wp_viewporter_get_viewport(wl->viewporter, wl->surface);
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
    if (wl->vo_opts->window_minimized)
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
    struct mp_vo_opts *opts = wl->vo_opts;
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

    return mpkey;
}

static void prepare_resize(struct vo_wayland_state *wl, int width, int height)
{
    if (!width)
        width = mp_rect_w(wl->geometry) / wl->scaling;
    if (!height)
        height = mp_rect_h(wl->geometry) / wl->scaling;
    xdg_surface_set_window_geometry(wl->xdg_surface, 0, 0, width, height);
    wl->pending_vo_events |= VO_EVENT_RESIZE;
}

static void request_decoration_mode(struct vo_wayland_state *wl, uint32_t mode)
{
    wl->requested_decoration = mode;
    zxdg_toplevel_decoration_v1_set_mode(wl->xdg_toplevel_decoration, mode);
}

static void rescale_geometry(struct vo_wayland_state *wl, double old_scale)
{
    if (!wl->vo_opts->hidpi_window_scale)
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
    return;
}

static void remove_seat(struct vo_wayland_seat *seat)
{
    if (!seat)
        return;

    MP_VERBOSE(seat->wl, "Deregistering seat 0x%x\n", seat->id);
    wl_list_remove(&seat->link);
    if (seat->keyboard)
        wl_keyboard_destroy(seat->keyboard);
    if (seat->pointer)
        wl_pointer_destroy(seat->pointer);
    if (seat->touch)
        wl_touch_destroy(seat->touch);
    if (seat->dnd_ddev)
        wl_data_device_destroy(seat->dnd_ddev);
#if HAVE_WAYLAND_PROTOCOLS_1_32
    if (seat->cursor_shape_device)
        wp_cursor_shape_device_v1_destroy(seat->cursor_shape_device);
#endif
    if (seat->xkb_keymap)
        xkb_keymap_unref(seat->xkb_keymap);
    if (seat->xkb_state)
        xkb_state_unref(seat->xkb_state);

    wl_seat_destroy(seat->seat);
    talloc_free(seat);
    return;
}

static void set_content_type(struct vo_wayland_state *wl)
{
    if (!wl->content_type_manager)
        return;
#if HAVE_WAYLAND_PROTOCOLS_1_27
    // handle auto;
    if (wl->vo_opts->content_type == -1) {
        wp_content_type_v1_set_content_type(wl->content_type, wl->current_content_type);
    } else {
        wp_content_type_v1_set_content_type(wl->content_type, wl->vo_opts->content_type);
    }
#endif
}

static void set_cursor_shape(struct vo_wayland_seat *s)
{
#if HAVE_WAYLAND_PROTOCOLS_1_32
    wp_cursor_shape_device_v1_set_shape(s->cursor_shape_device, s->pointer_serial,
                                        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
#endif
}

static int set_cursor_visibility(struct vo_wayland_seat *s, bool on)
{
    if (!s)
        return VO_FALSE;
    struct vo_wayland_state *wl = s->wl;
    wl->cursor_visible = on;
    if (on) {
        if (s->cursor_shape_device) {
            set_cursor_shape(s);
        } else {
            if (spawn_cursor(wl))
                return VO_FALSE;
            struct wl_cursor_image *img = wl->default_cursor->images[0];
            struct wl_buffer *buffer = wl_cursor_image_get_buffer(img);
            if (!buffer)
                return VO_FALSE;
            int scale = MPMAX(wl->scaling, 1);
            wl_pointer_set_cursor(s->pointer, s->pointer_serial, wl->cursor_surface,
                                  img->hotspot_x / scale, img->hotspot_y / scale);
            wl_surface_set_buffer_scale(wl->cursor_surface, scale);
            wl_surface_attach(wl->cursor_surface, buffer, 0, 0);
            wl_surface_damage_buffer(wl->cursor_surface, 0, 0, img->width, img->height);
        }
        wl_surface_commit(wl->cursor_surface);
    } else {
        wl_pointer_set_cursor(s->pointer, s->pointer_serial, NULL, 0, 0);
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
    vo_calc_window_geometry2(vo, &screenrc, wl->scaling, &geo);
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
        prepare_resize(wl, 0, 0);
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
        wl->idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(mgr, wl->surface);
    } else {
        MP_VERBOSE(wl, "Disabling the idle inhibitor\n");
        zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);
        wl->idle_inhibitor = NULL;
    }
    return VO_TRUE;
}

static void set_surface_scaling(struct vo_wayland_state *wl)
{
    if (wl->fractional_scale_manager)
        return;

    double old_scale = wl->scaling;
    wl->scaling = wl->current_output->scale;
    rescale_geometry(wl, old_scale);
}

static void set_window_bounds(struct vo_wayland_state *wl)
{
    // If the user has set geometry/autofit and the option is auto,
    // don't use these.
    if (wl->opts->configure_bounds == -1 && (wl->vo_opts->geometry.wh_valid ||
        wl->vo_opts->autofit.wh_valid || wl->vo_opts->autofit_larger.wh_valid ||
        wl->vo_opts->autofit_smaller.wh_valid))
    {
        return;
    }

    if (wl->bounded_width && wl->bounded_width < wl->window_size.x1)
        wl->window_size.x1 = wl->bounded_width;
    if (wl->bounded_height && wl->bounded_height < wl->window_size.y1)
        wl->window_size.y1 = wl->bounded_height;
}

static int spawn_cursor(struct vo_wayland_state *wl)
{
    if (wl->cursor_shape_manager)
        return 0;
    if (wl->allocated_cursor_scale == wl->scaling)
        return 0;
    else if (wl->cursor_theme)
        wl_cursor_theme_destroy(wl->cursor_theme);

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

    wl->cursor_theme = wl_cursor_theme_load(xcursor_theme, size*wl->scaling, wl->shm);
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
    wl->state_change = true;
    bool specific_screen = wl->vo_opts->fsscreen_id >= 0 || wl->vo_opts->fsscreen_name;
    if (wl->vo_opts->fullscreen && !specific_screen) {
        xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
    } else if (wl->vo_opts->fullscreen && specific_screen) {
        struct vo_wayland_output *output = find_output(wl);
        xdg_toplevel_set_fullscreen(wl->xdg_toplevel, output->output);
    } else {
        xdg_toplevel_unset_fullscreen(wl->xdg_toplevel);
    }
}

static void toggle_maximized(struct vo_wayland_state *wl)
{
    wl->state_change = true;
    if (wl->vo_opts->window_maximized) {
        xdg_toplevel_set_maximized(wl->xdg_toplevel);
    } else {
        xdg_toplevel_unset_maximized(wl->xdg_toplevel);
    }
}

static void update_app_id(struct vo_wayland_state *wl)
{
    xdg_toplevel_set_app_id(wl->xdg_toplevel, wl->vo_opts->appid);
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
    bool render = !wl->hidden || wl->vo_opts->force_render;
    wl->frame_wait = true;
    return render;
}

int vo_wayland_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_wayland_state *wl = vo->wl;
    struct mp_vo_opts *opts = wl->vo_opts;
    wl_display_dispatch_pending(wl->display);

    switch (request) {
    case VOCTRL_CHECK_EVENTS: {
        check_dnd_fd(wl);
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
        while (m_config_cache_get_next_changed(wl->vo_opts_cache, &opt)) {
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
                    m_config_cache_write_opt(wl->vo_opts_cache,
                                             &opts->border);
                    request_decoration_mode(
                        wl, requested_border_mode ?
                            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE :
                            ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                } else {
                    opts->border = false;
                    m_config_cache_write_opt(wl->vo_opts_cache,
                                             &wl->vo_opts->border);
                }
            }
            if (opt == &opts->content_type)
                set_content_type(wl);
            if (opt == &opts->cursor_passthrough)
                set_input_region(wl, opts->cursor_passthrough);
            if (opt == &opts->fullscreen)
                toggle_fullscreen(wl);
            if (opt == &opts->hidpi_window_scale)
                set_geometry(wl, true);
            if (opt == &opts->window_maximized)
                toggle_maximized(wl);
            if (opt == &opts->window_minimized)
                do_minimize(wl);
            if (opt == &opts->geometry || opt == &opts->autofit ||
                opt == &opts->autofit_smaller || opt == &opts->autofit_larger)
            {
                set_geometry(wl, true);
            }
        }
        return VO_TRUE;
    }
    case VOCTRL_CONTENT_TYPE: {
#if HAVE_WAYLAND_PROTOCOLS_1_27
        wl->current_content_type = *(enum mp_content_type *)arg;
        set_content_type(wl);
#endif
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
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        if (wl->vo_opts->window_maximized || wl->tiled) {
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
        if (!wl->vo_opts->fullscreen && !wl->tiled) {
            if (wl->vo_opts->window_maximized) {
                xdg_toplevel_unset_maximized(wl->xdg_toplevel);
                wl_display_dispatch_pending(wl->display);
                /* Make sure the compositor let us unmaximize */
                if (wl->vo_opts->window_maximized)
                    return VO_TRUE;
            }
            wl->geometry = wl->window_size;
            prepare_resize(wl, 0, 0);
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
        if (!wl->scaling)
            return VO_NOTAVAIL;
        *(double *)arg = wl->scaling;
        return VO_TRUE;
    }
    case VOCTRL_UPDATE_WINDOW_TITLE:
        return update_window_title(wl, (const char *)arg);
    case VOCTRL_SET_CURSOR_VISIBILITY:
        return set_cursor_visibility_all_seats(wl, *(bool *)arg);
    case VOCTRL_KILL_SCREENSAVER:
        return set_screensaver_inhibitor(wl, true);
    case VOCTRL_RESTORE_SCREENSAVER:
        return set_screensaver_inhibitor(wl, false);
    }

    return VO_NOTIMPL;
}

void vo_wayland_handle_scale(struct vo_wayland_state *wl)
{
    wp_viewport_set_destination(wl->viewport,
                                lround(mp_rect_w(wl->geometry) / wl->scaling),
                                lround(mp_rect_h(wl->geometry) / wl->scaling));
}

bool vo_wayland_init(struct vo *vo)
{
    vo->wl = talloc_zero(NULL, struct vo_wayland_state);
    struct vo_wayland_state *wl = vo->wl;

    *wl = (struct vo_wayland_state) {
        .display = wl_display_connect(NULL),
        .vo = vo,
        .log = mp_log_new(wl, vo->log, "wayland"),
        .bounded_width = 0,
        .bounded_height = 0,
        .refresh_interval = 0,
        .scaling = 1,
        .wakeup_pipe = {-1, -1},
        .display_fd = -1,
        .dnd_fd = -1,
        .cursor_visible = true,
        .vo_opts_cache = m_config_cache_alloc(wl, vo->global, &vo_sub_opts),
    };
    wl->vo_opts = wl->vo_opts_cache->opts;
    bool using_dmabuf_wayland = !strcmp(wl->vo->driver->name, "dmabuf-wayland");

    wl_list_init(&wl->output_list);
    wl_list_init(&wl->seat_list);

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

    if (!wl_list_length(&wl->output_list)) {
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

    if (wl->subcompositor) {
        wl->osd_subsurface = wl_subcompositor_get_subsurface(wl->subcompositor, wl->osd_surface, wl->video_surface);
        wl->video_subsurface = wl_subcompositor_get_subsurface(wl->subcompositor, wl->video_surface, wl->surface);
    }

#if HAVE_WAYLAND_PROTOCOLS_1_27
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
#endif

#if HAVE_WAYLAND_PROTOCOLS_1_31
    if (wl->fractional_scale_manager) {
        wl->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(wl->fractional_scale_manager, wl->surface);
        wp_fractional_scale_v1_add_listener(wl->fractional_scale, &fractional_scale_listener, wl);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_fractional_scale_manager_v1_interface.name);
    }
#endif

    if (!wl->dnd_devman) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s (ver. 3) protocol!\n",
                   wl_data_device_manager_interface.name);
    }

    if (wl->presentation) {
        wl->fback_pool = talloc_zero(wl, struct vo_wayland_feedback_pool);
        wl->fback_pool->wl = wl;
        wl->fback_pool->len = VO_MAX_SWAPCHAIN_DEPTH;
        wl->fback_pool->fback = talloc_zero_array(wl->fback_pool, struct wp_presentation_feedback *,
                                                  wl->fback_pool->len);
        wl->present = mp_present_initialize(wl, wl->vo_opts, VO_MAX_SWAPCHAIN_DEPTH);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   wp_presentation_interface.name);
    }

    if (wl->xdg_decoration_manager) {
        wl->xdg_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wl->xdg_decoration_manager, wl->xdg_toplevel);
        zxdg_toplevel_decoration_v1_add_listener(wl->xdg_toplevel_decoration, &decoration_listener, wl);
        request_decoration_mode(
            wl, wl->vo_opts->border ?
                ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE :
                ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    } else {
        wl->vo_opts->border = false;
        m_config_cache_write_opt(wl->vo_opts_cache,
                                 &wl->vo_opts->border);
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   zxdg_decoration_manager_v1_interface.name);
    }

    if (!wl->idle_inhibit_manager) {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   zwp_idle_inhibit_manager_v1_interface.name);
    }

    wl->opts = mp_get_config_group(wl, wl->vo->global, &wayland_conf);
    wl->display_fd = wl_display_get_fd(wl->display);

    update_app_id(wl);
    mp_make_wakeup_pipe(wl->wakeup_pipe);

    wl->callback_surface = using_dmabuf_wayland ? wl->video_surface : wl->surface;
    wl->frame_callback = wl_surface_frame(wl->callback_surface);
    wl_callback_add_listener(wl->frame_callback, &frame_listener, wl);
    wl_surface_commit(wl->surface);

    /* Do another roundtrip to ensure all of the above is initialized
     * before mpv does anything else. */
    wl_display_roundtrip(wl->display);

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
        wl->pending_vo_events |= VO_EVENT_DPI;
    }

    if (wl->vo_opts->auto_window_resize || !wl->configured)
        set_geometry(wl, false);

    if (wl->opts->configure_bounds)
        set_window_bounds(wl);

    if (!wl->configured || !wl->locked_size) {
        wl->geometry = wl->window_size;
        wl->configured = true;
    }

    if (wl->vo_opts->cursor_passthrough)
        set_input_region(wl, true);

    if (wl->vo_opts->fullscreen)
        toggle_fullscreen(wl);

    if (wl->vo_opts->window_maximized)
        toggle_maximized(wl);

    if (wl->vo_opts->window_minimized)
        do_minimize(wl);

    prepare_resize(wl, 0, 0);

    return true;
}

void vo_wayland_set_opaque_region(struct vo_wayland_state *wl, bool alpha)
{
    const int32_t width = mp_rect_w(wl->geometry);
    const int32_t height = mp_rect_h(wl->geometry);
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

#if HAVE_WAYLAND_PROTOCOLS_1_27
    if (wl->content_type)
        wp_content_type_v1_destroy(wl->content_type);

    if (wl->content_type_manager)
        wp_content_type_manager_v1_destroy(wl->content_type_manager);
#endif

    if (wl->dnd_devman)
        wl_data_device_manager_destroy(wl->dnd_devman);

    if (wl->dnd_offer)
        wl_data_offer_destroy(wl->dnd_offer);

    if (wl->fback_pool)
        clean_feedback_pool(wl->fback_pool);

#if HAVE_WAYLAND_PROTOCOLS_1_31
    if (wl->fractional_scale)
        wp_fractional_scale_v1_destroy(wl->fractional_scale);

    if (wl->fractional_scale_manager)
        wp_fractional_scale_manager_v1_destroy(wl->fractional_scale_manager);
#endif

    if (wl->frame_callback)
        wl_callback_destroy(wl->frame_callback);

    if (wl->idle_inhibitor)
        zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);

    if (wl->idle_inhibit_manager)
        zwp_idle_inhibit_manager_v1_destroy(wl->idle_inhibit_manager);

    if (wl->surface_invalidation)
        wp_surface_invalidation_v1_destroy(wl->surface_invalidation);

    if (wl->surface_invalidation_manager)
        wp_surface_invalidation_manager_v1_destroy(wl->surface_invalidation_manager);

    if (wl->presentation)
        wp_presentation_destroy(wl->presentation);

    if (wl->registry)
        wl_registry_destroy(wl->registry);

    if (wl->viewporter)
        wp_viewporter_destroy(wl->viewporter);

    if (wl->viewport)
        wp_viewport_destroy(wl->viewport);

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

#if HAVE_WAYLAND_PROTOCOLS_1_27
    if (wl->single_pixel_manager)
        wp_single_pixel_buffer_manager_v1_destroy(wl->single_pixel_manager);
#endif

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

    if (wl->display)
        wl_display_disconnect(wl->display);

    munmap(wl->format_map, wl->format_size);

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
