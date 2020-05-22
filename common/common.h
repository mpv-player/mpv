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

#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "osdep/compiler.h"
#include "mpv_talloc.h"

// double should be able to represent this exactly
#define MP_NOPTS_VALUE (-0x1p+63)

#define MP_CONCAT_(a, b) a ## b
#define MP_CONCAT(a, b) MP_CONCAT_(a, b)

#define MPMAX(a, b) ((a) > (b) ? (a) : (b))
#define MPMIN(a, b) ((a) > (b) ? (b) : (a))
#define MPCLAMP(a, min, max) (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))
#define MPSWAP(type, a, b) \
    do { type SWAP_tmp = b; b = a; a = SWAP_tmp; } while (0)
#define MP_ARRAY_SIZE(s) (sizeof(s) / sizeof((s)[0]))

// align must be a power of two (align >= 1), x >= 0
#define MP_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define MP_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define MP_IS_ALIGNED(x, align) (!((x) & ((align) - 1)))
#define MP_IS_POWER_OF_2(x) ((x) > 0 && !((x) & ((x) - 1)))

// Return "a", or if that is NOPTS, return "def".
#define MP_PTS_OR_DEF(a, def) ((a) == MP_NOPTS_VALUE ? (def) : (a))
// If one of the values is NOPTS, always pick the other one.
#define MP_PTS_MIN(a, b) MPMIN(MP_PTS_OR_DEF(a, b), MP_PTS_OR_DEF(b, a))
#define MP_PTS_MAX(a, b) MPMAX(MP_PTS_OR_DEF(a, b), MP_PTS_OR_DEF(b, a))
// Return a+b, unless a is NOPTS. b must not be NOPTS.
#define MP_ADD_PTS(a, b) ((a) == MP_NOPTS_VALUE ? (a) : ((a) + (b)))

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

extern const char mpv_version[];
extern const char mpv_builddate[];
extern const char mpv_copyright[];

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
bool mp_rect_equals(struct mp_rect *rc1, struct mp_rect *rc2);
int mp_rect_subtract(const struct mp_rect *rc1, const struct mp_rect *rc2,
                     struct mp_rect res_array[4]);

unsigned int mp_log2(uint32_t v);
uint32_t mp_round_next_power_of_2(uint32_t v);

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

char *mp_tag_str_buf(char *buf, size_t buf_size, uint32_t tag);
#define mp_tag_str(t) mp_tag_str_buf((char[22]){0}, 22, t)

// Return a printf(format, ...) formatted string of the given SIZE. SIZE must
// be a compile time constant. The result is allocated on the stack and valid
// only within the current block scope.
#define mp_tprintf(SIZE, format, ...) \
    mp_tprintf_buf((char[SIZE]){0}, (SIZE), (format), __VA_ARGS__)
char *mp_tprintf_buf(char *buf, size_t buf_size, const char *format, ...)
    PRINTF_ATTRIBUTE(3, 4);

char **mp_dup_str_array(void *tctx, char **s);

// We generally do not handle allocation failure of small malloc()s. This would
// create a large number of rarely tested code paths, which would probably
// regress and cause security issues. We prefer to fail fast.
// This macro generally behaves like an assert(), except it will make sure to
// kill the process even with NDEBUG.
#define MP_HANDLE_OOM(x) do {   \
        assert(x);              \
        if (!(x))               \
            abort();            \
    } while (0)

#endif /* MPLAYER_MPCOMMON_H */
