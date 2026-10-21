#include "test_utils.h"
#include "video/mp_image.h"
#include "video/csputils.h"

int main(void)
{
    // Test standard primaries
    const struct pl_raw_primaries *bt709 = pl_raw_primaries_get(PL_COLOR_PRIM_BT_709);
    assert_true(mp_raw_primaries_valid(bt709));

    const struct pl_raw_primaries *bt2020 = pl_raw_primaries_get(PL_COLOR_PRIM_BT_2020);
    assert_true(mp_raw_primaries_valid(bt2020));

    // Test degenerate primaries (blue == white point, as in issue #18417)
    struct pl_raw_primaries bad = {
        .red   = { 0.708, 0.292 },
        .green = { 0.170, 0.797 },
        .blue  = { 0.313, 0.329 },
        .white = { 0.313, 0.329 },
    };
    assert_false(mp_raw_primaries_valid(&bad));

    // Test mp_image_params_guess_csp sanitizes bad hdr primaries
    struct mp_image_params params = {
        .imgfmt = IMGFMT_420P,
        .w = 64,
        .h = 64,
        .color = {
            .primaries = PL_COLOR_PRIM_BT_2020,
            .transfer = PL_COLOR_TRC_PQ,
            .hdr = {
                .prim = bad,
            },
        },
    };
    mp_image_params_guess_csp(&params);
    assert_false(mp_raw_primaries_valid(&params.color.hdr.prim));
    assert_float_equal(params.color.hdr.prim.blue.x, 0.0, 1e-6);

    return 0;
}
