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

// Verbosity levels.
#define MSGL_FATAL 0  // will exit/abort (note: msg.c doesn't exit or abort)
#define MSGL_ERR 1    // continues
#define MSGL_WARN 2   // only warning
#define MSGL_HINT 3   // (to be phased out)
#define MSGL_INFO 4   // -quiet
#define MSGL_STATUS 5 // exclusively for the playback status line
#define MSGL_V 6      // -v
#define MSGL_DBG2 7   // -v -v
#define MSGL_DBG3 8   // ...
#define MSGL_DBG4 9   // ....
#define MSGL_DBG5 10  // .....

struct mp_log *mp_log_new(void *talloc_ctx, struct mp_log *parent,
                          const char *name);

void mp_msg_log(struct mp_log *log, int lev, const char *format, ...)
    PRINTF_ATTRIBUTE(3, 4);

// Convenience macros, typically called with a pointer to a context struct
// as first argument, which has a "struct mp_log log;" member.

#define MP_MSG(obj, lev, ...)   mp_msg_log((obj)->log, lev, __VA_ARGS__)
#define MP_MSGT(obj, lev, ...)  mp_msgt_log((obj)->log, lev, __VA_ARGS__)

#define MP_FATAL(obj, ...)      MP_MSG(obj, MSGL_FATAL, __VA_ARGS__)
#define MP_ERR(obj, ...)        MP_MSG(obj, MSGL_ERR, __VA_ARGS__)
#define MP_WARN(obj, ...)       MP_MSG(obj, MSGL_WARN, __VA_ARGS__)
#define MP_INFO(obj, ...)       MP_MSG(obj, MSGL_INFO, __VA_ARGS__)
#define MP_VERBOSE(obj, ...)    MP_MSG(obj, MSGL_V, __VA_ARGS__)
#define MP_DBG(obj, ...)        MP_MSG(obj, MSGL_DBG2, __VA_ARGS__)
#define MP_TRACE(obj, ...)      MP_MSG(obj, MSGL_DBG5, __VA_ARGS__)

#define mp_fatal(log, ...)      mp_msg_log(log, MSGL_FATAL, __VA_ARGS__)
#define mp_err(log, ...)        mp_msg_log(log, MSGL_ERR, __VA_ARGS__)
#define mp_warn(log, ...)       mp_msg_log(log, MSGL_WARN, __VA_ARGS__)
#define mp_info(log, ...)       mp_msg_log(log, MSGL_INFO, __VA_ARGS__)
#define mp_verbose(log, ...)    mp_msg_log(log, MSGL_V, __VA_ARGS__)
#define mp_dbg(log, ...)        mp_msg_log(log, MSGL_DBG2, __VA_ARGS__)
#define mp_trace(log, ...)      mp_msg_log(log, MSGL_DBG5, __VA_ARGS__)

struct mpv_global;
void mp_msg_init(struct mpv_global *global);
void mp_msg_uninit(struct mpv_global *global);

struct mpv_global *mp_log_get_global(struct mp_log *log);

// --- Legacy

// Note: using mp_msg_log or the MP_ERR/... macros is preferred.
int mp_msg_test(int mod, int lev);
bool mp_msg_test_log(struct mp_log *log, int lev);
void mp_msg_va(int mod, int lev, const char *format, va_list va);
void mp_msg(int mod, int lev, const char *format, ... ) PRINTF_ATTRIBUTE(3, 4);

#define MSGL_FIXME 1  // for conversions from printf where the appropriate MSGL is not known; set equal to ERR for obtrusiveness
#define MSGT_FIXME 0  // for conversions from printf where the appropriate MSGT is not known; set equal to GLOBAL for
#define MSGT_GLOBAL 0        // common player stuff errors
#define MSGT_CPLAYER 1       // console player (mplayer.c)
#define MSGT_VO 3              // libvo
#define MSGT_AO 4              // libao
#define MSGT_DEMUXER 5    // demuxer.c (general stuff)
#define MSGT_DS 6         // demux stream (add/read packet etc)
#define MSGT_DEMUX 7      // fileformat-specific stuff (demux_*.c)
#define MSGT_HEADER 8     // fileformat-specific header (*header.c)
#define MSGT_AVSYNC 9     // mplayer.c timer stuff
#define MSGT_AUTOQ 10     // mplayer.c auto-quality stuff
#define MSGT_CFGPARSER 11 // cfgparser.c
#define MSGT_DECAUDIO 12  // av decoder
#define MSGT_DECVIDEO 13
#define MSGT_SEEK 14    // seeking code
#define MSGT_WIN32 15   // win32 dll stuff
#define MSGT_OPEN 16    // open.c (stream opening)
#define MSGT_DVD 17     // open.c (DVD init/read/seek)
#define MSGT_PARSEES 18 // parse_es.c (mpeg stream parser)
#define MSGT_LIRC 19    // lirc_mp.c and input lirc driver
#define MSGT_STREAM 20  // stream.c
#define MSGT_CACHE 21   // cache2.c
#define MSGT_ENCODE 22 // now encode_lavc.c
#define MSGT_XACODEC 23 // XAnim codecs
#define MSGT_TV 24      // TV input subsystem
#define MSGT_OSDEP 25   // OS-dependent parts
#define MSGT_SPUDEC 26  // spudec.c
#define MSGT_PLAYTREE 27    // Playtree handeling (playtree.c, playtreeparser.c)
#define MSGT_INPUT 28
#define MSGT_VFILTER 29
#define MSGT_OSD 30
#define MSGT_NETWORK 31
#define MSGT_CPUDETECT 32
#define MSGT_CODECCFG 33
#define MSGT_SWS 34
#define MSGT_VOBSUB 35
#define MSGT_SUBREADER 36
#define MSGT_AFILTER 37  // Audio filter messages
#define MSGT_NETST 38 // Netstream
#define MSGT_MUXER 39 // muxer layer
#define MSGT_IDENTIFY 41  // -identify output
#define MSGT_RADIO 42
#define MSGT_ASS 43 // libass messages
#define MSGT_LOADER 44 // dll loader messages
#define MSGT_STATUSLINE 45 // playback/encoding status line
#define MSGT_TELETEXT 46       // Teletext decoder
#define MSGT_MAX 47

#endif /* MPLAYER_MP_MSG_H */
