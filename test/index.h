#pragma once

#include <stdbool.h>

struct mpv_global;
struct mp_log;
struct MPContext;

bool run_tests(struct MPContext *mpctx);

struct unittest {
    // This is used to select the test on command line with --unittest=<name>.
    const char *name;

    // Entrypoints. There are various for various purposes. Only 1 of them must
    // be set.

    // Entrypoint for tests which don't depend on the mpv core.
    void (*run_simple)(void);

    // Entrypoint for tests which have a simple dependency on the mpv core. The
    // core is sufficiently initialized at this point.
    void (*run)(struct mpv_global *global, struct mp_log *log);
};

extern const struct unittest test_chmap;
extern const struct unittest test_gl_video;
extern const struct unittest test_json;
extern const struct unittest test_linked_list;
