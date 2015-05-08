#include "test_helpers.h"
#include "audio/chmap_sel.h"

#define LAYOUTS(...) (char*[]){__VA_ARGS__, NULL}

static void test_sel(const char *input, const char *expected_selection,
                          char **layouts)
{
    struct mp_chmap_sel s = {0};
    struct mp_chmap input_map;
    struct mp_chmap expected_map;

    assert_true(mp_chmap_from_str(&input_map, bstr0(input)));
    assert_true(mp_chmap_from_str(&expected_map, bstr0(expected_selection)));

    for (int n = 0; layouts[n]; n++) {
        struct mp_chmap tmp;
        assert_true(mp_chmap_from_str(&tmp, bstr0(layouts[n])));
        int count = s.num_chmaps;
        mp_chmap_sel_add_map(&s, &tmp);
        assert_true(s.num_chmaps > count); // assure validity and max. count
    }

    assert_true(mp_chmap_sel_fallback(&s, &input_map));
    // We convert expected_map to a chmap and then back to a string to avoid
    // problems with ambiguous layouts.
    assert_string_equal(mp_chmap_to_str(&input_map),
                        mp_chmap_to_str(&expected_map));
}

static void test_mp_chmap_sel_fallback_upmix(void **state) {
    test_sel("5.1", "7.1", LAYOUTS("7.1"));
}

static void test_mp_chmap_sel_fallback_downmix(void **state) {
    test_sel("7.1", "5.1", LAYOUTS("5.1"));
}

static void test_mp_chmap_sel_fallback_incompatible(void **state) {
    test_sel("7.1(wide-side)", "7.1", LAYOUTS("7.1"));
}

static void test_mp_chmap_sel_fallback_prefer_compatible(void **state) {
    test_sel("7.1(wide-side)", "5.1(side)", LAYOUTS("7.1", "5.1(side)"));
}

static void test_mp_chmap_sel_fallback_prefer_closest_upmix(void **state) {
    test_sel("3.1", "5.1", LAYOUTS("7.1", "5.1", "2.1", "stereo", "mono"));
}

static void test_mp_chmap_sel_fallback_use_replacements(void **state) {
    test_sel("5.1", "7.1(rear)", LAYOUTS("7.1(rear)"));
}

static void test_mp_chmap_sel_fallback_works_on_alsa_chmaps(void **state) {
    test_sel("5.1", "7.1(alsa)", LAYOUTS("7.1(alsa)"));
}

static void test_mp_chmap_sel_fallback_mono_to_stereo(void **state) {
    test_sel("mono", "stereo", LAYOUTS("stereo", "5.1"));
}

static void test_mp_chmap_sel_fallback_stereo_to_stereo(void **state) {
    test_sel("stereo", "stereo", LAYOUTS("stereo", "5.1"));
}

static void test_mp_chmap_sel_fallback_no_downmix(void **state) {
    test_sel("5.1(side)", "7.1(rear)", LAYOUTS("stereo", "7.1(rear)"));
}

static void test_mp_chmap_sel_fallback_minimal_downmix(void **state) {
    test_sel("7.1", "fl-fr-lfe-fc-bl-br-flc-frc",
             LAYOUTS("fl-fr-lfe-fc-bl-br-flc-frc", "3.0(back)"));
}

static void test_mp_chmap_sel_fallback_reject_unknown(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    mp_chmap_set_unknown(&a, 2);

    mp_chmap_from_str(&b, bstr0("5.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_false(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "5.1");
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(test_mp_chmap_sel_fallback_upmix),
        unit_test(test_mp_chmap_sel_fallback_downmix),
        unit_test(test_mp_chmap_sel_fallback_incompatible),
        unit_test(test_mp_chmap_sel_fallback_prefer_compatible),
        unit_test(test_mp_chmap_sel_fallback_prefer_closest_upmix),
        unit_test(test_mp_chmap_sel_fallback_use_replacements),
        unit_test(test_mp_chmap_sel_fallback_works_on_alsa_chmaps),
        unit_test(test_mp_chmap_sel_fallback_mono_to_stereo),
        unit_test(test_mp_chmap_sel_fallback_stereo_to_stereo),
        unit_test(test_mp_chmap_sel_fallback_no_downmix),
        unit_test(test_mp_chmap_sel_fallback_minimal_downmix),
        unit_test(test_mp_chmap_sel_fallback_reject_unknown),
    };
    return run_tests(tests);
}
