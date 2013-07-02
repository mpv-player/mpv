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

#ifndef MPLAYER_MP_FIFO_H
#define MPLAYER_MP_FIFO_H

#include "core/bstr.h"
#include "core/input/input.h"

struct mp_fifo;
struct input_ctx;

// New code should use the wrapped functions directly.

static inline void mplayer_put_key(struct mp_fifo *fifo, int code)
{
    mp_input_put_key((struct input_ctx *)fifo, code);
}

static inline void mplayer_put_key_utf8(struct mp_fifo *fifo, int mods, struct bstr t)
{
    mp_input_put_key_utf8((struct input_ctx *)fifo, mods, t);
}

#endif /* MPLAYER_MP_FIFO_H */
