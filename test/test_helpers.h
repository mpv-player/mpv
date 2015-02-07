#ifndef MP_TESTS_H
#define MP_TESTS_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <math.h>
#include <float.h>

#define assert_double_equal(a, b) assert_true(fabs(a - b) <= DBL_EPSILON)

#endif
