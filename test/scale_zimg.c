#include <libswscale/swscale.h>

#include "scale_test.h"
#include "video/fmt-conversion.h"
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
    zimg->opts.threads = 1;

    struct scale_test *stest = talloc_zero(NULL, struct scale_test);
    stest->fns = &fns;
    stest->fns_priv = zimg;
    stest->test_name = "repack_zimg";
    stest->ctx = ctx;

    repack_test_run(stest);

    FILE *f = test_open_out(ctx, "zimg_formats.txt");
    init_imgfmts_list();
    for (int n = 0; n < num_imgfmts; n++) {
        int imgfmt = imgfmts[n];
        fprintf(f, "%15s%7s%7s%7s%8s |\n", mp_imgfmt_to_name(imgfmt),
                mp_zimg_supports_in_format(imgfmt) ? " Zin" : "",
                mp_zimg_supports_out_format(imgfmt) ? " Zout" : "",
                sws_isSupportedInput(imgfmt2pixfmt(imgfmt)) ? " SWSin" : "",
                sws_isSupportedOutput(imgfmt2pixfmt(imgfmt)) ? "  SWSout" : "");

    }
    fclose(f);

    assert_text_files_equal(stest->ctx, "zimg_formats.txt", "zimg_formats.txt",
                "This can fail if FFmpeg/libswscale adds or removes pixfmts.");

    talloc_free(stest);
    talloc_free(zimg);
}

const struct unittest test_repack_zimg = {
    .name = "repack_zimg",
    .run = run,
};
