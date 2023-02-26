#pragma once

#include "img_utils.h"
#include "test_utils.h"
#include "video/mp_image.h"

struct scale_test_fns {
    bool (*scale)(void *ctx, struct mp_image *dst, struct mp_image *src);
    bool (*supports_fmts)(void *ctx, int imgfmt_dst, int imgfmt_src);
};

struct scale_test {
    // To be filled in by user.
    const struct scale_test_fns *fns;
    void *fns_priv;
    const char *test_name;
    const char *refdir;
    const char *outdir;

    // Private.
    struct mp_image *img_repack_rgb8;
    struct mp_image *img_repack_rgba8;
    struct mp_image *img_repack_rgb16;
    struct mp_image *img_repack_rgba16;
    struct mp_sws_context *sws;
    int fail;
};

// Test color repacking between packed formats (typically RGB).
void repack_test_run(struct scale_test *stest);
