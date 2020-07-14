#pragma once

#include <stdbool.h>

#include <zimg.h>

#include "mp_image.h"

#define ZIMG_ALIGN 64

struct mpv_global;

bool mp_zimg_supports_in_format(int imgfmt);
bool mp_zimg_supports_out_format(int imgfmt);

struct zimg_opts {
    int scaler;
    double scaler_params[2];
    int scaler_chroma;
    double scaler_chroma_params[2];
    int dither;
    int fast;
    int threads;
};

extern const struct zimg_opts zimg_opts_defaults;

struct mp_zimg_context {
    // Can be set for verbose error printing.
    struct mp_log *log;

    // User configuration. Note: changing these requires calling mp_zimg_config()
    // to update the filter graph. The first mp_zimg_convert() call (or if the
    // image format changes) will do this automatically.
    struct zimg_opts opts;

    // Input/output parameters. Note: if these mismatch with the
    // mp_zimg_convert() parameters, mp_zimg_config() will be called
    // automatically.
    struct mp_image_params src, dst;

    // Cached zimg state (if any). Private, do not touch.
    struct m_config_cache *opts_cache;
    struct mp_zimg_state **states;
    int num_states;
    struct mp_thread_pool *tp;
    int current_thread_count;
};

// Allocate a zimg context. Always succeeds. Returns a talloc pointer (use
// talloc_free() to release it).
struct mp_zimg_context *mp_zimg_alloc(void);

// Enable auto-update of parameters from command line. Don't try to set custom
// options (other than possibly .src/.dst), because they might be overwritten
// if the user changes any options.
void mp_zimg_enable_cmdline_opts(struct mp_zimg_context *ctx,
                                 struct mpv_global *g);

// Try to build the conversion chain using the parameters currently set in ctx.
// If this succeeds, mp_zimg_convert() will always succeed (probably), as long
// as the input has the same parameters.
// Returns false on error.
bool mp_zimg_config(struct mp_zimg_context *ctx);

// Similar to mp_zimg_config(), but assume none of the user parameters changed,
// except possibly .src and .dst. This essentially checks whether src/dst
// changed, and if so, calls mp_zimg_config().
bool mp_zimg_config_image_params(struct mp_zimg_context *ctx);

// Convert/scale src to dst. On failure, the data in dst is not touched.
bool mp_zimg_convert(struct mp_zimg_context *ctx, struct mp_image *dst,
                     struct mp_image *src);
