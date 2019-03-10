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

#include <poll.h>
#include <unistd.h>
#include <linux/input.h>
#include "common/msg.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "osdep/io.h"
#include "osdep/timer.h"
#include "win_state.h"
#include "wayland_common.h"

// Generated from xdg-shell.xml
#include "video/out/wayland/xdg-shell.h"

// Generated from idle-inhibit-unstable-v1.xml
#include "video/out/wayland/idle-inhibit-v1.h"

// Generated from xdg-decoration-unstable-v1.xml
#include "video/out/wayland/xdg-decoration-v1.h"

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping,
};

static int spawn_cursor(struct vo_wayland_state *wl)
{
    if (wl->allocated_cursor_scale == wl->scaling) /* Reuse if size is identical */
        return 0;
    else if (wl->cursor_theme)
        wl_cursor_theme_destroy(wl->cursor_theme);

    wl->cursor_theme = wl_cursor_theme_load(NULL, 32*wl->scaling, wl->shm);
    if (!wl->cursor_theme) {
        MP_ERR(wl, "Unable to load cursor theme!\n");
        return 1;
    }

    wl->default_cursor = wl_cursor_theme_get_cursor(wl->cursor_theme, "left_ptr");
    if (!wl->default_cursor) {
        MP_ERR(wl, "Unable to load cursor theme!\n");
        return 1;
    }

    wl->allocated_cursor_scale = wl->scaling;

    return 0;
}

static int set_cursor_visibility(struct vo_wayland_state *wl, bool on)
{
    if (!wl->pointer)
        return VO_NOTAVAIL;
    if (on) {
        if (spawn_cursor(wl))
            return VO_FALSE;
        struct wl_cursor_image *img = wl->default_cursor->images[0];
        struct wl_buffer *buffer = wl_cursor_image_get_buffer(img);
        if (!buffer)
            return VO_FALSE;
        wl_pointer_set_cursor(wl->pointer, wl->pointer_id, wl->cursor_surface,
                              img->hotspot_x/wl->scaling, img->hotspot_y/wl->scaling);
        wl_surface_set_buffer_scale(wl->cursor_surface, wl->scaling);
        wl_surface_attach(wl->cursor_surface, buffer, 0, 0);
        wl_surface_damage(wl->cursor_surface, 0, 0, img->width, img->height);
        wl_surface_commit(wl->cursor_surface);
    } else {
        wl_pointer_set_cursor(wl->pointer, wl->pointer_id, NULL, 0, 0);
    }
    return VO_TRUE;
}

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy)
{
    struct vo_wayland_state *wl = data;

    wl->pointer    = pointer;
    wl->pointer_id = serial;

    set_cursor_visibility(wl, true);
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_MOUSE_ENTER);
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface)
{
    struct vo_wayland_state *wl = data;
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_MOUSE_LEAVE);
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    struct vo_wayland_state *wl = data;

    wl->mouse_x = wl_fixed_to_int(sx) * wl->scaling;
    wl->mouse_y = wl_fixed_to_int(sy) * wl->scaling;

    mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y);
}

static void window_move(struct vo_wayland_state *wl, uint32_t serial)
{
    if (wl->xdg_toplevel)
        xdg_toplevel_move(wl->xdg_toplevel, wl->seat, serial);
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state)
{
    struct vo_wayland_state *wl = data;

    state = state == WL_POINTER_BUTTON_STATE_PRESSED ? MP_KEY_STATE_DOWN
                                                     : MP_KEY_STATE_UP;

    button = button == BTN_LEFT   ? MP_MBTN_LEFT :
             button == BTN_MIDDLE ? MP_MBTN_MID  : MP_MBTN_RIGHT;

    mp_input_put_key(wl->vo->input_ctx, button | state);

    if (!mp_input_test_dragging(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y) &&
        (button == MP_MBTN_LEFT) && (state == MP_KEY_STATE_DOWN))
        window_move(wl, serial);
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value)
{
    struct vo_wayland_state *wl = data;
    double val = wl_fixed_to_double(value)*0.1;
    switch (axis) {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        if (value > 0)
            mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_DOWN,  +val);
        if (value < 0)
            mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_UP,    -val);
        break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        if (value > 0)
            mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_RIGHT, +val);
        if (value < 0)
            mp_input_put_wheel(wl->vo->input_ctx, MP_WHEEL_LEFT,  -val);
        break;
    }
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static int check_for_resize(struct vo_wayland_state *wl, wl_fixed_t x_w, wl_fixed_t y_w,
                            enum xdg_toplevel_resize_edge *edge)
{
    if (wl->touch_entries || wl->fullscreen || wl->maximized)
        return 0;

