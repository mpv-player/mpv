/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "test_helpers.h"
#include "video/out/gl_video.h"

static void test_scale_ambient_lux_limits(void **state) {
    float x;
    x = gl_video_scale_ambient_lux(16.0, 64.0, 2.40, 1.961, 16.0);
    assert_double_equal(x, 2.40f);

    x = gl_video_scale_ambient_lux(16.0, 64.0, 2.40, 1.961, 64.0);
    assert_double_equal(x, 1.961f);
}

static void test_scale_ambient_lux_sign(void **state) {
    float x;
    x = gl_video_scale_ambient_lux(16.0, 64.0, 1.961, 2.40, 64.0);
    assert_double_equal(x, 2.40f);
}

static void test_scale_ambient_lux_clamping(void **state) {
    float x;
    x = gl_video_scale_ambient_lux(16.0, 64.0, 2.40, 1.961, 0.0);
    assert_double_equal(x, 2.40f);
}

static void test_scale_ambient_lux_log10_midpoint(void **state) {
    float x;
    // 32 corresponds to the the midpoint after converting lux to the log10 scale
    x = gl_video_scale_ambient_lux(16.0, 64.0, 2.40, 1.961, 32.0);
    float mid_gamma = (2.40 - 1.961) / 2 + 1.961;
    assert_double_equal(x, mid_gamma);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(test_scale_ambient_lux_limits),
        unit_test(test_scale_ambient_lux_sign),
        unit_test(test_scale_ambient_lux_clamping),
        unit_test(test_scale_ambient_lux_log10_midpoint),
    };
    return run_tests(tests);
}

