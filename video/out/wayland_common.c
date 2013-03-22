/*
 * This file is part of MPlayer.
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2012-2013 Collabora, Ltd.
 * Copyright © 2012-2013 Scott Moreau <oreaus@gmail.com>
 * Copyright © 2012-2013 Alexander Preisinger <alexander.preisinger@gmail.com>
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <limits.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <linux/input.h>

#include "config.h"
#include "core/bstr.h"
#include "core/options.h"
#include "core/mp_msg.h"
#include "core/mp_fifo.h"
#include "libavutil/common.h"
#include "talloc.h"

#include "wayland_common.h"

#include "vo.h"
#include "aspect.h"
#include "osdep/timer.h"

#include "core/subopt-helper.h"

#include "core/input/input.h"
#include "core/input/keycodes.h"

#define MOD_SHIFT_MASK      0x01
#define MOD_ALT_MASK        0x02
#define MOD_CONTROL_MASK    0x04

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

static int lookupkey(int key);

static void hide_cursor(struct vo_wayland_state * wl);
static void show_cursor(struct vo_wayland_state * wl);


/*** wayland interface ***/

/* SHELL SURFACE LISTENER  */
static void ssurface_handle_ping(void *data,
                                 struct wl_shell_surface *shell_surface,
                                 uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void ssurface_schedule_resize(struct vo_wayland_window *window,
                                     int32_t width,
                                     int32_t height)
{
    window->pending_width = width;
    window->pending_height = height;
    window->resize_needed = 1;
    window->events |= VO_EVENT_RESIZE;
}

static void ssurface_handle_configure(void *data,
                                      struct wl_shell_surface *shell_surface,
                                      uint32_t edges,
                                      int32_t width,
                                      int32_t height)
{
    struct vo_wayland_state *wl = data;

    wl->window->edges = edges;
    ssurface_schedule_resize(wl->window, width, height);
}

static void ssurface_handle_popup_done(void *data,
                                       struct wl_shell_surface *shell_surface)
{
}

const struct wl_shell_surface_listener shell_surface_listener = {
    ssurface_handle_ping,
    ssurface_handle_configure,
    ssurface_handle_popup_done
};

/* OUTPUT LISTENER */
static void output_handle_geometry(void *data,
                                   struct wl_output *wl_output,
                                   int32_t x,
                                   int32_t y,
                                   int32_t physical_width,
                                   int32_t physical_height,
                                   int32_t subpixel,
                                   const char *make,
                                   const char *model,
                                   int32_t transform)
{
    /* Ignore transforms for now */
    switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_180:
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        default:
            break;
    }
}

static void output_handle_mode(void *data,
                               struct wl_output *wl_output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh)
{
    struct vo_wayland_display *d = data;
    struct vo_wayland_output *output;

    wl_list_for_each(output, &d->output_list, link) {
        if (wl_output == output->output) {
            output->width = width;
            output->height = height;
            if (flags)
                output->flags = flags;
        }
    }

    /* one output is enough */
    d->output_mode_received = 1;
}

const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode
};