    const int edge_pixels = 64;
    int pos[2] = { wl_fixed_to_double(x_w), wl_fixed_to_double(y_w) };
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

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
                              uint32_t serial, uint32_t time, struct wl_surface *surface,
                              int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct vo_wayland_state *wl = data;

    enum xdg_toplevel_resize_edge edge;
    if (check_for_resize(wl, x_w, y_w, &edge)) {
        wl->touch_entries = 0;
        xdg_toplevel_resize(wl->xdg_toplevel, wl->seat, serial, edge);
        return;
    } else if (wl->touch_entries) {
        wl->touch_entries = 0;
        xdg_toplevel_move(wl->xdg_toplevel, wl->seat, serial);
        return;
    }

    wl->touch_entries = 1;

    wl->mouse_x = wl_fixed_to_int(x_w) * wl->scaling;
    wl->mouse_y = wl_fixed_to_int(y_w) * wl->scaling;

    mp_input_set_mouse_pos(wl->vo->input_ctx, wl->mouse_x, wl->mouse_y);
    mp_input_put_key(wl->vo->input_ctx, MP_MBTN_LEFT | MP_KEY_STATE_DOWN);
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
                            uint32_t serial, uint32_t time, int32_t id)
{
    struct vo_wayland_state *wl = data;

    wl->touch_entries = 0;

    mp_input_put_key(wl->vo->input_ctx, MP_MBTN_LEFT | MP_KEY_STATE_UP);
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
                                uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct vo_wayland_state *wl = data;

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

static const struct wl_touch_listener touch_listener = {
    touch_handle_down,
    touch_handle_up,
    touch_handle_motion,
    touch_handle_frame,
    touch_handle_cancel,
};

static const struct mp_keymap keymap[] = {
    /* Special keys */
    {XKB_KEY_Pause,     MP_KEY_PAUSE}, {XKB_KEY_Escape, MP_KEY_ESC},
    {XKB_KEY_BackSpace, MP_KEY_BS},    {XKB_KEY_Tab,    MP_KEY_TAB},
    {XKB_KEY_Return,    MP_KEY_ENTER}, {XKB_KEY_Menu,   MP_KEY_MENU},
    {XKB_KEY_Print,     MP_KEY_PRINT},

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
    {XKB_KEY_KP_Insert, MP_KEY_KPINS}, {XKB_KEY_KP_End,       MP_KEY_KP1},
    {XKB_KEY_KP_Down,   MP_KEY_KP2},   {XKB_KEY_KP_Page_Down, MP_KEY_KP3},
    {XKB_KEY_KP_Left,   MP_KEY_KP4},   {XKB_KEY_KP_Begin,     MP_KEY_KP5},
    {XKB_KEY_KP_Right,  MP_KEY_KP6},   {XKB_KEY_KP_Home,      MP_KEY_KP7},
    {XKB_KEY_KP_Up,     MP_KEY_KP8},   {XKB_KEY_KP_Page_Up,   MP_KEY_KP9},
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

    {0, 0}
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t format, int32_t fd, uint32_t size)
{
    struct vo_wayland_state *wl = data;
    char *map_str;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    wl->xkb_keymap = xkb_keymap_new_from_string(wl->xkb_context, map_str,
                                                XKB_KEYMAP_FORMAT_TEXT_V1, 0);

    munmap(map_str, size);
    close(fd);

    if (!wl->xkb_keymap) {
        MP_ERR(wl, "failed to compile keymap\n");
        return;
    }

    wl->xkb_state = xkb_state_new(wl->xkb_keymap);
    if (!wl->xkb_state) {
        MP_ERR(wl, "failed to create XKB state\n");
        xkb_keymap_unref(wl->xkb_keymap);
        wl->xkb_keymap = NULL;
        return;
    }
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys)
{
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface)
{
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

static void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state)
{
    struct vo_wayland_state *wl = data;

    uint32_t code = code = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(wl->xkb_state, code);

    int mpmod = state == WL_KEYBOARD_KEY_STATE_PRESSED ? MP_KEY_STATE_DOWN
                                                       : MP_KEY_STATE_UP;

    static const char *mod_names[] = {
        XKB_MOD_NAME_SHIFT,
        XKB_MOD_NAME_CTRL,
        XKB_MOD_NAME_ALT,
        XKB_MOD_NAME_LOGO,
        0,
    };

    static int mods[] = {
        MP_KEY_MODIFIER_SHIFT,
        MP_KEY_MODIFIER_CTRL,
        MP_KEY_MODIFIER_ALT,
        MP_KEY_MODIFIER_META,
        0,
    };

    for (int n = 0; mods[n]; n++) {
        xkb_mod_index_t index = xkb_keymap_mod_get_index(wl->xkb_keymap, mod_names[n]);
        if (!xkb_state_mod_index_is_consumed(wl->xkb_state, code, index)
            && xkb_state_mod_index_is_active(wl->xkb_state, index,
                                             XKB_STATE_MODS_DEPRESSED))
            mpmod |= mods[n];
    }

    int mpkey = lookupkey(sym);
    if (mpkey) {
        mp_input_put_key(wl->vo->input_ctx, mpkey | mpmod);
    } else {
        char s[128];
        if (xkb_keysym_to_utf8(sym, s, sizeof(s)) > 0)
            mp_input_put_key_utf8(wl->vo->input_ctx, mpmod, bstr0(s));
    }
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group)
{
    struct vo_wayland_state *wl = data;

    xkb_state_update_mask(wl->xkb_state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                        int32_t rate, int32_t delay)
{
    struct vo_wayland_state *wl = data;
    if (wl->vo->opts->native_keyrepeat)
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
    struct vo_wayland_state *wl = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl->pointer) {
        wl->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(wl->pointer, &pointer_listener, wl);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl->pointer) {
        wl_pointer_destroy(wl->pointer);
        wl->pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->keyboard) {
        wl->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(wl->keyboard, &keyboard_listener, wl);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->keyboard) {
        wl_keyboard_destroy(wl->keyboard);
        wl->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !wl->touch) {
        wl->touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(wl->touch, wl);
        wl_touch_add_listener(wl->touch, &touch_listener, wl);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && wl->touch) {
        wl_touch_destroy(wl->touch);
        wl->touch = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_caps,
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

static void output_handle_done(void* data, struct wl_output *wl_output)
{
    struct vo_wayland_output *o = data;

    o->geometry.x1 += o->geometry.x0;
    o->geometry.y1 += o->geometry.y0;

    MP_VERBOSE(o->wl, "Registered output %s %s (0x%x):\n"
               "\tx: %dpx, y: %dpx\n"
               "\tw: %dpx (%dmm), h: %dpx (%dmm)\n"
               "\tscale: %d\n"
               "\tHz: %f\n", o->make, o->model, o->id, o->geometry.x0,
               o->geometry.y0, mp_rect_w(o->geometry), o->phys_width,
               mp_rect_h(o->geometry), o->phys_height, o->scale, o->refresh_rate);
}

static void output_handle_scale(void* data, struct wl_output *wl_output,
                                int32_t factor)
{
    struct vo_wayland_output *output = data;
    if (!factor) {
        MP_ERR(output->wl, "Invalid output scale given by the compositor!\n");
        return;
    }
    output->scale = factor;
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
    output_handle_done,
    output_handle_scale,
};

static void data_offer_handle_offer(void *data, struct wl_data_offer *offer,
                                    const char *mime_type)
{
    struct vo_wayland_state *wl = data;
    int score = mp_event_get_mime_type_score(wl->vo->input_ctx, mime_type);
    if (score > wl->dnd_mime_score) {
        wl->dnd_mime_score = score;
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
    struct vo_wayland_state *wl = data;
    wl->dnd_action = dnd_action & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY ?
                     DND_REPLACE : DND_APPEND;
    MP_VERBOSE(wl, "DND action is %s\n",
               wl->dnd_action == DND_REPLACE ? "DND_REPLACE" : "DND_APPEND");
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_handle_offer,
    data_offer_source_actions,
    data_offer_action,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *wl_ddev,
                                          struct wl_data_offer *id)
{
    struct vo_wayland_state *wl = data;
    if (wl->dnd_offer)
        wl_data_offer_destroy(wl->dnd_offer);

    wl->dnd_offer = id;
    wl_data_offer_add_listener(id, &data_offer_listener, wl);
}

static void data_device_handle_enter(void *data, struct wl_data_device *wl_ddev,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y,
                                     struct wl_data_offer *id)
{
    struct vo_wayland_state *wl = data;
    if (wl->dnd_offer != id) {
        MP_FATAL(wl, "DND offer ID mismatch!\n");
        return;
    }

    wl_data_offer_set_actions(id, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                                  WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                                  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);

    wl_data_offer_accept(id, serial, wl->dnd_mime_type);

    MP_VERBOSE(wl, "Accepting DND offer with mime type %s\n", wl->dnd_mime_type);
}

static void data_device_handle_leave(void *data, struct wl_data_device *wl_ddev)
{
    struct vo_wayland_state *wl = data;

    if (wl->dnd_offer) {
        if (wl->dnd_fd != -1)
            return;
        wl_data_offer_destroy(wl->dnd_offer);
        wl->dnd_offer = NULL;
    }

    MP_VERBOSE(wl, "Releasing DND offer with mime type %s\n", wl->dnd_mime_type);

    talloc_free(wl->dnd_mime_type);
    wl->dnd_mime_type = NULL;
    wl->dnd_mime_score = 0;
}

static void data_device_handle_motion(void *data, struct wl_data_device *wl_ddev,
                                      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    struct vo_wayland_state *wl = data;

    wl_data_offer_accept(wl->dnd_offer, time, wl->dnd_mime_type);
}

static void data_device_handle_drop(void *data, struct wl_data_device *wl_ddev)
{
    struct vo_wayland_state *wl = data;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) == -1) {
        MP_ERR(wl, "Failed to create dnd pipe!\n");
        return;
    }

    MP_VERBOSE(wl, "Receiving DND offer with mime %s\n", wl->dnd_mime_type);

    wl_data_offer_receive(wl->dnd_offer, wl->dnd_mime_type, pipefd[1]);
    close(pipefd[1]);

    wl->dnd_fd = pipefd[0];
}

static void data_device_handle_selection(void *data, struct wl_data_device *wl_ddev,
                                         struct wl_data_offer *id)
{
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_enter,
    data_device_handle_leave,
    data_device_handle_motion,
    data_device_handle_drop,
    data_device_handle_selection,
};

static void surface_handle_enter(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *output)
{
    struct vo_wayland_state *wl = data;
    wl->current_output = NULL;

    struct vo_wayland_output *o;
    wl_list_for_each(o, &wl->output_list, link) {
        if (o->output == output) {
            wl->current_output = o;
            break;
        }
    }

    wl->current_output->has_surface = true;
    if (wl->scaling != wl->current_output->scale)
        wl->pending_vo_events |= VO_EVENT_RESIZE;
    wl->scaling = wl->current_output->scale;

    MP_VERBOSE(wl, "Surface entered output %s %s (0x%x), scale = %i\n", o->make,
               o->model, o->id, wl->scaling);

    wl->pending_vo_events |= VO_EVENT_WIN_STATE;
}

static void surface_handle_leave(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *output)
{
    struct vo_wayland_state *wl = data;

    struct vo_wayland_output *o;
    wl_list_for_each(o, &wl->output_list, link) {
        if (o->output == output) {
            o->has_surface = false;
            wl->pending_vo_events |= VO_EVENT_WIN_STATE;
            return;
        }
    }
}

static const struct wl_surface_listener surface_listener = {
    surface_handle_enter,
    surface_handle_leave,
};

static const struct wl_callback_listener frame_listener;

static void frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    struct vo_wayland_state *wl = data;

    if (callback)
        wl_callback_destroy(callback);

    wl->frame_callback = wl_surface_frame(wl->surface);
    wl_callback_add_listener(wl->frame_callback, &frame_listener, wl);

    if (!vo_render_frame_external(wl->vo))
        wl_surface_commit(wl->surface);
}

static const struct wl_callback_listener frame_listener = {
    frame_callback,
};

static void registry_handle_add(void *data, struct wl_registry *reg, uint32_t id,
                                const char *interface, uint32_t ver)
{
    int found = 1;
    struct vo_wayland_state *wl = data;

    if (!strcmp(interface, wl_compositor_interface.name) && (ver >= 3) && found++) {
        wl->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 3);
        wl->surface = wl_compositor_create_surface(wl->compositor);
        wl->cursor_surface = wl_compositor_create_surface(wl->compositor);
        wl_surface_add_listener(wl->surface, &surface_listener, wl);
        vo_enable_external_renderloop(wl->vo);
        wl->frame_callback = wl_surface_frame(wl->surface);
        wl_callback_add_listener(wl->frame_callback, &frame_listener, wl);
    }

    if (!strcmp(interface, wl_output_interface.name) && (ver >= 2) && found++) {
        struct vo_wayland_output *output = talloc_zero(wl, struct vo_wayland_output);

        output->wl     = wl;
        output->id     = id;
        output->scale  = 1;
        output->output = wl_registry_bind(reg, id, &wl_output_interface, 2);

        wl_output_add_listener(output->output, &output_listener, output);
        wl_list_insert(&wl->output_list, &output->link);
    }

    if (!strcmp(interface, xdg_wm_base_interface.name) && found++) {
        ver = MPMIN(ver, 2); /* We can use either 1 or 2 */
        wl->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, ver);
        xdg_wm_base_add_listener(wl->wm_base, &xdg_wm_base_listener, wl);
    }

    if (!strcmp(interface, wl_seat_interface.name) && found++) {
        wl->seat = wl_registry_bind(reg, id, &wl_seat_interface, 1);
        wl_seat_add_listener(wl->seat, &seat_listener, wl);
    }

    if (!strcmp(interface, wl_shm_interface.name) && found++) {
        wl->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
    }

    if (!strcmp(interface, wl_data_device_manager_interface.name) && (ver >= 3) && found++) {
        wl->dnd_devman = wl_registry_bind(reg, id, &wl_data_device_manager_interface, 3);
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

static void remove_output(struct vo_wayland_output *out)
{
    if (!out)
        return;

    MP_VERBOSE(out->wl, "Deregistering output %s %s (0x%x)\n", out->make,
               out->model, out->id);
    wl_list_remove(&out->link);
    talloc_free(out->make);
    talloc_free(out->model);
    talloc_free(out);
    return;
}

static void registry_handle_remove(void *data, struct wl_registry *reg, uint32_t id)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &wl->output_list, link) {
        if (output->id == id) {
            remove_output(output);
            return;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_add,
    registry_handle_remove,
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
    struct mp_rect old_geometry = wl->geometry;

    int prev_fs_state = wl->fullscreen;
    wl->maximized = false;
    wl->fullscreen = false;
    enum xdg_toplevel_state *state;
    wl_array_for_each(state, states) {
        switch (*state) {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            wl->fullscreen = true;
            break;
        case XDG_TOPLEVEL_STATE_RESIZING:
            wl->pending_vo_events |= VO_EVENT_LIVE_RESIZING;
            break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
            break;
        case XDG_TOPLEVEL_STATE_TILED_TOP:
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            wl->maximized = true;
            break;
        }
    }

    if (prev_fs_state != wl->fullscreen)
        wl->pending_vo_events |= VO_EVENT_FULLSCREEN_STATE;
    if (!(wl->pending_vo_events & VO_EVENT_LIVE_RESIZING))
        vo_query_and_reset_events(wl->vo, VO_EVENT_LIVE_RESIZING);

    if (width > 0 && height > 0) {
        if (!wl->fullscreen) {
            if (wl->vo->opts->keepaspect && wl->vo->opts->keepaspect_window &&
                !wl->maximized) {
                if (width > height)
                    width  = height * wl->aspect_ratio;
                else
                    height =  width / wl->aspect_ratio;
            }
            wl->window_size.x0 = 0;
            wl->window_size.y0 = 0;
            wl->window_size.x1 = width;
            wl->window_size.y1 = height;
        }
        wl->geometry.x0 = 0;
        wl->geometry.y0 = 0;
        wl->geometry.x1 = width;
        wl->geometry.y1 = height;
    } else {
        wl->geometry = wl->window_size;
    }

    if (mp_rect_equals(&old_geometry, &wl->geometry))
        return;

    MP_VERBOSE(wl, "Resizing due to xdg from %ix%i to %ix%i\n",
               mp_rect_w(old_geometry)*wl->scaling, mp_rect_h(old_geometry)*wl->scaling,
               mp_rect_w(wl->geometry)*wl->scaling, mp_rect_h(wl->geometry)*wl->scaling);

    wl->pending_vo_events |= VO_EVENT_RESIZE;
}

static void handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    struct vo_wayland_state *wl = data;
    mp_input_put_key(wl->vo->input_ctx, MP_KEY_CLOSE_WIN);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_toplevel_config,
    handle_toplevel_close,
};

