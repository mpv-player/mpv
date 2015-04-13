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

#ifndef MP_DISPATCH_H_
#define MP_DISPATCH_H_

typedef void (*mp_dispatch_fn)(void *data);
struct mp_dispatch_queue;

struct mp_dispatch_queue *mp_dispatch_create(void *talloc_parent);
void mp_dispatch_set_wakeup_fn(struct mp_dispatch_queue *queue,
                               void (*wakeup_fn)(void *wakeup_ctx),
                               void *wakeup_ctx);
void mp_dispatch_enqueue(struct mp_dispatch_queue *queue,
                         mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_enqueue_autofree(struct mp_dispatch_queue *queue,
                                  mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_run(struct mp_dispatch_queue *queue,
                     mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_queue_process(struct mp_dispatch_queue *queue, double timeout);
void mp_dispatch_suspend(struct mp_dispatch_queue *queue);
void mp_dispatch_resume(struct mp_dispatch_queue *queue);
void mp_dispatch_lock(struct mp_dispatch_queue *queue);
void mp_dispatch_unlock(struct mp_dispatch_queue *queue);

#endif