/* KEY LOOKUP */
static const struct mp_keymap keymap[] = {
    // special keys
    {XKB_KEY_Pause,     MP_KEY_PAUSE}, {XKB_KEY_Escape, MP_KEY_ESC},
    {XKB_KEY_BackSpace, MP_KEY_BS},    {XKB_KEY_Tab,    MP_KEY_TAB},
    {XKB_KEY_Return,    MP_KEY_ENTER}, {XKB_KEY_Menu,   MP_KEY_MENU},
    {XKB_KEY_Print,     MP_KEY_PRINT},

    // cursor keys
    {XKB_KEY_Left, MP_KEY_LEFT}, {XKB_KEY_Right, MP_KEY_RIGHT},
    {XKB_KEY_Up,   MP_KEY_UP},   {XKB_KEY_Down,  MP_KEY_DOWN},

    // navigation block
    {XKB_KEY_Insert,  MP_KEY_INSERT},  {XKB_KEY_Delete,    MP_KEY_DELETE},
    {XKB_KEY_Home,    MP_KEY_HOME},    {XKB_KEY_End,       MP_KEY_END},
    {XKB_KEY_Page_Up, MP_KEY_PAGE_UP}, {XKB_KEY_Page_Down, MP_KEY_PAGE_DOWN},

    // F-keys
    {XKB_KEY_F1,  MP_KEY_F+1},  {XKB_KEY_F2,  MP_KEY_F+2},
    {XKB_KEY_F3,  MP_KEY_F+3},  {XKB_KEY_F4,  MP_KEY_F+4},
    {XKB_KEY_F5,  MP_KEY_F+5},  {XKB_KEY_F6,  MP_KEY_F+6},
    {XKB_KEY_F7,  MP_KEY_F+7},  {XKB_KEY_F8,  MP_KEY_F+8},
    {XKB_KEY_F9,  MP_KEY_F+9},  {XKB_KEY_F10, MP_KEY_F+10},
    {XKB_KEY_F11, MP_KEY_F+11}, {XKB_KEY_F12, MP_KEY_F+12},

    // numpad independent of numlock
    {XKB_KEY_KP_Subtract, '-'}, {XKB_KEY_KP_Add, '+'},
    {XKB_KEY_KP_Multiply, '*'}, {XKB_KEY_KP_Divide, '/'},
    {XKB_KEY_KP_Enter, MP_KEY_KPENTER},

    // numpad with numlock
    {XKB_KEY_KP_0, MP_KEY_KP0}, {XKB_KEY_KP_1, MP_KEY_KP1},
    {XKB_KEY_KP_2, MP_KEY_KP2}, {XKB_KEY_KP_3, MP_KEY_KP3},
    {XKB_KEY_KP_4, MP_KEY_KP4}, {XKB_KEY_KP_5, MP_KEY_KP5},
    {XKB_KEY_KP_6, MP_KEY_KP6}, {XKB_KEY_KP_7, MP_KEY_KP7},
    {XKB_KEY_KP_8, MP_KEY_KP8}, {XKB_KEY_KP_9, MP_KEY_KP9},
    {XKB_KEY_KP_Decimal, MP_KEY_KPDEC}, {XKB_KEY_KP_Separator, MP_KEY_KPDEC},

    // numpad without numlock
    {XKB_KEY_KP_Insert, MP_KEY_KPINS}, {XKB_KEY_KP_End,       MP_KEY_KP1},
    {XKB_KEY_KP_Down,   MP_KEY_KP2},   {XKB_KEY_KP_Page_Down, MP_KEY_KP3},
    {XKB_KEY_KP_Left,   MP_KEY_KP4},   {XKB_KEY_KP_Begin,     MP_KEY_KP5},
    {XKB_KEY_KP_Right,  MP_KEY_KP6},   {XKB_KEY_KP_Home,      MP_KEY_KP7},
    {XKB_KEY_KP_Up,     MP_KEY_KP8},   {XKB_KEY_KP_Page_Up,   MP_KEY_KP9},
    {XKB_KEY_KP_Delete, MP_KEY_KPDEL},

    {0, 0}
};

/* KEYBOARD LISTENER */
static void keyboard_handle_keymap(void *data,
                                   struct wl_keyboard *wl_keyboard,
                                   uint32_t format,
                                   int32_t fd,
                                   uint32_t size)
{
    struct vo_wayland_input *input = ((struct vo_wayland_state *) data)->input;
    char *map_str;

    if(!data) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    input->xkb.keymap = xkb_map_new_from_string(input->xkb.context,
            map_str, XKB_KEYMAP_FORMAT_TEXT_V1, 0);

    munmap(map_str, size);
    close(fd);

    if (!input->xkb.keymap) {
        mp_msg(MSGT_VO, MSGL_ERR, "[wayland] failed to compile keymap.\n");
        return;
    }

    input->xkb.state = xkb_state_new(input->xkb.keymap);
    if (!input->xkb.state) {
        mp_msg(MSGT_VO, MSGL_ERR, "[wayland] failed to create XKB state.\n");
        xkb_map_unref(input->xkb.keymap);
        input->xkb.keymap = NULL;
        return;
    }

    input->xkb.control_mask =
        1 << xkb_map_mod_get_index(input->xkb.keymap, "Control");
    input->xkb.alt_mask =
        1 << xkb_map_mod_get_index(input->xkb.keymap, "Mod1");
    input->xkb.shift_mask =
        1 << xkb_map_mod_get_index(input->xkb.keymap, "Shift");
}

static void keyboard_handle_enter(void *data,
                                  struct wl_keyboard *wl_keyboard,
                                  uint32_t serial,
                                  struct wl_surface *surface,
                                  struct wl_array *keys)
{
}

