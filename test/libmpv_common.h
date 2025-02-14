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

#include <inttypes.h>
#include <libmpv/client.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stolen from osdep/compiler.h
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format(printf, a1, a2)))
#define MP_NORETURN __attribute__((noreturn))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#define MP_NORETURN
#endif

// Broken crap with __USE_MINGW_ANSI_STDIO
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#undef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#endif

// Global handle.
static mpv_handle *ctx;
MP_NORETURN PRINTF_ATTRIBUTE(1, 2)

static inline void fail(const char *fmt, ...)
{
    if (fmt) {
        va_list va;
        va_start(va, fmt);
        vfprintf(stderr, fmt, va);
        va_end(va);
    }
    exit(1);
}

static inline void exit_cleanup(void)
{
    mpv_destroy(ctx);
    ctx = NULL;
}

static inline mpv_event *wrap_wait_event(void)
{
    while (1) {
        mpv_event *ev = mpv_wait_event(ctx, 1);
        if (ev->event_id == MPV_EVENT_NONE)
            continue;

        if (ev->event_id == MPV_EVENT_LOG_MESSAGE) {
            mpv_event_log_message *msg = (mpv_event_log_message*)ev->data;
            printf("[%s:%s] %s", msg->prefix, msg->level, msg->text);
            if (msg->log_level <= MPV_LOG_LEVEL_ERROR)
                fail("error was logged");
        } else {
            return ev;
        }
    }
}

static inline void check_api_error(int status)
{
    if (status < 0)
        fail("mpv API error: %s\n", mpv_error_string(status));
}

static inline void initialize(void)
{
    check_api_error(mpv_set_option_string(ctx, "vo", "null"));
    check_api_error(mpv_set_option_string(ctx, "ao", "null"));
    check_api_error(mpv_request_log_messages(ctx, "debug"));
    check_api_error(mpv_initialize(ctx));
}
