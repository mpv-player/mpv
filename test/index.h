#pragma once

#include <stdbool.h>

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
