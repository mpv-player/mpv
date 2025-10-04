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

#define command(cmd) command_impl(__FILE__, __LINE__, (cmd))
#define command_string(cmd) command_string_impl(__FILE__, __LINE__, (cmd))
#define get_property(property, format, result) \
    get_property_impl(__FILE__, __LINE__, (property), (format), (result))
#define set_option_or_property(property, format, value, option) \
    set_option_or_property_impl(__FILE__, __LINE__, (property), (format), (value), (option))
#define set_property_string(property, value) \
    set_property_string_impl(__FILE__, __LINE__, (property), (value))

// Global handle.
static mpv_handle *ctx;
MP_NORETURN MP_PRINTF_ATTRIBUTE(1, 2)

static inline void fail(const char *fmt, ...)
{
    if (fmt) {
        va_list va;
        va_start(va, fmt);
        vfprintf(stderr, fmt, va);
        va_end(va);
    }
    fflush(stdout);
    fflush(stderr);
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

static inline void command_impl(const char *file, int line, const char *cmd[])
{
    int ret = mpv_command(ctx, cmd);
    if (ret < 0)
        fail("mpv API error while running command '%s' at %s:%d (%s)\n",
             cmd[0], file, line, mpv_error_string(ret));
}

static inline void command_string_impl(const char *file, int line, const char *cmd)
{
    int ret = mpv_command_string(ctx, cmd);
    if (ret < 0)
        fail("mpv API error while running command '%s' at %s:%d (%s)\n",
             cmd, file, line, mpv_error_string(ret));
}

static inline void get_property_impl(const char *file, int line, const char *property,
                                     int format, void *result)
{
    int ret = mpv_get_property(ctx, property, format, result);
    if (ret < 0)
        fail("mpv API error while getting property '%s' at %s:%d (%s)\n",
             property, file, line, mpv_error_string(ret));
}

static inline void set_option_or_property_impl(const char *file, int line, const char *property,
                                               int format, void *value, bool option)
{
    int ret = option ? mpv_set_option(ctx, property, format, value) :
                       mpv_set_property(ctx, property, format, value);
    if (ret < 0)
        fail("mpv API while setting %s '%s' at %s:%d (%s)\n", option ? "option" : "property",
             property, file, line, mpv_error_string(ret));

}

static inline void set_property_string_impl(const char *file, int line, const char *property,
                                            const char *value)
{
    int ret = mpv_set_property_string(ctx, property, value);
    if (ret < 0)
        fail("mpv API error while setting property '%s' to '%s' at %s:%d (%s)\n",
             property, value, file, line, mpv_error_string(ret));
}

MP_UNUSED static void check_double(const char *property, double expect)
{
    double result_double;
    get_property(property, MPV_FORMAT_DOUBLE, &result_double);
    if (expect != result_double)
        fail("Double: expected '%f' but got '%f'!\n", expect, result_double);
}

MP_UNUSED static void check_flag(const char *property, int expect)
{
    int result_flag;
    get_property(property, MPV_FORMAT_FLAG, &result_flag);
    if (expect != result_flag)
        fail("Flag: expected '%d' but got '%d'!\n", expect, result_flag);
}

MP_UNUSED static void check_int(const char *property, int64_t expect)
{
    int64_t result_int;
    get_property(property, MPV_FORMAT_INT64, &result_int);
    if (expect != result_int)
        fail("Int: expected '%" PRId64 "' but got '%" PRId64 "'!\n", expect, result_int);
}

MP_UNUSED static inline void check_string(const char *property, const char *expect)
{
    char *result_string;
    get_property(property, MPV_FORMAT_STRING, &result_string);
    if (strcmp(expect, result_string) != 0)
        fail("String: expected '%s' but got '%s'!\n", expect, result_string);
    mpv_free(result_string);
}

static inline void initialize(void)
{
    set_property_string("vo", "null");
    set_property_string("ao", "null");
    int ret = mpv_request_log_messages(ctx, "debug");
    if (ret < 0)
        fail("mpv API error while setting log level to debug: %s\n", mpv_error_string(ret));
    ret = mpv_initialize(ctx);
    if (ret < 0)
        fail("mpv API error while initializing mpv: %s\n", mpv_error_string(ret));
}

static inline void reload_file(const char *path)
{
    const char *cmd[] = {"loadfile", path, NULL};
    command(cmd);
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
