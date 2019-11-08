#pragma once

#include <float.h>
#include <inttypes.h>
#include <math.h>

#include "common/common.h"

struct MPContext;

bool run_tests(struct MPContext *mpctx);

struct test_ctx {
    struct mpv_global *global;
    struct mp_log *log;
};

struct unittest {
    // This is used to select the test on command line with --unittest=<name>.
    const char *name;

    // Cannot run without additional arguments supplied.
    bool is_complex;

    // Entrypoints. There are various for various purposes. Only 1 of them must
    // be set.

    // Entrypoint for tests which have a simple dependency on the mpv core. The
    // core is sufficiently initialized at this point.
    void (*run)(struct test_ctx *ctx);
};

extern const struct unittest test_chmap;
extern const struct unittest test_gl_video;
extern const struct unittest test_json;
extern const struct unittest test_linked_list;

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
