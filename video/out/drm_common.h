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

#ifndef MP_VT_SWITCHER_H
#define MP_VT_SWITCHER_H

struct vt_switcher {
    int tty_fd;
    struct mp_log *log;
    void (*handlers[2])(void*);
    void *handler_data[2];
};

int vt_switcher_init(struct vt_switcher *s, struct mp_log *log);
void vt_switcher_destroy(struct vt_switcher *s);
void vt_switcher_poll(struct vt_switcher *s, int timeout_ms);
void vt_switcher_interrupt_poll(struct vt_switcher *s);

void vt_switcher_acquire(struct vt_switcher *s, void (*handler)(void*), void *user_data);
void vt_switcher_release(struct vt_switcher *s, void (*handler)(void*), void *user_data);

#endif
