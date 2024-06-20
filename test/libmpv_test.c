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

// Dummy values for test_options_and_properties
static const char *str = "string";
static int flag = 1;
static int64_t int_ = 20;
static double double_ = 1.5;

// Global handle.
static mpv_handle *ctx;

MP_NORETURN PRINTF_ATTRIBUTE(1, 2)
static void fail(const char *fmt, ...)
{
    if (fmt) {
        va_list va;
        va_start(va, fmt);
        vfprintf(stderr, fmt, va);
        va_end(va);
    }
    exit(1);
}


static void exit_cleanup(void)
{
    if (ctx)
        mpv_destroy(ctx);
}

static mpv_event *wrap_wait_event(void)
{
    while (1) {
        mpv_event *ev = mpv_wait_event(ctx, 1);

        if (ev->event_id == MPV_EVENT_NONE) {
            continue;
        } else if (ev->event_id == MPV_EVENT_LOG_MESSAGE) {
            mpv_event_log_message *msg = (mpv_event_log_message*)ev->data;
            printf("[%s:%s] %s", msg->prefix, msg->level, msg->text);
            if (msg->log_level <= MPV_LOG_LEVEL_ERROR)
                fail("error was logged");
        } else {
            return ev;
        }
    }
}

static void check_api_error(int status)
{
    if (status < 0)
        fail("mpv API error: %s\n", mpv_error_string(status));
}

/****/

static void check_double(const char *property, double expect)
{
    double result_double;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_DOUBLE, &result_double));
    if (expect != result_double)
        fail("Double: expected '%f' but got '%f'!\n", expect, result_double);
}

static void check_flag(const char *property, int expect)
{
    int result_flag;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_FLAG, &result_flag));
    if (expect != result_flag)
        fail("Flag: expected '%d' but got '%d'!\n", expect, result_flag);
}

static void check_int(const char *property, int64_t expect)
{
    int64_t result_int;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_INT64, &result_int));
    if (expect != result_int)
        fail("Int: expected '%" PRId64 "' but got '%" PRId64 "'!\n", expect, result_int);
}

static void check_string(const char *property, const char *expect)
{
    char *result_string;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_STRING, &result_string));
    if (strcmp(expect, result_string) != 0)
        fail("String: expected '%s' but got '%s'!\n", expect, result_string);
    mpv_free(result_string);
}

static void check_results(const char *properties[], enum mpv_format formats[])
{
    for (int i = 0; properties[i]; i++) {
        switch (formats[i]) {
        case MPV_FORMAT_STRING:
            check_string(properties[i], str);
            break;
        case MPV_FORMAT_FLAG:
            check_flag(properties[i], flag);
            break;
        case MPV_FORMAT_INT64:
            check_int(properties[i], int_);
            break;
        case MPV_FORMAT_DOUBLE:
            check_double(properties[i], double_);
            break;
        }
    }
}

static void set_options_and_properties(const char *options[], const char *properties[],
                                              enum mpv_format formats[])
{
    for (int i = 0; options[i]; i++) {
        switch (formats[i]) {
        case MPV_FORMAT_STRING:
            check_api_error(mpv_set_option(ctx, options[i], formats[i], &str));
            check_api_error(mpv_set_property(ctx, properties[i], formats[i], &str));
            break;
        case MPV_FORMAT_FLAG:
            check_api_error(mpv_set_option(ctx, options[i], formats[i], &flag));
            check_api_error(mpv_set_property(ctx, properties[i], formats[i], &flag));
            break;
        case MPV_FORMAT_INT64:
            check_api_error(mpv_set_option(ctx, options[i], formats[i], &int_));
            check_api_error(mpv_set_property(ctx, properties[i], formats[i], &int_));
            break;
        case MPV_FORMAT_DOUBLE:
            check_api_error(mpv_set_option(ctx, options[i], formats[i], &double_));
            check_api_error(mpv_set_property(ctx, properties[i], formats[i], &double_));
            break;
        }
    }
}

/****/

static void test_file_loading(char *file)
{
    const char *cmd[] = {"loadfile", file, NULL};
    check_api_error(mpv_command(ctx, cmd));
    int loaded = 0;
    int finished = 0;
    while (!finished) {
        mpv_event *event = wrap_wait_event();
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            // make sure it loads before exiting
            loaded = 1;
            break;
        case MPV_EVENT_END_FILE:
            if (loaded)
                finished = 1;
            break;
        }
    }
    if (!finished)
        fail("Unable to load test file!\n");
}

static void test_lavfi_complex(char *file)
{
    const char *cmd[] = {"loadfile", file, NULL};
    check_api_error(mpv_command(ctx, cmd));
    int finished = 0;
    int loaded = 0;
    while (!finished) {
        mpv_event *event = wrap_wait_event();
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            // Add file as external and toggle lavfi-complex on.
            if (!loaded) {
                check_api_error(mpv_set_property_string(ctx, "external-files", file));
                const char *add_cmd[] = {"video-add", file, "auto", NULL};
                check_api_error(mpv_command(ctx, add_cmd));
                check_api_error(mpv_set_property_string(ctx, "lavfi-complex", "[vid1] [vid2] vstack [vo]"));
            }
            loaded = 1;
            break;
        case MPV_EVENT_END_FILE:
            if (loaded)
                finished = 1;
            break;
        }
    }
    if (!finished)
        fail("Lavfi complex failed!\n");
}

// Ensure that setting options/properties work correctly and
// have the expected values.
static void test_options_and_properties(void)
{
    // Order matters. string -> flag -> int -> double (repeat)
    // One for set_option the other for set_property
    const char *options[] = {
        "screen-name",
        "save-position-on-quit",
        "cursor-autohide",
        "speed",
        NULL
    };

    const char *properties[] = {
        "fs-screen-name",
        "shuffle",
        "sub-pos",
        "window-scale",
        NULL
    };

    // Must match above ordering.
    enum mpv_format formats[] = {
        MPV_FORMAT_STRING,
        MPV_FORMAT_FLAG,
        MPV_FORMAT_INT64,
        MPV_FORMAT_DOUBLE,
    };

    set_options_and_properties(options, properties, formats);

    check_api_error(mpv_initialize(ctx));

    check_results(options, formats);
    check_results(properties, formats);

    // Ensure the format is still MPV_FORMAT_FLAG for these property types.
    mpv_node result_node;
    check_api_error(mpv_get_property(ctx, "idle-active", MPV_FORMAT_NODE, &result_node));
    if (result_node.format != MPV_FORMAT_FLAG)
        fail("Node: expected mpv format '%d' but got '%d'!\n", MPV_FORMAT_FLAG, result_node.format);

    // Always should be true.
    if (result_node.u.flag != 1)
        fail("Node: expected 1 but got %d'!\n", result_node.u.flag);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 1;
    atexit(exit_cleanup);

    ctx = mpv_create();
    if (!ctx)
        return 1;

    check_api_error(mpv_set_option_string(ctx, "vo", "null"));
    // load osc too to see if it works
    check_api_error(mpv_set_option_string(ctx, "osc", "yes"));
    check_api_error(mpv_request_log_messages(ctx, "debug"));

    const char *fmt = "================ TEST: %s ================\n";

    printf(fmt, "test_options_and_properties");
    test_options_and_properties();
    printf(fmt, "test_file_loading");
    test_file_loading(argv[1]);
    printf(fmt, "test_lavfi_complex");
    test_lavfi_complex(argv[1]);

    printf("================ SHUTDOWN ================\n");
    mpv_command_string(ctx, "quit");
    while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