static void keyboard_handle_leave(void *data,
                                  struct wl_keyboard *wl_keyboard,
                                  uint32_t serial,
                                  struct wl_surface *surface)
{
}

static void keyboard_handle_key(void *data,
                                struct wl_keyboard *wl_keyboard,
                                uint32_t serial,
                                uint32_t time,
                                uint32_t key,
                                uint32_t state)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_input *input = wl->input;
    uint32_t code, num_syms;

    struct itimerspec its = {{0, 0}, {0, 0}};

    const xkb_keysym_t *syms;
    xkb_keysym_t sym;
    xkb_mod_mask_t mask;

    code = key + 8;
    num_syms = xkb_key_get_syms(input->xkb.state, code, &syms);

    mask = xkb_state_serialize_mods(input->xkb.state,
            XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);

    input->modifiers = 0;
    if (mask & input->xkb.control_mask)
        input->modifiers |= MOD_CONTROL_MASK;
    if (mask & input->xkb.alt_mask)
        input->modifiers |= MOD_ALT_MASK;
    if (mask & input->xkb.shift_mask)
        input->modifiers |= MOD_SHIFT_MASK;

    sym = XKB_KEY_NoSymbol;
    if (num_syms == 1)
        sym = syms[0];

    if (sym != XKB_KEY_NoSymbol && state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        int mpkey = lookupkey(sym);
        if (mpkey)
            mplayer_put_key(wl->vo->key_fifo, mpkey);
    }

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == input->repeat.key) {
        input->repeat.sym = 0;
        input->repeat.key = 0;
        input->repeat.time = 0;
    }
    else if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (input->repeat.key == key) {
            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = 20 * 1000 * 1000;
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = 100 * 1000 * 1000;
        }
        else {
            input->repeat.sym = sym;
            input->repeat.key = key;
            input->repeat.time = time;
            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = 25 * 1000 * 1000;
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = 400 * 1000 * 1000;
        }
    }
    timerfd_settime(input->repeat.timer_fd, 0, &its, NULL);
}

static void keyboard_handle_modifiers(void *data,
                                      struct wl_keyboard *wl_keyboard,
                                      uint32_t serial,
                                      uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked,
                                      uint32_t group)
{
    struct vo_wayland_input *input = ((struct vo_wayland_state *) data)->input;

    xkb_state_update_mask(input->xkb.state, mods_depressed, mods_latched,
            mods_locked, 0, 0, group);
}

const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers
};

/* POINTER LISTENER */
static void pointer_handle_enter(void *data,
                                 struct wl_pointer *pointer,
                                 uint32_t serial,
                                 struct wl_surface *surface,
                                 wl_fixed_t sx_w,
                                 wl_fixed_t sy_w)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_display * display = wl->display;

    display->cursor.serial = serial;
    display->cursor.pointer = pointer;

    /* Release the left button on pointer enter again
     * because after moving the shell surface no release event is sent */
    mplayer_put_key(wl->vo->key_fifo, MP_MOUSE_BTN0);

    if (wl->window->type == TYPE_FULLSCREEN || wl->vo->opts->cursor_autohide_delay == -2)
        hide_cursor(wl);
    else if (display->cursor.default_cursor) {
        show_cursor(wl);
    }
}

static void pointer_handle_leave(void *data,
                                 struct wl_pointer *pointer,
                                 uint32_t serial,
                                 struct wl_surface *surface)
{
}

static void pointer_handle_motion(void *data,
                                  struct wl_pointer *pointer,
                                  uint32_t time,
                                  wl_fixed_t sx_w,
                                  wl_fixed_t sy_w)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_display * display = wl->display;

    display->cursor.pointer = pointer;

    if (wl->window->type == TYPE_FULLSCREEN) {
        show_cursor(wl);

        struct itimerspec its;
        int ms = wl->vo->opts->cursor_autohide_delay;
        int sec = ms / 1000;
        ms %= 1000;

        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 100 * 1000 * 1000;
        its.it_value.tv_sec = sec;
        its.it_value.tv_nsec = ms * 1000 * 1000;
        timerfd_settime(display->cursor.timer_fd, 0, &its, NULL);
    }
}

