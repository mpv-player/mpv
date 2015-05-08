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

int main(void) {
    const UnitTest tests[] = {
        unit_test(test_mp_chmap_diff),
    };
    return run_tests(tests);
}