static int create_xdg_surface(struct vo_wayland_state *wl)
{
    wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->wm_base, wl->surface);
    xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

    wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
    xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

    xdg_toplevel_set_title (wl->xdg_toplevel, "mpv");
    xdg_toplevel_set_app_id(wl->xdg_toplevel, "mpv");

    return 0;
}

static int set_border_decorations(struct vo_wayland_state *wl, int state)
{
    if (!wl->xdg_toplevel_decoration)
        return VO_NOTIMPL;

    enum zxdg_toplevel_decoration_v1_mode mode;
    if (state) {
        MP_VERBOSE(wl, "Enabling server decorations\n");
        mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    } else {
        MP_VERBOSE(wl, "Disabling server decorations\n");
        mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    }
    zxdg_toplevel_decoration_v1_set_mode(wl->xdg_toplevel_decoration, mode);
    return VO_TRUE;
}

int vo_wayland_init(struct vo *vo)
{
    vo->wl = talloc_zero(NULL, struct vo_wayland_state);
    struct vo_wayland_state *wl = vo->wl;

    *wl = (struct vo_wayland_state) {
        .display = wl_display_connect(NULL),
        .vo = vo,
        .log = mp_log_new(wl, vo->log, "wayland"),
        .scaling = 1,
        .wakeup_pipe = {-1, -1},
        .dnd_fd = -1,
    };

    wl_list_init(&wl->output_list);

    if (!wl->display)
        return false;

    if (create_input(wl))
        return false;

    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);

    /* Do a roundtrip to run the registry */
    wl_display_roundtrip(wl->display);

    if (!wl->wm_base) {
        MP_FATAL(wl, "Compositor doesn't support the required %s protocol!\n",
                 xdg_wm_base_interface.name);
        return false;
    }

    if (!wl_list_length(&wl->output_list)) {
        MP_FATAL(wl, "No outputs found or compositor doesn't support %s (ver. 2)\n",
                 wl_output_interface.name);
        return false;
    }

    /* Can't be initialized during registry due to multi-protocol dependence */
    if (create_xdg_surface(wl))
        return false;

    if (wl->dnd_devman) {
        wl->dnd_ddev = wl_data_device_manager_get_data_device(wl->dnd_devman, wl->seat);
        wl_data_device_add_listener(wl->dnd_ddev, &data_device_listener, wl);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s (ver. 3) protocol!\n",
                   wl_data_device_manager_interface.name);
    }

    if (wl->xdg_decoration_manager) {
        wl->xdg_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wl->xdg_decoration_manager, wl->xdg_toplevel);
        set_border_decorations(wl, vo->opts->border);
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   zxdg_decoration_manager_v1_interface.name);
    }

    if (!wl->idle_inhibit_manager)
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   zwp_idle_inhibit_manager_v1_interface.name);

    wl->display_fd = wl_display_get_fd(wl->display);
    mp_make_wakeup_pipe(wl->wakeup_pipe);

    return true;
}

