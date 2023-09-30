#pragma once

#include <float.h>
#include <inttypes.h>
#include <math.h>

#include "common/common.h"

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
#define assert_text_files_equal(refdir, outdir, name, err) \
    assert_text_files_equal_impl(__FILE__, __LINE__, (refdir), (outdir), (name), (name), (err))

void assert_int_equal_impl(const char *file, int line, int64_t a, int64_t b);
void assert_string_equal_impl(const char *file, int line,
                              const char *a, const char *b);
void assert_float_equal_impl(const char *file, int line,
                             double a, double b, double tolerance);
void assert_text_files_equal_impl(const char *file, int line,
                                  const char *refdir, const char *outdir,
                                  const char *ref, const char *new,
                                  const char *err);
void assert_memcmp_impl(const char *file, int line,
                        const void *a, const void *b, size_t size);

// Open a new file in the build dir path. Always succeeds.
FILE *test_open_out(const char *outdir, const char *name);

/* Stubs */

// Files commonly import common/msg.h which requires these to be
// defined. We don't actually need mpv's logging system here so
// just define these as stubs that do nothing.
struct mp_log;
void mp_msg(struct mp_log *log, int lev, const char *format, ...)
    PRINTF_ATTRIBUTE(3, 4);
int mp_msg_find_level(const char *s);
int mp_msg_level(struct mp_log *log);
void mp_write_console_ansi(void);
typedef struct AVDictionary AVDictionary;
void mp_set_avdict(AVDictionary **dict, char **kv);

// Windows additionally requires timer related code so it will actually
// import the real versions of these functions and use them. On other
// platforms, these can just be stubs for simplicity.
#ifndef WIN32_TESTS
void mp_time_us_add(void);
void mp_rel_time_to_timespec(void);
void mp_time_us(void);
void mp_time_us_to_realtime(void);
#endif
