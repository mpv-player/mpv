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
            set_option_or_property(options[i], formats[i], &str, true);
            set_option_or_property(properties[i], formats[i], &str, false);
            break;
        case MPV_FORMAT_FLAG:
            set_option_or_property(options[i], formats[i], &flag, true);
            set_option_or_property(properties[i], formats[i], &flag, false);
            break;
        case MPV_FORMAT_INT64:
            set_option_or_property(options[i], formats[i], &int_, true);
            set_option_or_property(properties[i], formats[i], &int_, false);
            break;
        case MPV_FORMAT_DOUBLE:
            set_option_or_property(options[i], formats[i], &double_, true);
            set_option_or_property(properties[i], formats[i], &double_, false);
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
    get_property("idle-active", MPV_FORMAT_NODE, &result_node);
    if (result_node.format != MPV_FORMAT_FLAG)
        fail("Node: expected mpv format '%d' but got '%d'!\n", MPV_FORMAT_FLAG, result_node.format);

    // Always should be true.
    if (result_node.u.flag != 1)
        fail("Node: expected 1 but got %d'!\n", result_node.u.flag);
}

static void test_secondary_sub_scale(void)
{
    check_string("secondary-sub-scale", "default");

    double sub_scale = 1.25;
    set_option_or_property("sub-scale", MPV_FORMAT_DOUBLE, &sub_scale, false);

    command_string("add secondary-sub-scale 0.25");
    check_double("secondary-sub-scale", 1.5);

    set_property_string("secondary-sub-scale", "default");
    check_string("secondary-sub-scale", "default");

    command_string("multiply secondary-sub-scale 2");
    check_double("secondary-sub-scale", 2.5);

    set_property_string("secondary-sub-scale", "default");
    check_string("secondary-sub-scale", "default");

    command_string("add options/secondary-sub-scale 0.25");
    check_double("secondary-sub-scale", 1.5);

    set_property_string("secondary-sub-scale", "default");
    check_string("secondary-sub-scale", "default");

    command_string("multiply options/secondary-sub-scale 2");
    check_double("secondary-sub-scale", 2.5);

    set_property_string("secondary-sub-scale", "default");
    check_string("secondary-sub-scale", "default");

    double secondary_sub_scale = 0.5;
    set_option_or_property("secondary-sub-scale", MPV_FORMAT_DOUBLE,
                           &secondary_sub_scale, false);
    check_double("secondary-sub-scale", secondary_sub_scale);

    set_property_string("secondary-sub-scale", "default");
    check_string("secondary-sub-scale", "default");
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
    printf(fmt, "test_secondary_sub_scale");
    test_secondary_sub_scale();
    printf("================ SHUTDOWN ================\n");

    command_string("quit");
    while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
