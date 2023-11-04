#include <limits.h>

#include <libavutil/pixfmt.h>

#include "common/common.h"
#include "common/global.h"
#include "img_utils.h"
#include "sub/draw_bmp.h"
#include "sub/osd.h"
#include "test_utils.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/img_format.h"
#include "video/repack.h"
#include "video/sws_utils.h"
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
#define SW16(v) ((((v) & 0xFF) << 8) | ((v) >> 8))
#define SW32(v) ((SW16((v) & 0xFFFFu) << 16) | (SW16(((v) | 0u) >> 16)))

#define ZIMG_IMAGE_DIMENSION_MAX ((size_t)(1) << (CHAR_BIT * sizeof(size_t) / 2 - 2))

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
    {1, 1, IMGFMT_BGR24,            {P8(1, 2, 3)},
           IMGFMT_GBRP,             {P8(2), P8(1), P8(3)}},
    {1, 1, IMGFMT_RGB24,            {P8(1, 2, 3)},
           IMGFMT_GBRP,             {P8(2), P8(3), P8(1)}},
    {1, 1, IMGFMT_RGBA64,           {P16(0x1a1b, 0x2a2b, 0x3a3b, 0x4a4b)},
           -AV_PIX_FMT_GBRAP16,     {P16(0x2a2b), P16(0x3a3b),
                                     P16(0x1a1b), P16(0x4a4b)}},
    {1, 1, -AV_PIX_FMT_BGRA64LE,    {P16(0x1a1b, 0x2a2b, 0x3a3b, 0x4a4b)},
           -AV_PIX_FMT_GBRAP16,     {P16(0x2a2b), P16(0x1a1b),
                                     P16(0x3a3b), P16(0x4a4b)}},
    {1, 1, -AV_PIX_FMT_RGBA64BE,    {P16(0x1b1a, 0x2b2a, 0x3b3a, 0x4b4a)},
           -AV_PIX_FMT_GBRAP16,     {P16(0x2a2b), P16(0x3a3b),
                                     P16(0x1a1b), P16(0x4a4b)}},
    {1, 1, -AV_PIX_FMT_BGRA64BE,    {P16(0x1b1a, 0x2b2a, 0x3b3a, 0x4b4a)},
           -AV_PIX_FMT_GBRAP16,     {P16(0x2a2b), P16(0x1a1b),
                                     P16(0x3a3b), P16(0x4a4b)}},
    {1, 1, -AV_PIX_FMT_RGB48BE,     {P16(0x1a1b, 0x2a2b, 0x3a3b)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2b2a), P16(0x3b3a), P16(0x1b1a)}},
    {1, 1, -AV_PIX_FMT_RGB48LE,     {P16(0x1a1b, 0x2a2b, 0x3a3b)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2a2b), P16(0x3a3b), P16(0x1a1b)}},
    {1, 1, -AV_PIX_FMT_BGR48BE,     {P16(0x1a1b, 0x2a2b, 0x3a3b)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2b2a), P16(0x1b1a), P16(0x3b3a)}},
    {1, 1, -AV_PIX_FMT_BGR48LE,     {P16(0x1a1b, 0x2a2b, 0x3a3b)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2a2b), P16(0x1a1b), P16(0x3a3b)}},
    {1, 1, -AV_PIX_FMT_XYZ12LE,     {P16(0x1a1b, 0x2a2b, 0x3a3b)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2a2b), P16(0x3a3b), P16(0x1a1b)}},
    {1, 1, -AV_PIX_FMT_XYZ12BE,     {P16(0x1b1a, 0x2b2a, 0x3b3a)},
           -AV_PIX_FMT_GBRP16,      {P16(0x2a2b), P16(0x3a3b), P16(0x1a1b)}},
    {3, 1, -AV_PIX_FMT_BGR8,        {P8(7, (7 << 3), (3 << 6))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB8,        {P8(3, (7 << 2), (7 << 5))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR4_BYTE,   {P8(1, (3 << 1), (1 << 3))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB4_BYTE,   {P8(1, (3 << 1), (1 << 3))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB565LE,    {P16((31), (63 << 5), (31 << 11))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB565BE,    {P16(SW16(31), SW16(63 << 5), SW16(31 << 11))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR565LE,    {P16((31), (63 << 5), (31 << 11))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR565BE,    {P16(SW16(31), SW16(63 << 5), SW16(31 << 11))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB555LE,    {P16((31), (31 << 5), (31 << 10))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB555BE,    {P16(SW16(31), SW16(31 << 5), SW16(31 << 10))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR555LE,    {P16((31), (31 << 5), (31 << 10))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR555BE,    {P16(SW16(31), SW16(31 << 5), SW16(31 << 10))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB444LE,    {P16((15), (15 << 4), (15 << 8))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_RGB444BE,    {P16(SW16(15), SW16(15 << 4), SW16(15 << 8))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0xFF,0,0), P8(0,0,0xFF)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR444LE,    {P16((15), (15 << 4), (15 << 8))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {3, 1, -AV_PIX_FMT_BGR444BE,    {P16(SW16(15), SW16(15 << 4), SW16(15 << 8))},
           IMGFMT_GBRP,             {P8(0,0xFF,0), P8(0,0,0xFF), P8(0xFF,0,0)},
        .flags = REPACK_CREATE_EXPAND_8BIT},
    {1, 1, IMGFMT_RGB30,            {P32((3 << 20) | (2 << 10) | 1)},
           -AV_PIX_FMT_GBRP10,      {P16(2), P16(1), P16(3)}},
    {1, 1, -AV_PIX_FMT_X2RGB10BE,   {P32(SW32((3 << 20) | (2 << 10) | 1))},
           -AV_PIX_FMT_GBRP10,      {P16(2), P16(1), P16(3)}},
    {8, 1, -AV_PIX_FMT_MONOWHITE,   {P8(0xAA)},
           IMGFMT_Y1,               {P8(0, 1, 0, 1, 0, 1, 0, 1)}},
    {8, 1, -AV_PIX_FMT_MONOBLACK,   {P8(0xAA)},
           IMGFMT_Y1,               {P8(1, 0, 1, 0, 1, 0, 1, 0)}},
    {2, 2, IMGFMT_NV12,             {P8(1, 2, 3, 4), P8(5, 6)},
           IMGFMT_420P,             {P8(1, 2, 3, 4), P8(5), P8(6)}},
    {2, 2, -AV_PIX_FMT_NV21,        {P8(1, 2, 3, 4), P8(5, 6)},
           IMGFMT_420P,             {P8(1, 2, 3, 4), P8(6), P8(5)}},
    {1, 1, -AV_PIX_FMT_AYUV64LE,    {P16(1, 2, 3, 4)},
           -AV_PIX_FMT_YUVA444P16,  {P16(2), P16(3), P16(4), P16(1)}},
    {1, 1, -AV_PIX_FMT_AYUV64BE,    {P16(0x0100, 0x0200, 0x0300, 0x0400)},
           -AV_PIX_FMT_YUVA444P16,  {P16(2), P16(3), P16(4), P16(1)}},
    {4, 1, -AV_PIX_FMT_YUYV422,     {P8(1, 2, 3, 4, 5, 6, 7, 8)},
           -AV_PIX_FMT_YUV422P,     {P8(1, 3, 5, 7), P8(2, 6), P8(4, 8)}},
    {2, 1, -AV_PIX_FMT_YVYU422,     {P8(1, 2, 3, 4)},
           -AV_PIX_FMT_YUV422P,     {P8(1, 3), P8(4), P8(2)}},
    {2, 1, -AV_PIX_FMT_UYVY422,     {P8(1, 2, 3, 4)},
           -AV_PIX_FMT_YUV422P,     {P8(2, 4), P8(1), P8(3)}},
    {2, 1, -AV_PIX_FMT_Y210LE,      {P16(0x1a1b, 0x2a2b, 0x3a3b, 0x4a4b)},
           -AV_PIX_FMT_YUV422P16,   {P16(0x1a1b, 0x3a3b), P16(0x2a2b), P16(0x4a4b)}},
    {2, 1, -AV_PIX_FMT_Y210BE,      {P16(0x1b1a, 0x2b2a, 0x3b3a, 0x4b4a)},
           -AV_PIX_FMT_YUV422P16,   {P16(0x1a1b, 0x3a3b), P16(0x2a2b), P16(0x4a4b)}},
    {1, 1, -AV_PIX_FMT_YA8,         {P8(1, 2)},
           IMGFMT_YAP8,             {P8(1), P8(2)}},
    {1, 1, -AV_PIX_FMT_YA16,        {P16(0x1a1b, 0x2a2b)},
           IMGFMT_YAP16,            {P16(0x1a1b), P16(0x2a2b)}},
    {2, 1, -AV_PIX_FMT_YUV422P16BE, {P16(0x1a1b, 0x2a2b), P16(0x3a3b),
                                     P16(0x4a4b)},
           -AV_PIX_FMT_YUV422P16,   {P16(0x1b1a, 0x2b2a), P16(0x3b3a),
                                     P16(0x4b4a)}},
    {8, 1, -AV_PIX_FMT_UYYVYY411,   {P8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)},
           -AV_PIX_FMT_YUV411P,     {P8(2, 3, 5, 6, 8, 9, 11, 12),
                                     P8(1, 7), P8(4, 10)}},
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

static int try_repack(FILE *f, int imgfmt, int flags, int not_if_fmt)
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
    if (a == imgfmt && b == imgfmt) {
        assert(is_true_planar(imgfmt));
        // (note that we require alpha-enabled zimg)
        assert(mp_zimg_supports_in_format(imgfmt));
        assert(un && pa);
        talloc_free(pa);
        talloc_free(un);
        return b;
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

    if (flags & REPACK_CREATE_PLANAR_F32)
        fprintf(f, " [planar-f32]");
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

        mp_image_params_guess_csp(&ia->params);
        mp_image_params_guess_csp(&ib->params);

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
                    int wb = mp_image_plane_bytes(srci, p, 0, e->w);
                    const void *cptr = (uint8_t *)srcd[p] + wb * y;
                    memcpy(ptr + srci->stride[p] * y, cptr, wb);
                }
            }

            repack_line(repacker, dx, dy, sx, sy, e->w);

            for (int p = 0; p < dsti->num_planes; p++) {
                uint8_t *ptr = mp_image_pixel_ptr(dsti, p, dx, dy);
                for (int y = 0; y < e->h >> dsti->fmt.ys[p]; y++) {
                    int wb = mp_image_plane_bytes(dsti, p, 0, e->w);
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

static void check_float_repack(int imgfmt, enum pl_color_system csp,
                               enum pl_color_levels levels)
{
    imgfmt = UNFUCK(imgfmt);

    struct mp_regular_imgfmt desc = {0};
    mp_get_regular_imgfmt(&desc, imgfmt);
    int bpp = desc.component_size;
    int comp_bits = desc.component_size * 8 + MPMIN(desc.component_pad, 0);

    assert(bpp == 1 || bpp == 2);

    int w = 1 << (bpp * 8);

    if (w > ZIMG_IMAGE_DIMENSION_MAX) {
        printf("Image dimension (%d) exceeded maximum allowed by zimg (%zu)."
               " Skipping test...\n", w, ZIMG_IMAGE_DIMENSION_MAX);
        return;
    }

    struct mp_image *src = mp_image_alloc(imgfmt, w, 1);
    assert(src);

    src->params.repr.sys = csp;
    src->params.repr.levels = levels;
    mp_image_params_guess_csp(&src->params);
    // mpv may not allow all combinations
    assert(src->params.repr.sys == csp);
    assert(src->params.repr.levels == levels);

    for (int p = 0; p < src->num_planes; p++) {
        int val = 0;
        for (int x = 0; x < w >> src->fmt.xs[p]; x++) {
            val = MPMIN(val, (1 << comp_bits) - 1);
            void *pixel = mp_image_pixel_ptr(src, p, x, 0);
            if (bpp == 1) {
                *(uint8_t *)pixel = val;
            } else {
                *(uint16_t *)pixel = val;
            }
            val++;
        }
    }

    struct mp_repack *to_f =
        mp_repack_create_planar(src->imgfmt, false, REPACK_CREATE_PLANAR_F32);
    struct mp_repack *from_f =
        mp_repack_create_planar(src->imgfmt, true, REPACK_CREATE_PLANAR_F32);
    assert(to_f && from_f);

    struct mp_image *z_f = mp_image_alloc(mp_repack_get_format_dst(to_f), w, 1);
    struct mp_image *r_f = mp_image_alloc(z_f->imgfmt, w, 1);
    struct mp_image *z_i = mp_image_alloc(src->imgfmt, w, 1);
    struct mp_image *r_i = mp_image_alloc(src->imgfmt, w, 1);
    assert(z_f && r_f && z_i && r_i);

    z_f->params.color = r_f->params.color = z_i->params.color =
        r_i->params.color = src->params.color;
    z_f->params.repr = r_f->params.repr = z_i->params.repr =
        r_i->params.repr = src->params.repr;

    // The idea is to use zimg to cross-check conversion.
    struct mp_sws_context *s = mp_sws_alloc(NULL);
    s->force_scaler = MP_SWS_ZIMG;
    struct zimg_opts opts = zimg_opts_defaults;
    opts.dither = ZIMG_DITHER_NONE;
    s->zimg_opts = &opts;
    int ret = mp_sws_scale(s, z_f, src);
    assert_true(ret >= 0);
    ret = mp_sws_scale(s, z_i, z_f);
    assert_true(ret >= 0);
    talloc_free(s);

    repack_config_buffers(to_f, 0, r_f, 0, src, NULL);
    repack_line(to_f, 0, 0, 0, 0, w);
    repack_config_buffers(from_f, 0, r_i, 0, r_f, NULL);
    repack_line(from_f, 0, 0, 0, 0, w);

    for (int p = 0; p < src->num_planes; p++) {
        for (int x = 0; x < w >> src->fmt.xs[p]; x++) {
            uint32_t src_val, z_i_val, r_i_val;
            if (bpp == 1) {
                src_val = *(uint8_t *)mp_image_pixel_ptr(src, p, x, 0);
                z_i_val = *(uint8_t *)mp_image_pixel_ptr(z_i, p, x, 0);
                r_i_val = *(uint8_t *)mp_image_pixel_ptr(r_i, p, x, 0);
            } else {
                src_val = *(uint16_t *)mp_image_pixel_ptr(src, p, x, 0);
                z_i_val = *(uint16_t *)mp_image_pixel_ptr(z_i, p, x, 0);
                r_i_val = *(uint16_t *)mp_image_pixel_ptr(r_i, p, x, 0);
            }
            float z_f_val = *(float *)mp_image_pixel_ptr(z_f, p, x, 0);
            float r_f_val = *(float *)mp_image_pixel_ptr(r_f, p, x, 0);

            assert_int_equal(src_val, z_i_val);
            assert_int_equal(src_val, r_i_val);
            double tolerance = 1.0 / (1 << (bpp * 8)) / 4;
            assert_float_equal(r_f_val, z_f_val, tolerance);
        }
    }

    talloc_free(src);
    talloc_free(z_i);
    talloc_free(z_f);
    talloc_free(r_i);
    talloc_free(r_f);
    talloc_free(to_f);
    talloc_free(from_f);
}

static bool try_draw_bmp(FILE *f, int imgfmt)
{
    bool ok = false;

    struct mp_image *dst = mp_image_alloc(imgfmt, 64, 64);
    if (!dst)
        goto done;

    struct sub_bitmap sb = {
        .bitmap = &(uint8_t[]){123},
        .stride = 1,
        .x = 1,
        .y = 1,
        .w = 1, .dw = 1,
        .h = 1, .dh = 1,

        .libass = { .color = 0xDEDEDEDE },
    };
    struct sub_bitmaps sbs = {
        .format = SUBBITMAP_LIBASS,
        .parts = &sb,
        .num_parts = 1,
        .change_id = 1,
    };
    struct sub_bitmap_list sbs_list = {
        .change_id = 1,
        .w = dst->w,
        .h = dst->h,
        .items = (struct sub_bitmaps *[]){&sbs},
        .num_items = 1,
    };

    struct mp_draw_sub_cache *c = mp_draw_sub_alloc_test(dst);
    if (mp_draw_sub_bitmaps(c, dst, &sbs_list)) {
        char *info = mp_draw_sub_get_dbg_info(c);
        fprintf(f, "%s\n", info);
        talloc_free(info);
        ok = true;
    }

    talloc_free(c);
    talloc_free(dst);

done:
    if (!ok)
        fprintf(f, "no\n");
    return ok;
}

int main(int argc, char *argv[])
{
    const char *refdir = argv[1];
    const char *outdir = argv[2];
    FILE *f = test_open_out(outdir, "repack.txt");

    init_imgfmts_list();
    for (int n = 0; n < num_imgfmts; n++) {
        int imgfmt = imgfmts[n];

        int other = try_repack(f, imgfmt, 0, 0);
        try_repack(f, imgfmt, REPACK_CREATE_ROUND_DOWN, other);
        try_repack(f, imgfmt, REPACK_CREATE_EXPAND_8BIT, other);
        try_repack(f, imgfmt, REPACK_CREATE_PLANAR_F32, other);
    }

    fclose(f);

    assert_text_files_equal(refdir, outdir, "repack.txt",
                            "This can fail if FFmpeg/libswscale adds or removes pixfmts.");

    check_float_repack(-AV_PIX_FMT_GBRAP, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL);
    check_float_repack(-AV_PIX_FMT_GBRAP10, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL);
    check_float_repack(-AV_PIX_FMT_GBRAP16, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL);
    check_float_repack(-AV_PIX_FMT_YUVA444P, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_FULL);
    check_float_repack(-AV_PIX_FMT_YUVA444P, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED);
    check_float_repack(-AV_PIX_FMT_YUVA444P10, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_FULL);
    check_float_repack(-AV_PIX_FMT_YUVA444P10, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED);
    check_float_repack(-AV_PIX_FMT_YUVA444P16, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_FULL);
    check_float_repack(-AV_PIX_FMT_YUVA444P16, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED);

    // Determine the list of possible draw_bmp input formats. Do this here
    // because it mostly depends on repack and imgformat stuff.
    f = test_open_out(outdir, "draw_bmp.txt");

    for (int n = 0; n < num_imgfmts; n++) {
        int imgfmt = imgfmts[n];

        fprintf(f, "%-12s= ", mp_imgfmt_to_name(imgfmt));
        try_draw_bmp(f, imgfmt);
    }

    fclose(f);

    assert_text_files_equal(refdir, outdir, "draw_bmp.txt",
                            "This can fail if FFmpeg/libswscale adds or removes pixfmts.");
    return 0;
}
