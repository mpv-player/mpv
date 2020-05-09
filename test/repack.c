#include <libavutil/pixfmt.h>

#include "common/common.h"
#include "tests.h"
#include "video/fmt-conversion.h"
#include "video/img_format.h"
#include "video/repack.h"
#include "video/zimg.h"

// Excuse the utter stupidity.
#define UNFUCK(v) ((v) > 0 ? (v) : pixfmt2imgfmt(-(v)))
static_assert(IMGFMT_START > 0, "");
#define IMGFMT_GBRP (-AV_PIX_FMT_GBRP)
#define IMGFMT_GBRAP (-AV_PIX_FMT_GBRAP)

struct entry {
    int w, h;
    int fmt_a;
    const void *const a[4];
    int fmt_b;
    const void *const b[4];
    int flags;
};

#define P8(...) (const uint8_t[]){__VA_ARGS__}
#define P16(...) (const uint16_t[]){__VA_ARGS__}
#define P32(...) (const uint32_t[]){__VA_ARGS__}

// Warning: only entries that match existing conversions are tested.
static const struct entry repack_tests[] = {
    // Note: the '0' tests rely on 0 being written, although by definition the
    //       contents of this padding is undefined. The repacker always writes
    //       it this way, though.
    {1, 1, IMGFMT_RGB0,             {P8(1, 2, 3, 0)},
           IMGFMT_GBRP,             {P8(2), P8(3), P8(1)}},
    {1, 1, IMGFMT_BGR0,             {P8(1, 2, 3, 0)},
           IMGFMT_GBRP,             {P8(2), P8(1), P8(3)}},
    {1, 1, IMGFMT_0RGB,             {P8(0, 1, 2, 3)},
           IMGFMT_GBRP,             {P8(2), P8(3), P8(1)}},
    {1, 1, IMGFMT_0BGR,             {P8(0, 1, 2, 3)},
           IMGFMT_GBRP,             {P8(2), P8(1), P8(3)}},
    {1, 1, IMGFMT_RGBA,             {P8(1, 2, 3, 4)},
           IMGFMT_GBRAP,            {P8(2), P8(3), P8(1), P8(4)}},
    {1, 1, IMGFMT_BGRA,             {P8(1, 2, 3, 4)},
           IMGFMT_GBRAP,            {P8(2), P8(1), P8(3), P8(4)}},
    {1, 1, IMGFMT_ARGB,             {P8(4, 1, 2, 3)},
           IMGFMT_GBRAP,            {P8(2), P8(3), P8(1), P8(4)}},
    {1, 1, IMGFMT_ABGR,             {P8(4, 1, 2, 3)},
           IMGFMT_GBRAP,            {P8(2), P8(1), P8(3), P8(4)}},
    {1, 1, IMGFMT_RGBA64,           {P16(0x1a1b, 0x2a2b, 0x3a3b, 0x4a4b)},
           -AV_PIX_FMT_GBRAP16,     {P16(0x2a2b), P16(0x3a3b),
                                     P16(0x1a1b), P16(0x4a4b)}},
    {1, 1, -AV_PIX_FMT_RGB48BE,     {P16(0x1a1b, 0x2a2b, 0x3a3b)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2b2a), P16(0x3b3a),
                                     P16(0x1b1a)}},
    {1, 1, IMGFMT_RGB30,            {P32((3 << 20) | (2 << 10) | 1)},
           -AV_PIX_FMT_GBRP10,      {P16(2), P16(1), P16(3)}},
    {8, 1, -AV_PIX_FMT_MONOWHITE,   {P8(0xAA)},
           IMGFMT_Y1,               {P8(0, 1, 0, 1, 0, 1, 0, 1)}},
    {8, 1, -AV_PIX_FMT_MONOBLACK,   {P8(0xAA)},
           IMGFMT_Y1,               {P8(1, 0, 1, 0, 1, 0, 1, 0)}},
    {2, 2, IMGFMT_NV12,             {P8(1, 2, 3, 4), P8(5, 6)},
           IMGFMT_420P,             {P8(1, 2, 3, 4), P8(5), P8(6)}},
    {2, 2, -AV_PIX_FMT_NV21,        {P8(1, 2, 3, 4), P8(5, 6)},
           IMGFMT_420P,             {P8(1, 2, 3, 4), P8(6), P8(5)}},
    {1, 1, -AV_PIX_FMT_AYUV64,      {P16(1, 2, 3, 4)},
           -AV_PIX_FMT_YUVA444P16,  {P16(2), P16(3), P16(4), P16(1)}},
    {1, 1, -AV_PIX_FMT_AYUV64BE,    {P16(0x0100, 0x0200, 0x0300, 0x0400)},
           -AV_PIX_FMT_YUVA444P16,  {P16(2), P16(3), P16(4), P16(1)}},
    {2, 1, -AV_PIX_FMT_YVYU422,     {P8(1, 2, 3, 4)},
           -AV_PIX_FMT_YUV422P,     {P8(1, 3), P8(4), P8(2)}},
    {1, 1, -AV_PIX_FMT_YA16,        {P16(1, 2)},
           IMGFMT_YAP16,            {P16(1), P16(2)}},
    {2, 1, -AV_PIX_FMT_YUV422P16BE, {P16(0x1a1b, 0x2a2b), P16(0x3a3b),
                                     P16(0x4a4b)},
           -AV_PIX_FMT_YUV422P16,   {P16(0x1b1a, 0x2b2a), P16(0x3b3a),
                                     P16(0x4b4a)}},
};

