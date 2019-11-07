#ifndef MP_TESTS_H
#define MP_TESTS_H

#include <math.h>
#include <float.h>

#include "common/common.h"

#define assert_true(x) assert(x)
#define assert_false(x) assert(!(x))
#define assert_int_equal(a, b) \
    assert_int_equal_impl(__FILE__, __LINE__, (a), (b))
#define assert_string_equal(a, b) \
    assert_string_equal_impl(__FILE__, __LINE__, (a), (b))
#define assert_float_equal(a, b, tolerance) \
    assert_float_equal_impl(__FILE__, __LINE__, (a), (b), (tolerance))

void assert_int_equal_impl(const char *file, int line, int64_t a, int64_t b);
void assert_string_equal_impl(const char *file, int line,
                              const char *a, const char *b);
void assert_float_equal_impl(const char *file, int line,
                              double a, double b, double tolerance);

#endif
