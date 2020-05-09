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

    // Path for ref files, without trailing "/".
    const char *ref_path;

    // Path for result files, without trailing "/".
    const char *out_path;
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
extern const struct unittest test_img_format;
extern const struct unittest test_json;
extern const struct unittest test_linked_list;
extern const struct unittest test_repack_sws;
extern const struct unittest test_repack_zimg;
extern const struct unittest test_repack;
extern const struct unittest test_paths;

#define assert_true(x) assert(x)
#define assert_false(x) assert(!(x))
#define assert_int_equal(a, b) \
    assert_int_equal_impl(__FILE__, __LINE__, (a), (b))
#define assert_string_equal(a, b) \
    assert_string_equal_impl(__FILE__, __LINE__, (a), (b))
#define assert_float_equal(a, b, tolerance) \
    assert_float_equal_impl(__FILE__, __LINE__, (a), (b), (tolerance))

// Assert that memcmp(a,b,s)==0, or hexdump output on failure.
#define assert_memcmp(a, b, s) \
    assert_memcmp_impl(__FILE__, __LINE__, (a), (b), (s))

// Require that the files "ref" and "new" are the same. The paths can be
// relative to ref_path and out_path respectively. If they're not the same,
// the output of "diff" is shown, the err message (if not NULL), and the test
// fails.
#define assert_text_files_equal(ctx, ref, new, err) \
    assert_text_files_equal_impl(__FILE__, __LINE__, (ctx), (ref), (new), (err))

void assert_int_equal_impl(const char *file, int line, int64_t a, int64_t b);
void assert_string_equal_impl(const char *file, int line,
                              const char *a, const char *b);
void assert_float_equal_impl(const char *file, int line,
                             double a, double b, double tolerance);
void assert_text_files_equal_impl(const char *file, int line,
                                  struct test_ctx *ctx, const char *ref,
                                  const char *new, const char *err);
void assert_memcmp_impl(const char *file, int line,
                        const void *a, const void *b, size_t size);

// Open a new file in the out_path. Always succeeds.
FILE *test_open_out(struct test_ctx *ctx, const char *name);

// Sorted list of valid imgfmts. Call init_imgfmts_list() before use.
extern int imgfmts[];
extern int num_imgfmts;

void init_imgfmts_list(void);
