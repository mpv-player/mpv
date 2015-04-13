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
#include "audio/chmap_sel.h"

static void test_mp_chmap_sel_fallback_upmix(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("7.1"));
    mp_chmap_from_str(&b, bstr0("5.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_true(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "7.1");
}

static void test_mp_chmap_sel_fallback_downmix(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("5.1"));
    mp_chmap_from_str(&b, bstr0("7.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_true(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "5.1");
}

static void test_mp_chmap_sel_fallback_incompatible(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("7.1"));
    mp_chmap_from_str(&b, bstr0("7.1(wide-side)"));

    mp_chmap_sel_add_map(&s, &a);
    assert_true(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "7.1");
}

static void test_mp_chmap_sel_fallback_prefer_compatible(void **state) {
    struct mp_chmap a, b, c;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("7.1"));
    mp_chmap_from_str(&b, bstr0("5.1(side)"));
    mp_chmap_from_str(&c, bstr0("7.1(wide-side)"));

    mp_chmap_sel_add_map(&s, &a);
    mp_chmap_sel_add_map(&s, &b);

    assert_true(mp_chmap_sel_fallback(&s, &c));
    assert_string_equal(mp_chmap_to_str(&b), "5.1(side)");
}

static void test_mp_chmap_sel_fallback_prefer_closest_upmix(void **state) {
    struct mp_chmap_sel s = {0};

    char *maps[] = { "7.1", "5.1", "2.1", "stereo", "mono", NULL };
    for (int i = 0; maps[i]; i++) {
        struct mp_chmap m;
        mp_chmap_from_str(&m, bstr0(maps[i]));
        mp_chmap_sel_add_map(&s, &m);
    }

    struct mp_chmap c;
    mp_chmap_from_str(&c, bstr0("3.1"));
    assert_true(mp_chmap_sel_fallback(&s, &c));
    assert_string_equal(mp_chmap_to_str(&c), "5.1");
}

static void test_mp_chmap_sel_fallback_use_replacements(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("7.1(rear)"));
    mp_chmap_from_str(&b, bstr0("5.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_true(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "7.1(rear)");
}

static void test_mp_chmap_sel_fallback_reject_unknown(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    a.num = 2;
    a.speaker[0] = MP_SPEAKER_ID_UNKNOWN0;
    a.speaker[1] = MP_SPEAKER_ID_UNKNOWN0 + 1;

    mp_chmap_from_str(&b, bstr0("5.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_false(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "5.1");
}

static void test_mp_chmap_sel_fallback_works_on_alsa_chmaps(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("7.1(alsa)"));
    mp_chmap_from_str(&b, bstr0("5.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_true(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "7.1(alsa)");
}

static void test_mp_chmap_sel_fallback_mono_to_stereo(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap c;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("stereo"));
    mp_chmap_from_str(&b, bstr0("5.1"));
    mp_chmap_from_str(&c, bstr0("mono"));

    mp_chmap_sel_add_map(&s, &a);
    mp_chmap_sel_add_map(&s, &b);
    assert_true(mp_chmap_sel_fallback(&s, &c));
    assert_string_equal(mp_chmap_to_str(&c), "stereo");
}

static void test_mp_chmap_sel_fallback_stereo_to_stereo(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap c;
    struct mp_chmap_sel s = {0};

    mp_chmap_from_str(&a, bstr0("stereo"));
    mp_chmap_from_str(&b, bstr0("5.1"));
    mp_chmap_from_str(&c, bstr0("stereo"));

    mp_chmap_sel_add_map(&s, &a);
    mp_chmap_sel_add_map(&s, &b);
    assert_true(mp_chmap_sel_fallback(&s, &c));
    assert_string_equal(mp_chmap_to_str(&c), "stereo");
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(test_mp_chmap_sel_fallback_upmix),
        unit_test(test_mp_chmap_sel_fallback_downmix),
        unit_test(test_mp_chmap_sel_fallback_incompatible),
        unit_test(test_mp_chmap_sel_fallback_prefer_compatible),
        unit_test(test_mp_chmap_sel_fallback_prefer_closest_upmix),
        unit_test(test_mp_chmap_sel_fallback_use_replacements),
        unit_test(test_mp_chmap_sel_fallback_reject_unknown),
        unit_test(test_mp_chmap_sel_fallback_works_on_alsa_chmaps),
        unit_test(test_mp_chmap_sel_fallback_mono_to_stereo),
        unit_test(test_mp_chmap_sel_fallback_stereo_to_stereo),
    };
    return run_tests(tests);
}
