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

#include "libmpv_common.h"

// Dummy values for test_options_and_properties
static const char *str = "string";
static int flag = 1;
static int64_t int_ = 20;
static double double_ = 1.5;

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
    if (argc != 1)
        return 1;

    ctx = mpv_create();
    if (!ctx)
        return 1;

    atexit(exit_cleanup);

    initialize();

    const char *fmt = "================ TEST: %s ================\n";
    printf(fmt, "test_options_and_properties");
    test_options_and_properties();
    printf("================ SHUTDOWN ================\n");

    mpv_command_string(ctx, "quit");
    while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