static void pointer_handle_button(void *data,
                                  struct wl_pointer *pointer,
                                  uint32_t serial,
                                  uint32_t time,
                                  uint32_t button,
                                  uint32_t state)
{
    struct vo_wayland_state *wl = data;

    mplayer_put_key(wl->vo->key_fifo, MP_MOUSE_BTN0 + (button - BTN_LEFT) |
        ((state == WL_POINTER_BUTTON_STATE_PRESSED) ? MP_KEY_STATE_DOWN : 0));

    if ((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED))
        wl_shell_surface_move(wl->window->shell_surface, wl->input->seat, serial);

}

static void pointer_handle_axis(void *data,
                                struct wl_pointer *pointer,
                                uint32_t time,
                                uint32_t axis,
                                wl_fixed_t value)
{
    struct vo_wayland_state *wl = data;

    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (value > 0)
            mplayer_put_key(wl->vo->key_fifo, MP_MOUSE_BTN4);
        if (value < 0)
            mplayer_put_key(wl->vo->key_fifo, MP_MOUSE_BTN3);
    }
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void seat_handle_capabilities(void *data,
                                     struct wl_seat *seat,
                                     enum wl_seat_capability caps)
{
    struct vo_wayland_state *wl = data;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->input->keyboard) {
        wl->input->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_set_user_data(wl->input->keyboard, wl);
        wl_keyboard_add_listener(wl->input->keyboard, &keyboard_listener, wl);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->input->keyboard) {
        wl_keyboard_destroy(wl->input->keyboard);
        wl->input->keyboard = NULL;
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl->input->pointer) {
        wl->input->pointer = wl_seat_get_pointer(seat);
        wl_pointer_set_user_data(wl->input->pointer, wl);
        wl_pointer_add_listener(wl->input->pointer, &pointer_listener, wl);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl->input->pointer) {
        wl_pointer_destroy(wl->input->pointer);
        wl->input->pointer = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

/* SHM LISTENER */
static void shm_handle_format(void *data,
                              struct wl_shm *wl_shm,
                              uint32_t format)
{
    struct vo_wayland_display *d = data;
    d->formats |= (1 << format);
}

const struct wl_shm_listener shm_listener = {
    shm_handle_format
};

static void registry_handle_global (void *data,
                                    struct wl_registry *registry,
                                    uint32_t id,
                                    const char *interface,
                                    uint32_t version)
{
    struct vo_wayland_state *wl = data;
    struct vo_wayland_display *d = wl->display;

    if (strcmp(interface, "wl_compositor") == 0) {

        d->compositor = wl_registry_bind(d->registry, id,
                &wl_compositor_interface, 1);
    }

    else if (strcmp(interface, "wl_shell") == 0) {

        d->shell = wl_registry_bind(d->registry, id, &wl_shell_interface, 1);
    }

    else if (strcmp(interface, "wl_shm") == 0) {

        d->cursor.shm = wl_registry_bind(d->registry, id, &wl_shm_interface, 1);
        d->cursor.theme = wl_cursor_theme_load(NULL, 32, d->cursor.shm);
        d->cursor.default_cursor = wl_cursor_theme_get_cursor(d->cursor.theme,
                                                              "left_ptr");
        wl_shm_add_listener(d->cursor.shm, &shm_listener, d);
    }

    else if (strcmp(interface, "wl_output") == 0) {

        struct vo_wayland_output *output = talloc_zero(d,
                struct vo_wayland_output);
        output->id = id;
        output->output = wl_registry_bind(d->registry,
                                          id,
                                          &wl_output_interface,
                                          1);

        wl_output_add_listener(output->output, &output_listener, d);
        wl_list_insert(&d->output_list, &output->link);
    }

    else if (strcmp(interface, "wl_seat") == 0) {

        wl->input->seat = wl_registry_bind(d->registry,
                                           id,
                                           &wl_seat_interface,
                                           1);

        wl_seat_add_listener(wl->input->seat, &seat_listener, wl);
    }
}

static void registry_handle_global_remove (void *data,
                                           struct wl_registry *registry,
                                           uint32_t id)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};


/*** internal functions ***/

static int set_cloexec_or_close(int fd)
{
    long flags;

    if (fd == -1)
        return -1;

    if ((flags = fcntl(fd, F_GETFD)) == -1)
        goto err;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
        goto err;

    return fd;

err:
    close(fd);
    return -1;
}

