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

#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "osdep/compiler.h"
#include "talloc.h"

// double should be able to represent this exactly
#define MP_NOPTS_VALUE (-0x1p+63)

#define MP_CONCAT_(a, b) a ## b
#define MP_CONCAT(a, b) MP_CONCAT_(a, b)

#define ROUND(x) ((int)((x) < 0 ? (x) - 0.5 : (x) + 0.5))

#define MPMAX(a, b) ((a) > (b) ? (a) : (b))
#define MPMIN(a, b) ((a) > (b) ? (b) : (a))
#define MPCLAMP(a, min, max) (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))
#define MPSWAP(type, a, b) \
    do { type SWAP_tmp = b; b = a; a = SWAP_tmp; } while (0)
#define MP_ARRAY_SIZE(s) (sizeof(s) / sizeof((s)[0]))

// align must be a power of two (align >= 1), x >= 0
#define MP_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define MP_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define CONTROL_OK 1
#define CONTROL_TRUE 1
#define CONTROL_FALSE 0
#define CONTROL_UNKNOWN -1
#define CONTROL_ERROR -2
#define CONTROL_NA -3

enum stream_type {
    STREAM_VIDEO,
    STREAM_AUDIO,
    STREAM_SUB,
    STREAM_TYPE_COUNT,
};

extern const char *const mpv_version;
extern const char *const mpv_builddate;

char *mp_format_time(double time, bool fractions);
char *mp_format_time_fmt(const char *fmt, double time);

struct mp_rect {
    int x0, y0;
    int x1, y1;
};

#define mp_rect_w(r) ((r).x1 - (r).x0)
#define mp_rect_h(r) ((r).y1 - (r).y0)

void mp_rect_union(struct mp_rect *rc, const struct mp_rect *src);
bool mp_rect_intersection(struct mp_rect *rc, const struct mp_rect *rc2);
bool mp_rect_contains(struct mp_rect *rc, int x, int y);

int mp_snprintf_cat(char *str, size_t size, const char *format, ...)
    PRINTF_ATTRIBUTE(3, 4);

struct bstr;

void mp_append_utf8_bstr(void *talloc_ctx, struct bstr *buf, uint32_t codepoint);

bool mp_append_escaped_string_noalloc(void *talloc_ctx, struct bstr *dst,
                                      struct bstr *src);
bool mp_append_escaped_string(void *talloc_ctx, struct bstr *dst,
                              struct bstr *src);

char *mp_strerror_buf(char *buf, size_t buf_size, int errnum);
#define mp_strerror(e) mp_strerror_buf((char[80]){0}, 80, e)

#endif /* MPLAYER_MPCOMMON_H */
