// Test scaling using libswscale.
// Note: libswscale is already tested in FFmpeg. This code serves mostly to test
//       the functionality scale_test.h using the already tested libswscale as
//       reference.

#include "scale_test.h"
#include "video/sws_utils.h"

static bool scale(void *pctx, struct mp_image *dst, struct mp_image *src)
{
    struct mp_sws_context *ctx = pctx;
    return mp_sws_scale(ctx, dst, src) >= 0;
}

static bool supports_fmts(void *pctx, int imgfmt_dst, int imgfmt_src)
{
    struct mp_sws_context *ctx = pctx;
    return mp_sws_supports_formats(ctx, imgfmt_dst, imgfmt_src);
}

static const struct scale_test_fns fns = {
    .scale = scale,
    .supports_fmts = supports_fmts,
};

static void run(struct test_ctx *ctx)
{
    struct mp_sws_context *sws = mp_sws_alloc(NULL);

    struct scale_test *stest = talloc_zero(NULL, struct scale_test);
    stest->fns = &fns;
    stest->fns_priv = sws;
    stest->test_name = "repack_sws";
    stest->ctx = ctx;

    repack_test_run(stest);

    talloc_free(stest);
    talloc_free(sws);
}

const struct unittest test_repack_sws = {
    .name = "repack_sws",
    .run = run,
};
