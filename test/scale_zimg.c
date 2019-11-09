#include "scale_test.h"
#include "video/zimg.h"

static bool scale(void *pctx, struct mp_image *dst, struct mp_image *src)
{
    struct mp_zimg_context *ctx = pctx;
    return mp_zimg_convert(ctx, dst, src);
}

static bool supports_fmts(void *pctx, int imgfmt_dst, int imgfmt_src)
{
    return mp_zimg_supports_in_format(imgfmt_src) &&
           mp_zimg_supports_out_format(imgfmt_dst);
}

static const struct scale_test_fns fns = {
    .scale = scale,
    .supports_fmts = supports_fmts,
};

static void run(struct test_ctx *ctx)
{
    struct mp_zimg_context *zimg = mp_zimg_alloc();

    struct scale_test *stest = talloc_zero(NULL, struct scale_test);
    stest->fns = &fns;
    stest->fns_priv = zimg;
    stest->test_name = "repack_zimg";
    stest->ctx = ctx;

    repack_test_run(stest);

    talloc_free(stest);
    talloc_free(zimg);
}

const struct unittest test_repack_zimg = {
    .name = "repack_zimg",
    .run = run,
};
