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

#ifndef MPLAYER_MP_MSG_H
#define MPLAYER_MP_MSG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "compat/compiler.h"

struct mp_log;

extern int verbose;
extern bool mp_msg_mute;
extern bool mp_msg_stdout_in_use;
extern int mp_smode; // slave mode compatibility glue

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
    MSGL_SMODE,     // old slave mode (-identify)
};

struct mp_log *mp_log_new(void *talloc_ctx, struct mp_log *parent,
                          const char *name);

void mp_msg_log(struct mp_log *log, int lev, const char *format, ...)
    PRINTF_ATTRIBUTE(3, 4);
void mp_msg_log_va(struct mp_log *log, int lev, const char *format, va_list va);

bool mp_msg_test_log(struct mp_log *log, int lev);

// Convenience macros.
#define mp_fatal(log, ...)      mp_msg_log(log, MSGL_FATAL, __VA_ARGS__)
#define mp_err(log, ...)        mp_msg_log(log, MSGL_ERR, __VA_ARGS__)
#define mp_warn(log, ...)       mp_msg_log(log, MSGL_WARN, __VA_ARGS__)
#define mp_info(log, ...)       mp_msg_log(log, MSGL_INFO, __VA_ARGS__)
#define mp_verbose(log, ...)    mp_msg_log(log, MSGL_V, __VA_ARGS__)
#define mp_dbg(log, ...)        mp_msg_log(log, MSGL_DEBUG, __VA_ARGS__)
#define mp_trace(log, ...)      mp_msg_log(log, MSGL_TRACE, __VA_ARGS__)

// Convenience macros, typically called with a pointer to a context struct
// as first argument, which has a "struct mp_log log;" member.

#define MP_MSG(obj, lev, ...)   mp_msg_log((obj)->log, lev, __VA_ARGS__)

#define MP_FATAL(obj, ...)      MP_MSG(obj, MSGL_FATAL, __VA_ARGS__)
#define MP_ERR(obj, ...)        MP_MSG(obj, MSGL_ERR, __VA_ARGS__)
#define MP_WARN(obj, ...)       MP_MSG(obj, MSGL_WARN, __VA_ARGS__)
#define MP_INFO(obj, ...)       MP_MSG(obj, MSGL_INFO, __VA_ARGS__)
#define MP_VERBOSE(obj, ...)    MP_MSG(obj, MSGL_V, __VA_ARGS__)
#define MP_DBG(obj, ...)        MP_MSG(obj, MSGL_DEBUG, __VA_ARGS__)
#define MP_TRACE(obj, ...)      MP_MSG(obj, MSGL_TRACE, __VA_ARGS__)
#define MP_SMODE(obj, ...)      MP_MSG(obj, MSGL_SMODE, __VA_ARGS__)

struct mpv_global;
void mp_msg_init(struct mpv_global *global);
void mp_msg_uninit(struct mpv_global *global);
void mp_msg_update_msglevels(struct mpv_global *global);

struct mpv_global *mp_log_get_global(struct mp_log *log);

struct bstr;
int mp_msg_split_msglevel(struct bstr *s, struct bstr *out_mod, int *out_level);

#endif /* MPLAYER_MP_MSG_H */
