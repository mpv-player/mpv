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

#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "compat/compiler.h"
#include "core/mp_talloc.h"

// both int64_t and double should be able to represent this exactly
#define MP_NOPTS_VALUE (-1LL<<63)

#define MP_CONCAT_(a, b) a ## b
#define MP_CONCAT(a, b) MP_CONCAT_(a, b)

#define ROUND(x) ((int)((x) < 0 ? (x) - 0.5 : (x) + 0.5))

#define CONTROL_OK 1
#define CONTROL_TRUE 1
#define CONTROL_FALSE 0
#define CONTROL_UNKNOWN -1
#define CONTROL_ERROR -2
#define CONTROL_NA -3

extern const char *mplayer_version;
extern const char *mplayer_builddate;

char *mp_format_time(double time, bool fractions);

struct mp_rect {
    int x0, y0;
    int x1, y1;
};

void mp_rect_union(struct mp_rect *rc, const struct mp_rect *src);
bool mp_rect_intersection(struct mp_rect *rc, const struct mp_rect *rc2);

char *mp_append_utf8_buffer(char *buffer, uint32_t codepoint);

struct bstr;
bool mp_parse_escape(struct bstr *code, char **str);

#endif /* MPLAYER_MPCOMMON_H */
