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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpv/client.h>

#include "osdep/compiler.h"

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

MP_UNUSED static void check_double(const char *property, double expect)
{
    double result_double;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_DOUBLE, &result_double));
    if (expect != result_double)
        fail("Double: expected '%f' but got '%f'!\n", expect, result_double);
}

MP_UNUSED static void check_flag(const char *property, int expect)
{
    int result_flag;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_FLAG, &result_flag));
    if (expect != result_flag)
        fail("Flag: expected '%d' but got '%d'!\n", expect, result_flag);
}

MP_UNUSED static void check_int(const char *property, int64_t expect)
{
    int64_t result_int;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_INT64, &result_int));
    if (expect != result_int)
        fail("Int: expected '%" PRId64 "' but got '%" PRId64 "'!\n", expect, result_int);
}

MP_UNUSED static inline void check_string(const char *property, const char *expect)
{
    char *result_string;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_STRING, &result_string));
    if (strcmp(expect, result_string) != 0)
        fail("String: expected '%s' but got '%s'!\n", expect, result_string);
    mpv_free(result_string);
}

static inline void initialize(void)
{
    check_api_error(mpv_set_option_string(ctx, "vo", "null"));
    check_api_error(mpv_set_option_string(ctx, "ao", "null"));
    check_api_error(mpv_request_log_messages(ctx, "debug"));
    check_api_error(mpv_initialize(ctx));
}

static inline void reload_file(const char *path)
{
    const char *cmd[] = {"loadfile", path, NULL};
    check_api_error(mpv_command(ctx, cmd));
    bool loaded = false;
    while (!loaded) {
        mpv_event *event = wrap_wait_event();
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            loaded = true;
            break;
        }
    }
}
