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

#include "common/common.h"
#include "common/msg.h"
#include "input.h"
#include "input/keycodes.h"
#include "misc/path_utils.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/poll_wrapper.h"

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
};

static void input_wheel(struct mp_input_src *src, bool horiz, double value)
{
    if (value > 0 && !horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_DOWN, +value);
    if (value < 0 && !horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_UP, -value);
    if (value > 0 && horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_RIGHT, +value);
    if (value < 0 && horiz)
        mp_input_put_wheel(src->input_ctx, MP_WHEEL_LEFT, -value);
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
            mp_input_put_key(src->input_ctx, button | state);
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
        MP_ERR(src, "Failed to init udev context.");
        goto fail;
    }
    p->li = libinput_udev_create_context(&interface, NULL, p->u);
    if (!p->li) {
        MP_ERR(src, "Failed to init libinput context.");
        goto fail;
    }
    libinput_udev_assign_seat(p->li, "seat0");
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
