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
    struct input_ctx *input;
    int last_key_down;
    unsigned last_down_time;
};

struct mp_fifo *mp_fifo_create(struct input_ctx *input, struct MPOpts *opts)
{
    struct mp_fifo *fifo = talloc_zero(NULL, struct mp_fifo);
    fifo->input = input;
    fifo->opts = opts;
    return fifo;
}

static void put_double(struct mp_fifo *fifo, int code)
{
  if (code >= MOUSE_BTN0 && code < MOUSE_BTN_END)
      mp_input_feed_key(fifo->input, code - MOUSE_BTN0 + MOUSE_BTN0_DBL);
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
    mp_input_feed_key(fifo->input, code);
    if (code & MP_KEY_DOWN) {
        code &= ~MP_KEY_DOWN;
        if (fifo->last_key_down == code
            && now - fifo->last_down_time < doubleclick_time)
            put_double(fifo, code);
        fifo->last_key_down = code;
        fifo->last_down_time = now;
    }
}
