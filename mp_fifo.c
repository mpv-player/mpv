/*
 * This file is part of MPlayer.
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

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "osdep/timer.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "mp_fifo.h"
#include "talloc.h"
#include "options.h"


struct mp_fifo {
    struct MPOpts *opts;
    int *data;
    int readpos;
    int size;
    int num_entries;
    int max_up;
    int num_up;
    int last_key_down;
    unsigned last_down_time;
};

struct mp_fifo *mp_fifo_create(struct MPOpts *opts)
{
    struct mp_fifo *fifo = talloc_zero(NULL, struct mp_fifo);
    fifo->opts = opts;
    /* Typical mouse wheel use will generate a sequence repeating 3 events:
     * down, doubleclick, up, down, doubleclick, up, ...
     * Normally only one of those event types triggers a command,
     * so allow opts->key_fifo_size such repeats.
     */
    fifo->max_up = opts->key_fifo_size;
    fifo->size = opts->key_fifo_size * 3;
    fifo->data = talloc_array_ptrtype(fifo, fifo->data, fifo->size);
    return fifo;
}

static bool is_up(int code)
{
    return code > 0 && !(code & MP_KEY_DOWN)
        && !(code >= MOUSE_BTN0_DBL && code < MOUSE_BTN_DBL_END);
}

static int fifo_peek(struct mp_fifo *fifo, int offset)
{
    return fifo->data[(fifo->readpos + offset) % fifo->size];
}

static int fifo_read(struct mp_fifo *fifo)
{
    int code = fifo_peek(fifo, 0);
    fifo->readpos += 1;
    fifo->readpos %= fifo->size;
    fifo->num_entries--;
    fifo->num_up -= is_up(code);
    assert(fifo->num_entries >= 0);
    assert(fifo->num_up >= 0);
    return code;
}

static void fifo_write(struct mp_fifo *fifo, int code)
{
    fifo->data[(fifo->readpos + fifo->num_entries) % fifo->size] = code;
    fifo->num_entries++;
    fifo->num_up += is_up(code);
    assert(fifo->num_entries <= fifo->size);
    assert(fifo->num_up <= fifo->max_up);
}

static void mplayer_put_key_internal(struct mp_fifo *fifo, int code)
{
    // Clear key-down state if we're forced to drop entries
    if (fifo->num_entries >= fifo->size - 1
        || fifo->num_up >= fifo->max_up) {
        if (fifo_peek(fifo, fifo->num_entries - 1) != MP_INPUT_RELEASE_ALL)
            fifo_write(fifo, MP_INPUT_RELEASE_ALL);
    } else
        fifo_write(fifo, code);
}

int mplayer_get_key(void *ctx, int fd)
{
    struct mp_fifo *fifo = ctx;
    if (!fifo->num_entries)
        return MP_INPUT_NOTHING;
    return fifo_read(fifo);
}

static void put_double(struct mp_fifo *fifo, int code)
{
  if (code >= MOUSE_BTN0 && code < MOUSE_BTN_END)
      mplayer_put_key_internal(fifo, code - MOUSE_BTN0 + MOUSE_BTN0_DBL);
}

void mplayer_put_key(struct mp_fifo *fifo, int code)
{
    unsigned now = GetTimerMS();
    int doubleclick_time = fifo->opts->doubleclick_time;
    // ignore system-doubleclick if we generate these events ourselves
    if (doubleclick_time
        && (code & ~MP_KEY_DOWN) >= MOUSE_BTN0_DBL
        && (code & ~MP_KEY_DOWN) < MOUSE_BTN_DBL_END)
        return;
    mplayer_put_key_internal(fifo, code);
    if (code & MP_KEY_DOWN) {
        code &= ~MP_KEY_DOWN;
        if (fifo->last_key_down == code
            && now - fifo->last_down_time < doubleclick_time)
            put_double(fifo, code);
        fifo->last_key_down = code;
        fifo->last_down_time = now;
    }
}
