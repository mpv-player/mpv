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

#include <errno.h>
#include <stdio.h>

#include "config.h"

#if HAVE_POSIX
#include <unistd.h>
#endif

#include "common/common.h"
#include "common/msg.h"
#include "osdep/io.h"
#include "osdep/terminal.h"
#include "osdep/threads.h"
#include "terminal_swapchain.h"
#include "vo.h"

struct terminal_swapchain {
    struct vo *vo;
    mp_thread thread;
    bool started;
    mp_mutex lock;      // protects the ring and exit
    mp_cond cond;
    bstr bufs[VO_MAX_SWAPCHAIN_DEPTH + 1];
    int head;
    int in_flight;
    bool acquired;
    bstr next;          // goes out at the start of the next acquired buffer
    bool exit;
};

static void write_terminal(bstr data)
{
#if HAVE_POSIX
    // Other writers can only interrupt between calls, so a frame goes out
    // in as few of them as possible.
    while (data.len) {
        ssize_t written = write(STDOUT_FILENO, data.start, data.len);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        data = bstr_cut(data, written);
    }
#else
    fwrite(data.start, data.len, 1, stdout);
    fflush(stdout);
#endif
}

static MP_THREAD_VOID writer_thread(void *ctx)
{
    struct terminal_swapchain *sc = ctx;
    mp_thread_set_name("vo/terminal");

    mp_mutex_lock(&sc->lock);
    while (1) {
        while (!sc->in_flight && !sc->exit)
            mp_cond_wait(&sc->cond, &sc->lock);
        if (!sc->in_flight)
            break;
        bstr data = sc->bufs[sc->head];
        mp_mutex_unlock(&sc->lock);

        terminal_lock_output();
        write_terminal(data);
        terminal_unlock_output();

        mp_mutex_lock(&sc->lock);
        sc->head = (sc->head + 1) % MP_ARRAY_SIZE(sc->bufs);
        sc->in_flight--;
        mp_cond_broadcast(&sc->cond);
    }
    mp_mutex_unlock(&sc->lock);
    MP_THREAD_RETURN();
}

struct terminal_swapchain *terminal_swapchain_create(struct vo *vo)
{
    struct terminal_swapchain *sc = talloc_zero(NULL, struct terminal_swapchain);
    sc->vo = vo;
    mp_mutex_init(&sc->lock);
    mp_cond_init(&sc->cond);
    sc->started = mp_thread_create(&sc->thread, writer_thread, sc) == 0;
    if (!sc->started)
        MP_WARN(vo, "Failed to create the terminal writer thread, writing inline.\n");
    return sc;
}

static bstr *acquired_buffer(struct terminal_swapchain *sc)
{
    return &sc->bufs[(sc->head + sc->in_flight) % MP_ARRAY_SIZE(sc->bufs)];
}

bstr *terminal_swapchain_acquire(struct terminal_swapchain *sc)
{
    int depth = MPCLAMP(sc->vo->opts->swapchain_depth, 1, VO_MAX_SWAPCHAIN_DEPTH);
    mp_mutex_lock(&sc->lock);
    mp_assert(!sc->acquired);
    while (sc->in_flight >= depth)
        mp_cond_wait(&sc->cond, &sc->lock);
    sc->acquired = true;
    bstr *buf = acquired_buffer(sc);
    mp_mutex_unlock(&sc->lock);
    buf->len = 0;
    bstr_xappend(NULL, buf, sc->next);
    sc->next.len = 0;
    return buf;
}

void terminal_swapchain_present(struct terminal_swapchain *sc, bstr *frame)
{
    mp_mutex_lock(&sc->lock);
    mp_assert(sc->acquired && frame == acquired_buffer(sc));
    sc->acquired = false;
    if (frame->len && sc->started) {
        sc->in_flight++;
        mp_cond_broadcast(&sc->cond);
        mp_mutex_unlock(&sc->lock);
        return;
    }
    mp_mutex_unlock(&sc->lock);

    if (frame->len) {
        terminal_lock_output();
        write_terminal(*frame);
        terminal_unlock_output();
    }
}

bstr *terminal_swapchain_next(struct terminal_swapchain *sc)
{
    mp_assert(!sc->acquired);
    return &sc->next;
}

void terminal_swapchain_wait(struct terminal_swapchain *sc)
{
    mp_mutex_lock(&sc->lock);
    while (sc->in_flight)
        mp_cond_wait(&sc->cond, &sc->lock);
    mp_mutex_unlock(&sc->lock);
}

void terminal_swapchain_destroy(struct terminal_swapchain *sc)
{
    if (!sc)
        return;
    if (sc->started) {
        mp_mutex_lock(&sc->lock);
        sc->exit = true;
        mp_cond_broadcast(&sc->cond);
        mp_mutex_unlock(&sc->lock);
        mp_thread_join(sc->thread);
    }
    mp_assert(!sc->next.len);
    talloc_free(sc->next.start);
    for (int i = 0; i < MP_ARRAY_SIZE(sc->bufs); i++)
        talloc_free(sc->bufs[i].start);
    mp_cond_destroy(&sc->cond);
    mp_mutex_destroy(&sc->lock);
    talloc_free(sc);
}
