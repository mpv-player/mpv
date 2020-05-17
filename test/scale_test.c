#include <libavcodec/avcodec.h>

#include "scale_test.h"
#include "video/image_writer.h"
#include "video/sws_utils.h"

static struct mp_image *gen_repack_test_img(int w, int h, int bytes, bool rgb,
                                            bool alpha)
{
    struct mp_regular_imgfmt planar_desc = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        .component_size = bytes,
        .forced_csp = rgb ? MP_CSP_RGB : 0,
        .num_planes = alpha ? 4 : 3,
        .planes = {
            {1, {rgb ? 2 : 1}},
            {1, {rgb ? 3 : 2}},
            {1, {rgb ? 1 : 3}},
            {1, {4}},
        },
    };
    int mpfmt = mp_find_regular_imgfmt(&planar_desc);
    assert(mpfmt);
    struct mp_image *mpi = mp_image_alloc(mpfmt, w, h);
    assert(mpi);

    // Well, I have no idea what makes a good test image. So here's some crap.
    // This contains bars/tiles of solid colors. For each of R/G/B, it toggles
    // though 0/100% range, so 2*2*2 = 8 combinations (16 with alpha).
    int b_h = 16, b_w = 16;

    for (int y = 0; y < h; y++) {
        for (int p = 0; p < mpi->num_planes; p++) {
            void *line = mpi->planes[p] + mpi->stride[p] * (ptrdiff_t)y;

            for (int x = 0; x < w; x += b_w) {
                unsigned i = x / b_w + y / b_h * 2;
                int c = ((i >> p) & 1);
                if (bytes == 1) {
                    c *= (1 << 8) - 1;
                    for (int xs = x; xs < x + b_w; xs++)
                        ((uint8_t *)line)[xs] = c;
                } else if (bytes == 2) {
                    c *= (1 << 16) - 1;
                    for (int xs = x; xs < x + b_w; xs++)
                        ((uint16_t *)line)[xs] = c;
                }
            }
        }
    }

    return mpi;
}

static void dump_image(struct scale_test *stest, const char *name,
                       struct mp_image *img)
{
    char *path = mp_tprintf(4096, "%s/%s.png", stest->ctx->out_path, name);

    struct image_writer_opts opts = image_writer_opts_defaults;
    opts.format = AV_CODEC_ID_PNG;

    if (!write_image(img, &opts, path, stest->ctx->global, stest->ctx->log)) {
        MP_FATAL(stest->ctx, "Failed to write '%s'.\n", path);
        abort();
    }
}

// Compare 2 images (same format and size) for exact pixel data match.
// Does generally not work with formats that include undefined padding.
// Does not work with non-byte aligned formats.
static void assert_imgs_equal(struct scale_test *stest, FILE *f,
                              struct mp_image *ref, struct mp_image *new)
{
    assert(ref->imgfmt == new->imgfmt);
    assert(ref->w == new->w);
    assert(ref->h == new->h);

    assert(ref->fmt.flags & MP_IMGFLAG_BYTE_ALIGNED);
    assert(ref->fmt.bpp[0]);

    for (int p = 0; p < ref->num_planes; p++) {
        for (int y = 0; y < ref->h; y++) {
            void *line_r = ref->planes[p] + ref->stride[p] * (ptrdiff_t)y;
            void *line_o = new->planes[p] + new->stride[p] * (ptrdiff_t)y;
            size_t size = mp_image_plane_bytes(ref, p, 0, new->w);

            bool ok = memcmp(line_r, line_o, size) == 0;
            if (!ok) {
                stest->fail += 1;
                char *fn_a = mp_tprintf(80, "img%d_ref", stest->fail);
                char *fn_b = mp_tprintf(80, "img%d_new", stest->fail);
                fprintf(f, "Images mismatching, dumping to %s/%s\n", fn_a, fn_b);
                dump_image(stest, fn_a, ref);
                dump_image(stest, fn_b, new);
                return;
            }
        }
    }
}

void repack_test_run(struct scale_test *stest)
{
    char *logname = mp_tprintf(80, "%s.log", stest->test_name);
    FILE *f = test_open_out(stest->ctx, logname);

    if (!stest->sws) {
        init_imgfmts_list();

        stest->sws = mp_sws_alloc(stest);

        stest->img_repack_rgb8 = gen_repack_test_img(256, 128, 1, true, false);
        stest->img_repack_rgba8 = gen_repack_test_img(256, 128, 1, true, true);
        stest->img_repack_rgb16 = gen_repack_test_img(256, 128, 2, true, false);
        stest->img_repack_rgba16 = gen_repack_test_img(256, 128, 2, true, true);

        talloc_steal(stest, stest->img_repack_rgb8);
        talloc_steal(stest, stest->img_repack_rgba8);
        talloc_steal(stest, stest->img_repack_rgb16);
        talloc_steal(stest, stest->img_repack_rgba16);
    }

    for (int a = 0; a < num_imgfmts; a++) {
        int mpfmt = imgfmts[a];
        struct mp_imgfmt_desc fmtdesc = mp_imgfmt_get_desc(mpfmt);
        struct mp_regular_imgfmt rdesc;
        if (!mp_get_regular_imgfmt(&rdesc, mpfmt)) {
            int ofmt = mp_find_other_endian(mpfmt);
            if (!mp_get_regular_imgfmt(&rdesc, ofmt))
                continue;
        }
        if (rdesc.num_planes > 1 || rdesc.forced_csp != MP_CSP_RGB)
            continue;

        struct mp_image *test_img = NULL;
        bool alpha = fmtdesc.flags & MP_IMGFLAG_ALPHA;
        bool hidepth = rdesc.component_size > 1;
        if (alpha) {
            test_img = hidepth ? stest->img_repack_rgba16 : stest->img_repack_rgba8;
        } else {
            test_img = hidepth ? stest->img_repack_rgb16 : stest->img_repack_rgb8;
        }

        if (test_img->imgfmt == mpfmt)
            continue;

        if (!stest->fns->supports_fmts(stest->fns_priv, mpfmt, test_img->imgfmt))
            continue;

        if (!mp_sws_supports_formats(stest->sws, mpfmt, test_img->imgfmt))
            continue;

        fprintf(f, "%s using %s\n", mp_imgfmt_to_name(mpfmt),
                mp_imgfmt_to_name(test_img->imgfmt));

        struct mp_image *dst = mp_image_alloc(mpfmt, test_img->w, test_img->h);
        assert(dst);

        // This tests packing.
        bool ok = stest->fns->scale(stest->fns_priv, dst, test_img);
        assert(ok);

        // Cross-check with swscale in the other direction.
        // (Mostly so we don't have to worry about padding.)
        struct mp_image *src2 =
            mp_image_alloc(test_img->imgfmt, test_img->w, test_img->h);
        assert(src2);
        ok = mp_sws_scale(stest->sws, src2, dst) >= 0;
        assert_imgs_equal(stest, f, test_img, src2);

        // Assume the other conversion direction also works.
        assert(stest->fns->supports_fmts(stest->fns_priv, test_img->imgfmt, mpfmt));

        struct mp_image *back = mp_image_alloc(test_img->imgfmt, dst->w, dst->h);
        assert(back);

        // This tests unpacking.
        ok = stest->fns->scale(stest->fns_priv, back, dst);
        assert(ok);

        assert_imgs_equal(stest, f, test_img, back);

        talloc_free(back);
        talloc_free(src2);
        talloc_free(dst);
    }

    fclose(f);

    assert_text_files_equal(stest->ctx, logname, logname,
                            "This can fail if FFmpeg adds or removes pixfmts.");
}