static bool is_true_planar(int imgfmt)
{
    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, imgfmt))
        return false;

    for (int n = 0; n < desc.num_planes; n++) {
        if (desc.planes[n].num_components != 1)
            return false;
    }

    return true;
}

static int try_repack(struct test_ctx *ctx, FILE *f, int imgfmt, int flags,
                      int not_if_fmt)
{
    char *head = mp_tprintf(80, "%-15s =>", mp_imgfmt_to_name(imgfmt));
    struct mp_repack *un = mp_repack_create_planar(imgfmt, false, flags);
    struct mp_repack *pa = mp_repack_create_planar(imgfmt, true, flags);

    // If both exists, they must be always symmetric.
    if (un && pa) {
        assert(mp_repack_get_format_src(pa) == mp_repack_get_format_dst(un));
        assert(mp_repack_get_format_src(un) == mp_repack_get_format_dst(pa));
        assert(mp_repack_get_align_x(pa) == mp_repack_get_align_x(un));
        assert(mp_repack_get_align_y(pa) == mp_repack_get_align_y(un));
    }

    int a = 0;
    int b = 0;
    if (un) {
        a = mp_repack_get_format_src(un);
        b = mp_repack_get_format_dst(un);
    } else if (pa) {
        a = mp_repack_get_format_dst(pa);
        b = mp_repack_get_format_src(pa);
    }

    // Skip the identity ones because they're uninteresting, and add too much
    // noise. But still make sure they behave as expected.
    if (is_true_planar(imgfmt)) {
        // (note that we require alpha-enabled zimg)
        assert(mp_zimg_supports_in_format(imgfmt));
        assert(un && pa);
        assert(a == imgfmt && b == imgfmt);
        talloc_free(pa);
        talloc_free(un);
        return 0;
    }

    struct mp_repack *rp = pa ? pa : un;
    if (!rp) {
        if (!flags)
            fprintf(f, "%s no\n", head);
        return 0;
    }

    assert(a == imgfmt);
    if (b && b == not_if_fmt) {
        talloc_free(pa);
        talloc_free(un);
        return 0;
    }

    fprintf(f, "%s %4s %4s %-15s |", head, pa ? "[pa]" : "", un ? "[un]" : "",
            mp_imgfmt_to_name(b));

    fprintf(f, " a=%d:%d", mp_repack_get_align_x(rp), mp_repack_get_align_y(rp));

    if (flags & REPACK_CREATE_ROUND_DOWN)
        fprintf(f, " [round-down]");
    if (flags & REPACK_CREATE_EXPAND_8BIT)
        fprintf(f, " [expand-8bit]");

    // LCM of alignment of all packers.
    int ax = mp_repack_get_align_x(rp);
    int ay = mp_repack_get_align_y(rp);
    if (pa && un) {
        ax = MPMAX(mp_repack_get_align_x(pa), mp_repack_get_align_x(un));
        ay = MPMAX(mp_repack_get_align_y(pa), mp_repack_get_align_y(un));
    }

    for (int n = 0; n < MP_ARRAY_SIZE(repack_tests); n++) {
        const struct entry *e = &repack_tests[n];
        int fmt_a = UNFUCK(e->fmt_a);
        int fmt_b = UNFUCK(e->fmt_b);
        if (!(fmt_a == a && fmt_b == b && e->flags == flags))
            continue;

        // We convert a "random" macro pixel to catch potential addressing bugs
        // that might be ignored with (0, 0) origins.
        struct mp_image *ia = mp_image_alloc(fmt_a, e->w * 5 * ax, e->h * 5 * ay);
        struct mp_image *ib = mp_image_alloc(fmt_b, e->w * 7 * ax, e->h * 6 * ay);
        int sx = 4 * ax, sy = 3 * ay, dx = 3 * ax, dy = 2 * ay;

        assert(ia && ib);

        for (int pack = 0; pack < 2; pack++) {
            struct mp_repack *repacker = pack ? pa : un;
            if (!repacker)
                continue;

            mp_image_clear(ia, 0, 0, ia->w, ia->h);
            mp_image_clear(ib, 0, 0, ib->w, ib->h);

            const void *const *dstd = pack ? e->a : e->b;
            const void *const *srcd = pack ? e->b : e->a;
            struct mp_image *dsti = pack ? ia : ib;
            struct mp_image *srci = pack ? ib : ia;

            bool r = repack_config_buffers(repacker, 0, dsti, 0, srci, NULL);
            assert(r);

            for (int p = 0; p < srci->num_planes; p++) {
                uint8_t *ptr = mp_image_pixel_ptr(srci, p, sx, sy);
                for (int y = 0; y < e->h >> srci->fmt.ys[p]; y++) {
                    int w = e->w >> srci->fmt.xs[p];
                    int wb = (w * srci->fmt.bpp[p] + 7) / 8;
                    const void *cptr = (uint8_t *)srcd[p] + wb * y;
                    memcpy(ptr + srci->stride[p] * y, cptr, wb);
                }
            }

            repack_line(repacker, dx, dy, sx, sy, e->w);

            for (int p = 0; p < dsti->num_planes; p++) {
                uint8_t *ptr = mp_image_pixel_ptr(dsti, p, dx, dy);
                for (int y = 0; y < e->h >> dsti->fmt.ys[p]; y++) {
                    int w = e->w >> dsti->fmt.xs[p];
                    int wb = (w * dsti->fmt.bpp[p] + 7) / 8;
                    const void *cptr = (uint8_t *)dstd[p] + wb * y;
                    assert_memcmp(ptr + dsti->stride[p] * y, cptr, wb);
                }
            }

            fprintf(f, " [t%s]", pack ? "p" : "u");
        }

        talloc_free(ia);
        talloc_free(ib);
    }

    fprintf(f, "\n");

    talloc_free(pa);
    talloc_free(un);
    return b;
}

static void run(struct test_ctx *ctx)
{
    FILE *f = test_open_out(ctx, "repack.txt");

    init_imgfmts_list();
    for (int n = 0; n < num_imgfmts; n++) {
        int imgfmt = imgfmts[n];

        int other = try_repack(ctx, f, imgfmt, 0, 0);
        try_repack(ctx, f, imgfmt, REPACK_CREATE_ROUND_DOWN, other);
        try_repack(ctx, f, imgfmt, REPACK_CREATE_EXPAND_8BIT, other);
    }

    fclose(f);

    assert_text_files_equal(ctx, "repack.txt", "repack.txt",
                "This can fail if FFmpeg/libswscale adds or removes pixfmts.");
}

const struct unittest test_repack = {
    .name = "repack",
    .run = run,
};
