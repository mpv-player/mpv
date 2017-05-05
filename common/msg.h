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

#ifndef MPLAYER_MP_MSG_H
#define MPLAYER_MP_MSG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "osdep/compiler.h"

struct mp_log;

// A mp_log instance that never outputs anything.
extern struct mp_log *const mp_null_log;

// Verbosity levels.
enum {
    MSGL_FATAL,     // will exit/abort (note: msg.c doesn't exit or abort)
    MSGL_ERR,       // continues
    MSGL_WARN,      // only warning
    MSGL_INFO,      // -quiet
    MSGL_STATUS,    // exclusively for the playback status line
    MSGL_V,         // -v
    MSGL_DEBUG,     // -v -v
    MSGL_TRACE,     // -v -v -v
    MSGL_STATS,     // dumping fine grained stats (--dump-stats)

    MSGL_MAX = MSGL_STATS,
};

struct mp_log *mp_log_new(void *talloc_ctx, struct mp_log *parent,
                          const char *name);

void mp_msg(struct mp_log *log, int lev, const char *format, ...)
    PRINTF_ATTRIBUTE(3, 4);
void mp_msg_va(struct mp_log *log, int lev, const char *format, va_list va);

bool mp_msg_test(struct mp_log *log, int lev);

// Convenience macros.
#define mp_fatal(log, ...)      mp_msg(log, MSGL_FATAL, __VA_ARGS__)
#define mp_err(log, ...)        mp_msg(log, MSGL_ERR, __VA_ARGS__)
#define mp_warn(log, ...)       mp_msg(log, MSGL_WARN, __VA_ARGS__)
#define mp_info(log, ...)       mp_msg(log, MSGL_INFO, __VA_ARGS__)
#define mp_verbose(log, ...)    mp_msg(log, MSGL_V, __VA_ARGS__)
#define mp_dbg(log, ...)        mp_msg(log, MSGL_DEBUG, __VA_ARGS__)
#define mp_trace(log, ...)      mp_msg(log, MSGL_TRACE, __VA_ARGS__)

// Convenience macros, typically called with a pointer to a context struct
// as first argument, which has a "struct mp_log *log;" member.

#define MP_MSG(obj, lev, ...)   mp_msg((obj)->log, lev, __VA_ARGS__)

#define MP_FATAL(obj, ...)      MP_MSG(obj, MSGL_FATAL, __VA_ARGS__)
#define MP_ERR(obj, ...)        MP_MSG(obj, MSGL_ERR, __VA_ARGS__)
#define MP_WARN(obj, ...)       MP_MSG(obj, MSGL_WARN, __VA_ARGS__)
#define MP_INFO(obj, ...)       MP_MSG(obj, MSGL_INFO, __VA_ARGS__)
#define MP_VERBOSE(obj, ...)    MP_MSG(obj, MSGL_V, __VA_ARGS__)
#define MP_DBG(obj, ...)        MP_MSG(obj, MSGL_DEBUG, __VA_ARGS__)
#define MP_TRACE(obj, ...)      MP_MSG(obj, MSGL_TRACE, __VA_ARGS__)

// This is a bit special. See TOOLS/stats-conv.py what rules text passed
// to these functions should follow. Also see --dump-stats.
#define mp_stats(obj, ...)      mp_msg(obj, MSGL_STATS, __VA_ARGS__)
#define MP_STATS(obj, ...)      MP_MSG(obj, MSGL_STATS, __VA_ARGS__)

#endif /* MPLAYER_MP_MSG_H */
