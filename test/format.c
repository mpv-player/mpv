#include "test_utils.h"
#include "common/common.h"

int main(void)
{
    void *ta_ctx = talloc_new(NULL);

    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, false, false, false), "123");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, false, false,  true), "123");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, false,  true, false), "123%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, false,  true,  true), "123%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, true,  false, false), "+123");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, true,  false,  true), "+123");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, true,   true, false), "+123%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 0, true,   true,  true), "+123%");

    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, false, false, false), "-123");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, false, false,  true), "-123");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, false,  true, false), "-123%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, false,  true,  true), "-123%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, true,  false, false), "-123");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, true,  false,  true), "-123");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, true,   true, false), "-123%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 0, true,   true,  true), "-123%");

    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, false, false, false), "123.46");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, false, false,  true), "123.46");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, false,  true, false), "123.46%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, false,  true,  true), "123.46%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, true,  false, false), "+123.46");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, true,  false,  true), "+123.46");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, true,   true, false), "+123.46%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 2, true,   true,  true), "+123.46%");

    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, false, false, false), "-123.46");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, false, false,  true), "-123.46");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, false,  true, false), "-123.46%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, false,  true,  true), "-123.46%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, true,  false, false), "-123.46");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, true,  false,  true), "-123.46");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, true,   true, false), "-123.46%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 2, true,   true,  true), "-123.46%");

    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, false, false, false), "123.456000");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, false, false,  true), "123.456");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, false,  true, false), "123.456000%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, false,  true,  true), "123.456%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, true,  false, false), "+123.456000");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, true,  false,  true), "+123.456");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, true,   true, false), "+123.456000%");
    assert_string_equal(mp_format_double(ta_ctx,  123.456, 6, true,   true,  true), "+123.456%");

    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, false, false, false), "-123.456000");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, false, false,  true), "-123.456");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, false,  true, false), "-123.456000%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, false,  true,  true), "-123.456%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, true,  false, false), "-123.456000");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, true,  false,  true), "-123.456");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, true,   true, false), "-123.456000%");
    assert_string_equal(mp_format_double(ta_ctx, -123.456, 6, true,   true,  true), "-123.456%");

    assert_string_equal(mp_format_double(ta_ctx,  123, 6, false, false, false), "123.000000");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, false, false,  true), "123");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, false,  true, false), "123.000000%");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, false,  true,  true), "123%");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, true,  false, false), "+123.000000");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, true,  false,  true), "+123");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, true,   true, false), "+123.000000%");
    assert_string_equal(mp_format_double(ta_ctx,  123, 6, true,   true,  true), "+123%");

    assert_string_equal(mp_format_double(ta_ctx, -123, 6, false, false, false), "-123.000000");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, false, false,  true), "-123");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, false,  true, false), "-123.000000%");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, false,  true,  true), "-123%");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, true,  false, false), "-123.000000");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, true,  false,  true), "-123");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, true,   true, false), "-123.000000%");
    assert_string_equal(mp_format_double(ta_ctx, -123, 6, true,   true,  true), "-123%");

    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, false, false, false), "inf");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, false, false,  true), "inf");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, false,  true, false), "inf%");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, false,  true,  true), "inf%");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, true,  false, false), "+inf");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, true,  false,  true), "+inf");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, true,   true, false), "+inf%");
    assert_string_equal(mp_format_double(ta_ctx,  INFINITY, 6, true,   true,  true), "+inf%");

    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, false, false, false), "-inf");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, false, false,  true), "-inf");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, false,  true, false), "-inf%");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, false,  true,  true), "-inf%");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, true,  false, false), "-inf");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, true,  false,  true), "-inf");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, true,   true, false), "-inf%");
    assert_string_equal(mp_format_double(ta_ctx, -INFINITY, 6, true,   true,  true), "-inf%");

    talloc_free(ta_ctx);
}
