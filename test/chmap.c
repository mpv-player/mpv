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
#include "audio/chmap.h"

static void test_mp_chmap_diff(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap diff;

    mp_chmap_from_str(&a, bstr0("3.1"));
    mp_chmap_from_str(&b, bstr0("2.1"));

    mp_chmap_diff(&a, &b, &diff);
    assert_int_equal(diff.num, 1);
    assert_int_equal(diff.speaker[0], MP_SPEAKER_ID_FC);

    mp_chmap_from_str(&b, bstr0("6.1(back)"));
    mp_chmap_diff(&a, &b, &diff);
    assert_int_equal(diff.num, 0);

    mp_chmap_diff(&b, &a, &diff);
    assert_int_equal(diff.num, 3);
    assert_int_equal(diff.speaker[0], MP_SPEAKER_ID_BL);
    assert_int_equal(diff.speaker[1], MP_SPEAKER_ID_BR);
    assert_int_equal(diff.speaker[2], MP_SPEAKER_ID_BC);
}

static void test_mp_chmap_contains_with_related_chmaps(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;

    mp_chmap_from_str(&a, bstr0("3.1"));
    mp_chmap_from_str(&b, bstr0("2.1"));

    assert_true(mp_chmap_contains(&a, &b));
    assert_false(mp_chmap_contains(&b, &a));
}

static void test_mp_chmap_contains_with_unrelated_chmaps(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;

    mp_chmap_from_str(&a, bstr0("mono"));
    mp_chmap_from_str(&b, bstr0("stereo"));

    assert_false(mp_chmap_contains(&a, &b));
    assert_false(mp_chmap_contains(&b, &a));
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(test_mp_chmap_diff),
        unit_test(test_mp_chmap_contains_with_related_chmaps),
        unit_test(test_mp_chmap_contains_with_unrelated_chmaps),
    };
    return run_tests(tests);
}
