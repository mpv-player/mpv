#include "test_helpers.h"
#include "audio/chmap.h"

static void test_mp_chmap_diff(void **state) {
    struct mp_chmap a;
    struct mp_chmap b;

    mp_chmap_from_str(&a, bstr0("3.1"));
    mp_chmap_from_str(&b, bstr0("2.1"));

    assert_int_equal(mp_chmap_diffn(&a, &b), 1);

    mp_chmap_from_str(&b, bstr0("6.1(back)"));
    assert_int_equal(mp_chmap_diffn(&a, &b), 0);
    assert_int_equal(mp_chmap_diffn(&b, &a), 3);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(test_mp_chmap_diff),
    };
    return run_tests(tests);
}
