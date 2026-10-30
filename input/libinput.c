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

#include <stdbool.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>

#include "common/common.h"
#include "common/msg.h"
#include "input.h"
#include "input/keycodes.h"
#include "misc/path_utils.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/poll_wrapper.h"
#include "video/out/vo.h"

static int open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

struct libinput_priv {
    struct libinput *li;
    struct udev *u;
    int message_pipe[2];
    double dx, dy;
    int mods;
    //xkb states
    struct xkb_context *ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
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

static int get_mods(struct mp_input_src *src)
{
    struct libinput_priv *p = src->priv;
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
        xkb_mod_index_t index = xkb_keymap_mod_get_index(p->keymap, mod_names[n]);
        if (index != XKB_MOD_INVALID
            && xkb_state_mod_index_is_active(p->state, index,
                                             XKB_STATE_MODS_EFFECTIVE))
            modifiers |= mods[n];
    }
    return modifiers;
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

static void input_key(struct mp_input_src *src, struct libinput_event *event)
{
    struct libinput_priv *p = src->priv;
    struct libinput_event_keyboard *ev = libinput_event_get_keyboard_event(event);

    xkb_keycode_t keycode = libinput_event_keyboard_get_key(ev) + 8;
    bool pressed = libinput_event_keyboard_get_key_state(ev) == LIBINPUT_KEY_STATE_PRESSED;
    int state = pressed ? MP_KEY_STATE_DOWN : MP_KEY_STATE_UP;

    int mods = get_mods(src);
    xkb_keysym_t sym = xkb_state_key_get_one_sym(p->state, keycode);
    xkb_state_update_key(p->state, keycode, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
    p->mods = get_mods(src);

    int mpkey = lookupkey(sym);
    if (mpkey) {
        mp_input_put_key(src->input_ctx, mpkey | state | mods);
    } else {
        char str[128];
        if (xkb_keysym_to_utf8(sym, str, sizeof(str)) > 0) {
            mp_input_put_key_utf8(src->input_ctx, state | mods, bstr0(str));
        }
    }
}

static void update_leds(struct mp_input_src *src, struct libinput_event *event)
{
    struct libinput_priv *p = src->priv;
    enum libinput_led leds = 0;

    if (xkb_state_led_name_is_active(p->state, XKB_LED_NAME_NUM) == 1)
        leds |= LIBINPUT_LED_NUM_LOCK;
    if (xkb_state_led_name_is_active(p->state, XKB_LED_NAME_CAPS) == 1)
        leds |= LIBINPUT_LED_CAPS_LOCK;
    if (xkb_state_led_name_is_active(p->state, XKB_LED_NAME_SCROLL) == 1)
        leds |= LIBINPUT_LED_SCROLL_LOCK;

    libinput_device_led_update(libinput_event_get_device(event), leds);
}

static void input_wheel(struct mp_input_src *src, bool horiz, double value)
{
    struct libinput_priv *p = src->priv;
    if (value > 0 && !horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_DOWN | p->mods, +value);
    if (value < 0 && !horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_UP | p->mods, -value);
    if (value > 0 && horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_RIGHT | p->mods, +value);
    if (value < 0 && horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_LEFT | p->mods, -value);
}

static void input_tablet_pos(struct mp_input_src *src, struct libinput_event_tablet_tool *ev)
{
    struct mp_rect dst = {0};
    mp_input_get_mouse_transform(src->input_ctx, &dst);
    double x = libinput_event_tablet_tool_get_x_transformed(ev, mp_rect_w(dst));
    double y = libinput_event_tablet_tool_get_y_transformed(ev, mp_rect_h(dst));
    mp_input_set_tablet_pos(src->input_ctx, x, y, false);
}

static void process_event(struct mp_input_src *src, struct libinput_event *event)
{
    struct libinput_priv *p = src->priv;
    enum libinput_event_type type = libinput_event_get_type(event);
    switch (type) {
    case LIBINPUT_EVENT_KEYBOARD_KEY: {
        input_key(src, event);
        update_leds(src, event);
        break;
    }
    case LIBINPUT_EVENT_POINTER_MOTION: {
        struct libinput_event_pointer *ev = libinput_event_get_pointer_event(event);
        p->dx += libinput_event_pointer_get_dx(ev);
        p->dy += libinput_event_pointer_get_dy(ev);
        int dx = trunc(p->dx);
        int dy = trunc(p->dy);
        p->dx -= dx;
        p->dy -= dy;
        int x, y, hover;
        mp_input_get_mouse_pos(src->input_ctx, &x, &y, &hover);
        mp_input_set_mouse_pos(src->input_ctx, x + dx, y + dy, false);
        break;
    }
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
        struct libinput_event_pointer *ev = libinput_event_get_pointer_event(event);
        struct mp_rect dst = {0};
        mp_input_get_mouse_transform(src->input_ctx, &dst);
        double x = libinput_event_pointer_get_absolute_x_transformed(ev, mp_rect_w(dst));
        double y = libinput_event_pointer_get_absolute_y_transformed(ev, mp_rect_h(dst));
        mp_input_set_mouse_pos(src->input_ctx, x, y, false);
        break;
    }
    case LIBINPUT_EVENT_POINTER_BUTTON: {
        struct libinput_event_pointer *ev = libinput_event_get_pointer_event(event);
        int state = libinput_event_pointer_get_button_state(ev) == LIBINPUT_BUTTON_STATE_PRESSED ?
            MP_KEY_STATE_DOWN : MP_KEY_STATE_UP;
        uint32_t button = 0;
        if (button >= BTN_MOUSE && button < BTN_JOYSTICK) {
            switch (libinput_event_pointer_get_button(ev)) {
            case BTN_LEFT:   button = MP_MBTN_LEFT; break;
            case BTN_MIDDLE: button = MP_MBTN_MID; break;
            case BTN_RIGHT:  button = MP_MBTN_RIGHT; break;
            case BTN_SIDE:   button = MP_MBTN_BACK; break;
            case BTN_EXTRA:  button = MP_MBTN_FORWARD; break;
            default: button += MP_MBTN9 - BTN_FORWARD; break;
            }
        }
        if (button)
            mp_input_put_key(src->input_ctx, button | state | p->mods);
        break;
    }
    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
        struct libinput_event_pointer *ev = libinput_event_get_pointer_event(event);
        if (libinput_event_pointer_has_axis(ev, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
            double value = libinput_event_pointer_get_scroll_value_v120(ev, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL) / 120.0;
            input_wheel(src, true, value);
        }
        if (libinput_event_pointer_has_axis(ev, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
            double value = libinput_event_pointer_get_scroll_value_v120(ev, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) / 120.0;
            input_wheel(src, false, value);
        }
        break;
    }
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
    case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS: {
        struct libinput_event_pointer *ev = libinput_event_get_pointer_event(event);
        if (libinput_event_pointer_has_axis(ev, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
            double value = libinput_event_pointer_get_scroll_value(ev, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL) / 15.0;
            input_wheel(src, true, value);
        }
        if (libinput_event_pointer_has_axis(ev, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
            double value = libinput_event_pointer_get_scroll_value(ev, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) / 15.0;
            input_wheel(src, false, value);
        }
        break;
    }
    case LIBINPUT_EVENT_TOUCH_CANCEL: {
        mp_input_put_key(src->input_ctx, MP_TOUCH_RELEASE_ALL);
        break;
    }
    case LIBINPUT_EVENT_TOUCH_UP: {
        struct libinput_event_touch *ev = libinput_event_get_touch_event(event);
        int32_t slot = libinput_event_touch_get_seat_slot(ev);
        mp_input_remove_touch_point(src->input_ctx, slot);
        break;
    }
    case LIBINPUT_EVENT_TOUCH_MOTION:
    case LIBINPUT_EVENT_TOUCH_DOWN: {
        struct libinput_event_touch *ev = libinput_event_get_touch_event(event);
        int32_t slot = libinput_event_touch_get_seat_slot(ev);
        struct mp_rect dst = {0};
        mp_input_get_mouse_transform(src->input_ctx, &dst);
        double x = libinput_event_touch_get_x_transformed(ev, mp_rect_w(dst));
        double y = libinput_event_touch_get_y_transformed(ev, mp_rect_h(dst));
        if (type == LIBINPUT_EVENT_TOUCH_DOWN)
            mp_input_add_touch_point(src->input_ctx, slot, x, y);
        else
            mp_input_update_touch_point(src->input_ctx, slot, x, y);
        break;
    }
    case LIBINPUT_EVENT_TABLET_TOOL_AXIS: {
        struct libinput_event_tablet_tool *ev = libinput_event_get_tablet_tool_event(event);
        input_tablet_pos(src, ev);
        break;
    }
    case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY: {
        struct libinput_event_tablet_tool *ev = libinput_event_get_tablet_tool_event(event);
        input_tablet_pos(src, ev);
        bool in = libinput_event_tablet_tool_get_proximity_state(ev) == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN;
        mp_input_set_tablet_tool_in_proximity(src->input_ctx, in);
        break;
    }
    case LIBINPUT_EVENT_TABLET_TOOL_TIP: {
        struct libinput_event_tablet_tool *ev = libinput_event_get_tablet_tool_event(event);
        input_tablet_pos(src, ev);
        bool down = libinput_event_tablet_tool_get_tip_state(ev) == LIBINPUT_TABLET_TOOL_TIP_DOWN;
        if (down)
            mp_input_tablet_tool_down(src->input_ctx);
        else
            mp_input_tablet_tool_up(src->input_ctx);
        break;
    }
    case LIBINPUT_EVENT_TABLET_TOOL_BUTTON: {
        struct libinput_event_tablet_tool *ev = libinput_event_get_tablet_tool_event(event);
        uint32_t button = libinput_event_tablet_tool_get_button(ev);
        int state = libinput_event_tablet_tool_get_button_state(ev) == LIBINPUT_BUTTON_STATE_PRESSED ?
            MP_KEY_STATE_DOWN : MP_KEY_STATE_UP;
        switch (button) {
        case BTN_STYLUS:  button = MP_KEY_TABLET_TOOL_STYLUS_BTN1; break;
        case BTN_STYLUS2: button = MP_KEY_TABLET_TOOL_STYLUS_BTN2; break;
        case BTN_STYLUS3: button = MP_KEY_TABLET_TOOL_STYLUS_BTN3; break;
        default: button = 0; break;
        }
        if (button)
            mp_input_tablet_tool_button(src->input_ctx, button, state);
        break;
    }
    }
}

static const uint8_t MESSAGE_DEATH = 0;
static void request_cancel(struct mp_input_src *src)
{
    MP_VERBOSE(src, "exiting...\n");
    struct libinput_priv *p = src->priv;
    (void)write(p->message_pipe[1], &MESSAGE_DEATH, sizeof(MESSAGE_DEATH));
}

static void uninit(struct mp_input_src *src)
{
    struct libinput_priv *p = src->priv;
    if (p->li)
        libinput_unref(p->li);
    if (p->u)
        udev_unref(p->u);
    if (p->state)
        xkb_state_unref(p->state);
    if (p->keymap)
        xkb_keymap_unref(p->keymap);
    if (p->ctx)
        xkb_context_unref(p->ctx);
    close(p->message_pipe[0]);
    close(p->message_pipe[1]);
    MP_VERBOSE(src, "exited.\n");
}

static void read_libinput_thread(struct mp_input_src *src, void *param)
{
    struct libinput_priv *p = src->priv = talloc_zero(src, struct libinput_priv);
    src->cancel = request_cancel;
    src->uninit = uninit;

    p->message_pipe[0] = p->message_pipe[1] = -1;
    p->u = udev_new();
    if (!p->u) {
        MP_ERR(src, "Failed to init udev context.\n");
        goto fail;
    }
    p->li = libinput_udev_create_context(&interface, NULL, p->u);
    if (!p->li) {
        MP_ERR(src, "Failed to init libinput context.\n");
        goto fail;
    }
    p->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!p->ctx) {
        MP_ERR(src, "Failed to init xkb context.\n");
        goto fail;
    }
    p->keymap = xkb_keymap_new_from_names(p->ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!p->keymap) {
        MP_ERR(src, "Failed to init xkb keymap.\n");
        goto fail;
    }
    p->state = xkb_state_new(p->keymap);
    if (!p->state) {
        MP_ERR(src, "Failed to init xkb state.\n");
        goto fail;
    }
    if (libinput_udev_assign_seat(p->li, "seat0")) {
        MP_ERR(src, "Failed to assign seat.");
        goto fail;
    }
    if (mp_make_wakeup_pipe(p->message_pipe) < 0) {
        MP_ERR(src, "Failed to init message pipe.");
        goto fail;
    }
    mp_input_src_init_done(src);

    // Make sure mouse cursor is visible
    mp_input_put_key(src->input_ctx, MP_KEY_MOUSE_ENTER);
    struct libinput_event *event;
    int fd = libinput_get_fd(p->li);
    do {
        struct pollfd fds[] = {
            {.fd = fd, .events = POLLIN},
            {.fd = p->message_pipe[0], .events = POLLIN},
        };
        while (mp_poll(fds, MP_ARRAY_SIZE(fds), -1) > -1) {
            if (fds[1].revents & POLLIN)
                return;
            if (libinput_dispatch(p->li))
                return;
            while ((event = libinput_get_event(p->li)) != NULL) {
                process_event(src, event);
                libinput_event_destroy(event);
            }
        }
    } while (errno == EINTR);
    return;
fail:
    mp_input_src_init_done(src);
    return;
}

void mp_input_libinput_add(struct input_ctx *ictx)
{
    mp_input_add_thread_src(ictx, NULL, read_libinput_thread);
}