void vo_wayland_uninit(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    if (!wl)
        return;

    mp_input_put_key(wl->vo->input_ctx, MP_INPUT_RELEASE_ALL);

    if (wl->cursor_theme)
        wl_cursor_theme_destroy(wl->cursor_theme);

    if (wl->cursor_surface)
        wl_surface_destroy(wl->cursor_surface);

    if (wl->xkb_context)
        xkb_context_unref(wl->xkb_context);

    if (wl->idle_inhibitor)
        zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);

    if (wl->idle_inhibit_manager)
        zwp_idle_inhibit_manager_v1_destroy(wl->idle_inhibit_manager);

    if (wl->wm_base)
        xdg_wm_base_destroy(wl->wm_base);

    if (wl->shm)
        wl_shm_destroy(wl->shm);

    if (wl->dnd_devman)
        wl_data_device_manager_destroy(wl->dnd_devman);

    if (wl->xdg_toplevel_decoration)
        zxdg_toplevel_decoration_v1_destroy(wl->xdg_toplevel_decoration);

    if (wl->xdg_decoration_manager)
        zxdg_decoration_manager_v1_destroy(wl->xdg_decoration_manager);

    if (wl->surface)
        wl_surface_destroy(wl->surface);

    if (wl->frame_callback)
        wl_callback_destroy(wl->frame_callback);

    if (wl->display) {
        close(wl_display_get_fd(wl->display));
        wl_display_disconnect(wl->display);
    }

    struct vo_wayland_output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &wl->output_list, link)
        remove_output(output);

    talloc_free(wl->dnd_mime_type);

    for (int n = 0; n < 2; n++)
        close(wl->wakeup_pipe[n]);
    talloc_free(wl);
    vo->wl = NULL;
}

