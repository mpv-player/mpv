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
#include "osdep/timer.h"
#include "input/input.h"
#include "input/mouse.h"
#include "mp_fifo.h"
#include "talloc.h"
#include "options.h"


struct mp_fifo {
    struct MPOpts *opts;
    int *data;
    int readpos;
    int writepos;
    int size;
    unsigned last_key_time[2];
    int last_key[2];
};

struct mp_fifo *mp_fifo_create(struct MPOpts *opts)
{
    struct mp_fifo *fifo = talloc_zero(NULL, struct mp_fifo);
    fifo->opts = opts;
    fifo->size = opts->key_fifo_size;
    fifo->data = talloc_array_ptrtype(fifo, fifo->data, fifo->size);
    return fifo;
}

static void mplayer_put_key_internal(struct mp_fifo *fifo, int code)
{
    int fifo_free = fifo->readpos - fifo->writepos - 1;
    if (fifo_free < 0)
        fifo_free += fifo->size;
    if (!fifo_free)
        return; // FIFO FULL!!
    // reserve some space for key release events to avoid stuck keys
    if((code & MP_KEY_DOWN) && fifo_free < (fifo->size >> 1))
        return;
    fifo->data[fifo->writepos++] = code;
    fifo->writepos %= fifo->size;
}

int mplayer_get_key(void *ctx, int fd)
{
    struct mp_fifo *fifo = ctx;
    if (fifo->writepos == fifo->readpos)
        return MP_INPUT_NOTHING;
    int key = fifo->data[fifo->readpos++];
    fifo->readpos %= fifo->size;
    return key;
}

static void put_double(struct mp_fifo *fifo, int code)
{
  if (code >= MOUSE_BTN0 && code <= MOUSE_BTN9)
      mplayer_put_key_internal(fifo, code - MOUSE_BTN0 + MOUSE_BTN0_DBL);
}

void mplayer_put_key(struct mp_fifo *fifo, int code)
{
    unsigned now = GetTimerMS();
    int doubleclick_time = fifo->opts->doubleclick_time;
    // ignore system-doubleclick if we generate these events ourselves
    if (doubleclick_time
        && (code & ~MP_KEY_DOWN) >= MOUSE_BTN0_DBL
        && (code & ~MP_KEY_DOWN) <= MOUSE_BTN9_DBL)
        return;
    mplayer_put_key_internal(fifo, code);
    if (code & MP_KEY_DOWN) {
        code &= ~MP_KEY_DOWN;
        fifo->last_key[1] = fifo->last_key[0];
        fifo->last_key[0] = code;
        fifo->last_key_time[1] = fifo->last_key_time[0];
        fifo->last_key_time[0] = now;
        if (fifo->last_key[1] == code
            && now - fifo->last_key_time[1] < doubleclick_time)
            put_double(fifo, code);
        return;
    }
    if (fifo->last_key[0] == code && fifo->last_key[1] == code
        && now - fifo->last_key_time[1] < doubleclick_time)
        put_double(fifo, code);
}
