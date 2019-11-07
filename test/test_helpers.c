#include <inttypes.h>

#include "test_helpers.h"

void assert_int_equal_impl(const char *file, int line, int64_t a, int64_t b)
{
    if (a != b) {
        printf("%s:%d: %"PRId64" != %"PRId64"\n", file, line, a, b);
        abort();
    }
}

void assert_string_equal_impl(const char *file, int line,
                              const char *a, const char *b)
{
    if (strcmp(a, b) != 0) {
        printf("%s:%d: '%s' != '%s'\n", file, line, a, b);
        abort();
    }
}

void assert_float_equal_impl(const char *file, int line,
                              double a, double b, double tolerance)
{
    if (fabs(a - b) > tolerance) {
        printf("%s:%d: %f != %f\n", file, line, a, b);
        abort();
    }
}