static struct vo_wayland_output *find_output(struct vo_wayland_state *wl, int index)
{
    int screen_id = 0;
    struct vo_wayland_output *output;
    wl_list_for_each(output, &wl->output_list, link) {
        if (index == screen_id++)
            return output;
    }
    return NULL;
}

int vo_wayland_reconfig(struct vo *vo)
{
    struct wl_output *wl_out = NULL;
    struct mp_rect screenrc = { 0 };
    struct vo_wayland_state *wl = vo->wl;

    MP_VERBOSE(wl, "Reconfiguring!\n");

    /* Surface enter events happen later but we already know the outputs and we'd
     * like to know the output the surface would be on (for scaling or fullscreen),
     * so if fsscreen_id is set or there's only one possible output, use it. */
    if (((!wl->current_output) && (wl_list_length(&wl->output_list) == 1)) ||
        (vo->opts->fullscreen && (vo->opts->fsscreen_id >= 0))) {
        int idx = 0;
        if (vo->opts->fullscreen && (vo->opts->fsscreen_id >= 0))
            idx = vo->opts->fsscreen_id;
        struct vo_wayland_output *out = find_output(wl, idx);
        if (!out) {
            MP_ERR(wl, "Screen index %i not found/unavailable!\n", idx);
        } else {
            wl_out = out->output;
            wl->current_output = out;
            wl->scaling = out->scale;
            screenrc = wl->current_output->geometry;
        }
    }

    struct vo_win_geometry geo;
    vo_calc_window_geometry(vo, &screenrc, &geo);
    vo_apply_window_geometry(vo, &geo);

    if (!wl->configured || !wl->maximized) {
        wl->geometry.x0 = 0;
        wl->geometry.y0 = 0;
        wl->geometry.x1 = vo->dwidth  / wl->scaling;
        wl->geometry.y1 = vo->dheight / wl->scaling;
        wl->window_size = wl->geometry;
    }

    wl->aspect_ratio = vo->dwidth / (float)vo->dheight;

    if (vo->opts->fullscreen) {
        /* If already fullscreen, fix resolution for the frame size change */
        if (wl->fullscreen && wl->current_output) {
            wl->geometry.x0  = 0;
            wl->geometry.y0  = 0;
            wl->geometry.x1  = mp_rect_w(wl->current_output->geometry)/wl->scaling;
            wl->geometry.y1  = mp_rect_h(wl->current_output->geometry)/wl->scaling;
        } else {
            xdg_toplevel_set_fullscreen(wl->xdg_toplevel, wl_out);
        }
    }

    wl_surface_set_buffer_scale(wl->surface, wl->scaling);
    wl_surface_commit(wl->surface);
    wl->pending_vo_events |= VO_EVENT_RESIZE;
    if (!wl->configured) {
        if (spawn_cursor(wl))
            return false;
        wl_display_roundtrip(wl->display);
        wl->configured = true;
    }

    return true;
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

static int toggle_fullscreen(struct vo_wayland_state *wl)
{
    if (!wl->xdg_toplevel)
        return VO_NOTAVAIL;
    if (wl->fullscreen)
        xdg_toplevel_unset_fullscreen(wl->xdg_toplevel);
    else
        xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
    return VO_TRUE;
}

static int update_window_title(struct vo_wayland_state *wl, char *title)
{
    if (!wl->xdg_toplevel)
        return VO_NOTAVAIL;
    xdg_toplevel_set_title(wl->xdg_toplevel, title);
    return VO_TRUE;
}

static void check_dnd_fd(struct vo_wayland_state *wl)
{
    if (wl->dnd_fd == -1)
        return;

    struct pollfd fdp = { wl->dnd_fd, POLLIN | POLLERR | POLLHUP, 0 };
    if (poll(&fdp, 1, 0) <= 0)
        return;

    if (fdp.revents & POLLIN) {
        ptrdiff_t offset = 0;
        size_t data_read = 0;
        const size_t chunk_size = 1;
        uint8_t *buffer = ta_zalloc_size(wl, chunk_size);
        if (!buffer)
            goto end;

        while ((data_read = read(wl->dnd_fd, buffer + offset, chunk_size)) > 0) {
            offset += data_read;
            buffer = ta_realloc_size(wl, buffer, offset + chunk_size);
            memset(buffer + offset, 0, chunk_size);
            if (!buffer)
                goto end;
        }

        MP_VERBOSE(wl, "Read %td bytes from the DND fd\n", offset);

        struct bstr file_list = bstr0(buffer);
        mp_event_drop_mime_data(wl->vo->input_ctx, wl->dnd_mime_type,
                                file_list, wl->dnd_action);
        talloc_free(buffer);
end:
        wl_data_offer_finish(wl->dnd_offer);
        talloc_free(wl->dnd_mime_type);
        wl->dnd_mime_type = NULL;
        wl->dnd_mime_score = 0;
    }

    if (fdp.revents & (POLLIN | POLLERR | POLLHUP)) {
        close(wl->dnd_fd);
        wl->dnd_fd = -1;
    }
}

static char **get_displays_spanned(struct vo_wayland_state *wl)
{
    char **names = NULL;
    int displays_spanned = 0;
    struct vo_wayland_output *output;
    wl_list_for_each(output, &wl->output_list, link) {
        if (output->has_surface)
            MP_TARRAY_APPEND(NULL, names, displays_spanned,
                             talloc_strdup(NULL, output->model));
    }
    MP_TARRAY_APPEND(NULL, names, displays_spanned, NULL);
    return names;
}

int vo_wayland_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_wayland_state *wl = vo->wl;
    wl_display_dispatch_pending(wl->display);

    switch (request) {
    case VOCTRL_CHECK_EVENTS: {
        check_dnd_fd(wl);
        *events |= wl->pending_vo_events;
        wl->pending_vo_events = 0;
        return VO_TRUE;
    }
    case VOCTRL_GET_FULLSCREEN: {
        *(int *)arg = wl->fullscreen;
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_NAMES: {
        *(char ***)arg = get_displays_spanned(wl);
        return VO_TRUE;
    }
    case VOCTRL_PAUSE: {
        wl_callback_destroy(wl->frame_callback);
        wl->frame_callback = NULL;
        vo_disable_external_renderloop(wl->vo);
        return VO_TRUE;
    }
    case VOCTRL_RESUME: {
        vo_enable_external_renderloop(wl->vo);
        frame_callback(wl, NULL, 0);
        return VO_TRUE;
    }
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        s[0] = mp_rect_w(wl->geometry)*wl->scaling;
        s[1] = mp_rect_h(wl->geometry)*wl->scaling;
        return VO_TRUE;
    }
    case VOCTRL_SET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        if (!wl->fullscreen && !wl->maximized) {
            wl->geometry.x0 = 0;
            wl->geometry.y0 = 0;
            wl->geometry.x1 = s[0]/wl->scaling;
            wl->geometry.y1 = s[1]/wl->scaling;
            wl->pending_vo_events |= VO_EVENT_RESIZE;
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_FPS: {
        if (!wl->current_output)
            return VO_NOTAVAIL;
        *(double *)arg = wl->current_output->refresh_rate;
        return VO_TRUE;
    }
    case VOCTRL_UPDATE_WINDOW_TITLE:
        return update_window_title(wl, (char *)arg);
    case VOCTRL_FULLSCREEN:
        return toggle_fullscreen(wl);
    case VOCTRL_SET_CURSOR_VISIBILITY:
        return set_cursor_visibility(wl, *(bool *)arg);
    case VOCTRL_BORDER:
        return set_border_decorations(wl, vo->opts->border);
    case VOCTRL_KILL_SCREENSAVER:
        return set_screensaver_inhibitor(wl, true);
    case VOCTRL_RESTORE_SCREENSAVER:
        return set_screensaver_inhibitor(wl, false);
    }

    return VO_NOTIMPL;
}

void vo_wayland_wakeup(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    (void)write(wl->wakeup_pipe[1], &(char){0}, 1);
}

void vo_wayland_wait_events(struct vo *vo, int64_t until_time_us)
{
    struct vo_wayland_state *wl = vo->wl;
    struct wl_display *display = wl->display;

    if (wl->display_fd == -1)
        return;

    struct pollfd fds[2] = {
        {.fd = wl->display_fd,     .events = POLLIN },
        {.fd = wl->wakeup_pipe[0], .events = POLLIN },
    };

    int64_t wait_us = until_time_us - mp_time_us();
    int timeout_ms = MPCLAMP((wait_us + 999) / 1000, 0, 10000);

    wl_display_dispatch_pending(display);
    wl_display_flush(display);

    poll(fds, 2, timeout_ms);

    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        MP_FATAL(wl, "Error occurred on the display fd, closing\n");
        close(wl->display_fd);
        wl->display_fd = -1;
        mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
    }

    if (fds[0].revents & POLLIN)
        wl_display_dispatch(display);

    if (fds[1].revents & POLLIN)
        mp_flush_wakeup_pipe(wl->wakeup_pipe[0]);
}