static int os_epoll_create_cloexec(void)
{
    int fd;

#ifdef EPOLL_CLOEXEC
    if ((fd = epoll_create1(EPOLL_CLOEXEC)) >= 0)
        return fd;
    if (errno != EINVAL)
        return -1;
#endif

    return set_cloexec_or_close(epoll_create(1));
}

static int lookupkey(int key)
{
    static const char *passthrough_keys
        = " -+*/<>`~!@#$%^&()_{}:;\"\',.?\\|=[]";

    int mpkey = 0;
    if ((key >= 'a' && key <= 'z') ||
        (key >= 'A' && key <= 'Z') ||
        (key >= '0' && key <= '9') ||
        (key >  0   && key <  256 && strchr(passthrough_keys, key)))
        mpkey = key;

    if (!mpkey)
        mpkey = lookup_keymap_table(keymap, key);

    return mpkey;
}

static void hide_cursor (struct vo_wayland_state *wl)
{
    struct vo_wayland_display *display = wl->display;
    if (!display->cursor.pointer || wl->vo->opts->cursor_autohide_delay == -1)
        return;

    wl_pointer_set_cursor(display->cursor.pointer, display->cursor.serial,
            NULL, 0, 0);
}

static void show_cursor (struct vo_wayland_state *wl)
{
    struct vo_wayland_display *display = wl->display;
    if (!display->cursor.pointer || wl->vo->opts->cursor_autohide_delay == -2)
        return;

    struct wl_buffer *buffer;
    struct wl_cursor_image *image;

    image = display->cursor.default_cursor->images[0];
    buffer = wl_cursor_image_get_buffer(image);
    wl_pointer_set_cursor(display->cursor.pointer, display->cursor.serial,
            display->cursor.surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(display->cursor.surface, buffer, 0, 0);
    wl_surface_damage(display->cursor.surface, 0, 0,
            image->width, image->height);
    wl_surface_commit(display->cursor.surface);
}

static void
display_watch_fd(struct vo_wayland_display *display,
                 int fd, uint32_t events, struct vo_wayland_task *task)
{
    struct epoll_event ep;

    if (display->epoll_fd < 0) {
        mp_msg(MSGT_VO, MSGL_WARN, "[wayland] Could not watch fd\n");
        return;
    }

    ep.events = events;
    ep.data.ptr = task;
    epoll_ctl(display->epoll_fd, EPOLL_CTL_ADD, fd, &ep);
}

static void display_handle_data(struct vo_wayland_task *task,
                                uint32_t events,
                                struct vo_wayland_state *wl)
{
    struct vo_wayland_display *display = wl->display;
    struct epoll_event ep;
    int ret;

    if (events & EPOLLERR || events & EPOLLHUP)
        exit(-1);

    if (events & EPOLLIN) {
        ret = wl_display_dispatch(display->display);
        if (ret == -1)
            exit(-1);
    }

    if (events & EPOLLOUT) {
        ret = wl_display_flush(display->display);
        if (ret == 0) {
            ep.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            ep.data.ptr = &display->display_task;
            epoll_ctl(display->epoll_fd, EPOLL_CTL_MOD,
                  display->display_fd, &ep);
        } else if (ret == -1 && errno != EAGAIN)
            exit(-1);
    }
}

static void cursor_timer_func(struct vo_wayland_task *task,
                              uint32_t events,
                              struct vo_wayland_state *wl)
{
    if (wl->window->type == TYPE_FULLSCREEN)
        hide_cursor(wl);
}

static void keyboard_timer_func(struct vo_wayland_task *task,
                                uint32_t events,
                                struct vo_wayland_state *wl)
{
    struct vo_wayland_input *input = wl->input;
    uint64_t exp;

    if (read(input->repeat.timer_fd, &exp, sizeof exp) != sizeof exp)
        /* If we change the timer between the fd becoming
         * readable and getting here, there'll be nothing to
         * read and we get EAGAIN. */
        return;

    keyboard_handle_key(wl, input->keyboard, 0, input->repeat.time,
            input->repeat.key, WL_KEYBOARD_KEY_STATE_PRESSED);
}

static bool create_display (struct vo_wayland_state *wl)
{
    struct vo_wayland_display *d = wl->display;

    if (d)
        return true;

    d = talloc_zero(wl, struct vo_wayland_display);
    d->display = wl_display_connect(NULL);

    wl_list_init(&d->output_list);

    if (!d->display)
        return false;

    wl->display = d;
    d->registry = wl_display_get_registry(d->display);
    wl_registry_add_listener(d->registry, &registry_listener, wl);

    wl_display_dispatch(d->display);

    d->cursor.surface =
        wl_compositor_create_surface(d->compositor);

    d->epoll_fd = os_epoll_create_cloexec();
    d->display_fd = wl_display_get_fd(d->display);
    d->display_task.run = display_handle_data;
    display_watch_fd(d, d->display_fd, EPOLLIN | EPOLLERR | EPOLLHUP,
                    &d->display_task);

    return true;
}

static void destroy_display (struct vo_wayland_state *wl)
{

    if (wl->display->cursor.theme)
        wl_cursor_theme_destroy(wl->display->cursor.theme);

    if (wl->display->cursor.surface)
        wl_surface_destroy(wl->display->cursor.surface);

    if (wl->display->shell)
        wl_shell_destroy(wl->display->shell);

    if (wl->display->compositor)
        wl_compositor_destroy(wl->display->compositor);

    wl_registry_destroy(wl->display->registry);
    wl_display_flush(wl->display->display);
    wl_display_disconnect(wl->display->display);
}

static void create_window (struct vo_wayland_state *wl)
{
    if (wl->window)
        return;

    wl->window = talloc_zero(wl, struct vo_wayland_window);

    wl->window->surface = wl_compositor_create_surface(wl->display->compositor);
    wl->window->shell_surface = wl_shell_get_shell_surface(wl->display->shell,
            wl->window->surface);

    if (wl->window->shell_surface)
        wl_shell_surface_add_listener(wl->window->shell_surface,
                &shell_surface_listener, wl);

    wl_shell_surface_set_toplevel(wl->window->shell_surface);
    wl_shell_surface_set_class(wl->window->shell_surface, "mpv");
}

static void destroy_window (struct vo_wayland_state *wl)
{
    if (wl->window->callback)
        wl_callback_destroy(wl->window->callback);

    wl_shell_surface_destroy(wl->window->shell_surface);
    wl_surface_destroy(wl->window->surface);
}

static void create_input (struct vo_wayland_state *wl)
{
    if (wl->input)
        return;

    wl->input = talloc_zero(wl, struct vo_wayland_input);

    wl->input->xkb.context = xkb_context_new(0);
    if (wl->input->xkb.context == NULL) {
        mp_msg(MSGT_VO, MSGL_ERR, "[wayland] failed to initialize input.\n");
        return;
    }
}

static void destroy_input (struct vo_wayland_state *wl)
{
    if (wl->input->keyboard)
        wl_keyboard_destroy(wl->input->keyboard);

    if (wl->input->pointer)
        wl_pointer_destroy(wl->input->pointer);

    if (wl->input->seat)
        wl_seat_destroy(wl->input->seat);

    xkb_context_unref(wl->input->xkb.context);
}

static void create_timers (struct vo_wayland_state *wl)
{
    struct vo_wayland_display *d = wl->display;
    struct vo_wayland_input *i = wl->input;

    d->cursor.task.run = cursor_timer_func;
    d->cursor.timer_fd = timerfd_create(CLOCK_MONOTONIC,
                                        TFD_CLOEXEC | TFD_NONBLOCK);
    display_watch_fd(d, d->cursor.timer_fd, EPOLLIN, &d->cursor.task);

    i->repeat.task.run = keyboard_timer_func;
    i->repeat.timer_fd = timerfd_create(CLOCK_MONOTONIC,
                                        TFD_CLOEXEC | TFD_NONBLOCK);
    display_watch_fd(d, i->repeat.timer_fd, EPOLLIN, &i->repeat.task);
}

static void destroy_timers (struct vo_wayland_state *wl)
{
    close(wl->input->repeat.timer_fd);
    close(wl->display->cursor.timer_fd);
}


/*** mplayer2 interface ***/

int vo_wayland_init (struct vo *vo)
{
    vo->wayland = talloc_zero(NULL, struct vo_wayland_state);
    struct vo_wayland_state *wl = vo->wayland;
    wl->vo = vo;

    create_input(wl);

    if (!create_display(wl)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[wayland] failed to initialize display.\n");
        return false;
    }

    create_timers(wl);
    vo->event_fd = wl->display->epoll_fd;

    create_window(wl);
    vo_wayland_update_window_title(vo);
    return 1;
}

void vo_wayland_uninit (struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wayland;
    destroy_timers(wl);
    destroy_input(wl);
    destroy_window(wl);
    destroy_display(wl);
    talloc_free(wl);
    vo->wayland = NULL;
}

void vo_wayland_ontop (struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wayland;

    vo->opts->ontop = !vo->opts->ontop;

    if (vo->opts->fs)
        vo_wayland_fullscreen(vo);
        /* use the already existing code to leave fullscreen mode and go into
         * toplevel mode */
    else
        wl_shell_surface_set_toplevel(wl->window->shell_surface);
}

void vo_wayland_border (struct vo *vo)
{
    /* wayland clienst have to do the decorations themself
     * (client side decorations) but there is no such code implement nor
     * do I plan on implementing something like client side decorations
     *
     * The only exception would be resizing on when clicking and dragging
     * on the border region of the window but this should be discussed at first
     */
}

void vo_wayland_fullscreen (struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wayland;
    if (!wl->window || !wl->display->shell)
        return;

    struct wl_output *fs_output = wl->display->fs_output;

    if (!vo->opts->fs) {
        wl->window->p_width = wl->window->width;
        wl->window->p_height = wl->window->height;
        wl_shell_surface_set_fullscreen(wl->window->shell_surface,
                WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE,
                0, fs_output);

        wl->window->type = TYPE_FULLSCREEN;
        vo->opts->fs = true;

        hide_cursor(wl);
    }

    else {
        wl_shell_surface_set_toplevel(wl->window->shell_surface);
        ssurface_schedule_resize(wl->window, wl->window->p_width,
                                             wl->window->p_height);
        wl->window->type = TYPE_TOPLEVEL;
        vo->opts->fs = false;

        show_cursor(wl);
    }
}

int vo_wayland_check_events (struct vo *vo)
{
    struct vo_wayland_task *task;
    struct vo_wayland_state *wl = vo->wayland;
    int i, ret, count;
    struct epoll_event ep[16];

    wl_display_dispatch_pending(wl->display->display);

    ret = wl_display_flush(wl->display->display);

    if (ret < 0 && errno == EAGAIN) {
        ep[0].events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
        ep[0].data.ptr = NULL;

        epoll_ctl(wl->display->epoll_fd, EPOLL_CTL_MOD,
        wl->display->display_fd, &ep[0]);
    }
    else if (ret < 0) {
        return 0;
    }

    count = epoll_wait(wl->display->epoll_fd, ep, ARRAY_LENGTH(ep), 1);

    for (i = 0; i < count; i++) {
        task = ep[i].data.ptr;
        task->run(task, ep[i].events, wl);
    }

    ret = wl->window->events;

    wl->window->events = 0;

    return ret;
}

void vo_wayland_update_screeninfo (struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wayland;
    struct mp_vo_opts *opts = vo->opts;

    wl_display_roundtrip(wl->display->display);
    if (!wl->display->output_mode_received)
        mp_msg(MSGT_VO, MSGL_ERR, "[wayland] no output mode detected\n");

    vo->xinerama_x = vo->xinerama_y = 0;

    int screen_id = 0;

    struct vo_wayland_output *output;
    struct vo_wayland_output *first_output = NULL;
    struct vo_wayland_output *fsscreen_output = NULL;

    wl_list_for_each_reverse(output, &wl->display->output_list, link) {
        if (opts->fsscreen_id == screen_id)
            fsscreen_output = output;

        if (!first_output)
            first_output = output;

        screen_id++;
    }

    if (fsscreen_output) {
        wl->display->fs_output = fsscreen_output->output;
        opts->screenwidth = fsscreen_output->width;
        opts->screenheight = fsscreen_output->height;
    }
    else {
        wl->display->fs_output = NULL; /* current output is always 0 */

        if (first_output) {
            opts->screenwidth = first_output->width;
            opts->screenheight = first_output->height;
        }
    }

    aspect_save_screenres(vo, opts->screenwidth, opts->screenheight);
}

void vo_wayland_update_window_title(struct vo *vo)
{
    struct vo_wayland_window *w = vo->wayland->window;
    wl_shell_surface_set_title(w->shell_surface, vo_get_window_title(vo));
}

