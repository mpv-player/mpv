#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>

#include "img_utils.h"
#include "options/path.h"
#include "test_utils.h"
#include "video/fmt-conversion.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"

static enum AVPixelFormat pixfmt_unsup[100];
static int num_pixfmt_unsup;

static const char *comp_type(enum mp_component_type type)
{
    switch (type) {
    case MP_COMPONENT_TYPE_UINT:  return "uint";
    case MP_COMPONENT_TYPE_FLOAT: return "float";
    default: return "unknown";
    }
}

int main(int argc, char *argv[])
{
    init_imgfmts_list();
    const char *refdir = argv[1];
    const char *outdir = argv[2];

    FILE *f = test_open_out(outdir, "img_formats.txt");

    for (int z = 0; z < num_imgfmts; z++) {
        int mpfmt = imgfmts[z];
        enum AVPixelFormat pixfmt = imgfmt2pixfmt(mpfmt);
        const AVPixFmtDescriptor *avd = av_pix_fmt_desc_get(pixfmt);

        fprintf(f, "%s: ", mp_imgfmt_to_name(mpfmt));
        if (mpfmt >= IMGFMT_AVPIXFMT_START && mpfmt < IMGFMT_AVPIXFMT_END)
            fprintf(f, "[GENERIC] ");

        int fcsp = mp_imgfmt_get_forced_csp(mpfmt);
        if (fcsp)
            fprintf(f, "fcsp=%s ", m_opt_choice_str(mp_csp_names, fcsp));
        fprintf(f, "ctype=%s\n", comp_type(mp_imgfmt_get_component_type(mpfmt)));

        struct mp_imgfmt_desc d = mp_imgfmt_get_desc(mpfmt);
        if (d.id) {
            fprintf(f, "  Basic desc: ");
            #define FLAG(t, c) if (d.flags & (t)) fprintf(f, "[%s]", c);
            FLAG(MP_IMGFLAG_BYTE_ALIGNED, "ba")
            FLAG(MP_IMGFLAG_BYTES, "bb")
            FLAG(MP_IMGFLAG_ALPHA, "a")
            FLAG(MP_IMGFLAG_YUV_P, "yuvp")
            FLAG(MP_IMGFLAG_YUV_NV, "nv")
            FLAG(MP_IMGFLAG_COLOR_YUV, "yuv")
            FLAG(MP_IMGFLAG_COLOR_RGB, "rgb")
            FLAG(MP_IMGFLAG_COLOR_XYZ, "xyz")
            FLAG(MP_IMGFLAG_GRAY, "gray")
            FLAG(MP_IMGFLAG_LE, "le")
            FLAG(MP_IMGFLAG_BE, "be")
            FLAG(MP_IMGFLAG_TYPE_PAL8, "pal")
            FLAG(MP_IMGFLAG_TYPE_HW, "hw")
            FLAG(MP_IMGFLAG_TYPE_FLOAT, "float")
            FLAG(MP_IMGFLAG_TYPE_UINT, "uint")
            fprintf(f, "\n");
            fprintf(f, "    planes=%d, chroma=%d:%d align=%d:%d\n",
                    d.num_planes, d.chroma_xs, d.chroma_ys, d.align_x, d.align_y);
            fprintf(f, "    {");
            for (int n = 0; n < MP_MAX_PLANES; n++) {
                if (n >= d.num_planes) {
                    assert(d.bpp[n] == 0 && d.xs[n] == 0 && d.ys[n] == 0);
                    continue;
                }
                fprintf(f, "%d/[%d:%d] ", d.bpp[n], d.xs[n], d.ys[n]);
            }
            fprintf(f, "}\n");
        } else {
            fprintf(f, "  [NODESC]\n");
        }

        for (int n = 0; n < d.num_planes; n++) {
            fprintf(f, "    %d: %dbits", n, d.bpp[n]);
            if (d.endian_shift)
                fprintf(f, " endian_bytes=%d", 1 << d.endian_shift);
            for (int x = 0; x < MP_NUM_COMPONENTS; x++) {
                struct mp_imgfmt_comp_desc cm = d.comps[x];
                fprintf(f, " {");
                if (cm.plane == n) {
                    if (cm.size) {
                        fprintf(f, "%d:%d", cm.offset, cm.size);
                        if (cm.pad)
                            fprintf(f, "/%d", cm.pad);
                    } else {
                        assert(cm.offset == 0);
                        assert(cm.pad == 0);
                    }
                }
                fprintf(f, "}");
                if (!(d.flags & (MP_IMGFLAG_PACKED_SS_YUV | MP_IMGFLAG_HAS_COMPS)))
                {
                    assert(cm.size == 0);
                    assert(cm.offset == 0);
                    assert(cm.pad == 0);
                }
            }
            fprintf(f, "\n");
            if (d.flags & MP_IMGFLAG_PACKED_SS_YUV) {
                assert(!(d.flags & MP_IMGFLAG_HAS_COMPS));
                uint8_t offsets[10];
                bool r = mp_imgfmt_get_packed_yuv_locations(mpfmt, offsets);
                assert(r);
                fprintf(f, "       luma_offsets=[");
                for (int x = 0; x < d.align_x; x++)
                    fprintf(f, " %d", offsets[x]);
                fprintf(f, "]\n");
            }
        }

        if (!(d.flags & MP_IMGFLAG_HWACCEL) && pixfmt != AV_PIX_FMT_NONE) {
            AVFrame *fr = av_frame_alloc();
            fr->format = pixfmt;
            fr->width = 128;
            fr->height = 128;
            int err = av_frame_get_buffer(fr, MP_IMAGE_BYTE_ALIGN);
            assert(err >= 0);
            struct mp_image *mpi = mp_image_alloc(mpfmt, fr->width, fr->height);
            if (mpi) {
                // A rather fuzzy test, which might fail even if there's no bug.
                for (int n = 0; n < 4; n++) {
                    if (!!mpi->planes[n] != !!fr->data[n]) {
                    #ifdef AV_PIX_FMT_FLAG_PSEUDOPAL
                        if (n == 1 && (avd->flags & AV_PIX_FMT_FLAG_PSEUDOPAL))
                            continue;
                    #endif
                        fprintf(f, "  Warning: p%d: %p %p\n", n,
                                mpi->planes[n], fr->data[n]);
                    }
                    if (mpi->stride[n] != fr->linesize[n]) {
                        fprintf(f, "  Warning: p%d: %d %d\n", n,
                                mpi->stride[n], fr->linesize[n]);
                    }
                }
            } else {
                fprintf(f, "  [NOALLOC]\n");
            }
            talloc_free(mpi);
            av_frame_free(&fr);
        }

        struct mp_regular_imgfmt reg;
        if (mp_get_regular_imgfmt(&reg, mpfmt)) {
            fprintf(f, "  Regular: planes=%d compbytes=%d bitpad=%d "
                    "chroma=%dx%d ctype=%s\n",
                    reg.num_planes, reg.component_size, reg.component_pad,
                    1 << reg.chroma_xs, 1 << reg.chroma_ys,
                    comp_type(reg.component_type));
            for (int n = 0; n < reg.num_planes; n++) {
                struct mp_regular_imgfmt_plane *plane = &reg.planes[n];
                fprintf(f, "    %d: {", n);
                for (int i = 0; i < plane->num_components; i++) {
                    if (i > 0)
                        fprintf(f, ", ");
                    fprintf(f, "%d", plane->components[i]);
                }
                fprintf(f, "}\n");
            }
        }

        // This isn't ours, but changes likely affect us.
        if (avd) {
            fprintf(f, "  AVD: name=%s chroma=%d:%d flags=0x%"PRIx64, avd->name,
                    avd->log2_chroma_w, avd->log2_chroma_h, avd->flags);
            #define FLAGAV(t, c) if (avd->flags & (t)) \
            {fprintf(f, "%s[%s]", pre, c); pre = ""; }
            char *pre = " ";
            FLAGAV(AV_PIX_FMT_FLAG_BE, "be")
            FLAGAV(AV_PIX_FMT_FLAG_PAL, "pal")
            FLAGAV(AV_PIX_FMT_FLAG_BITSTREAM, "bs")
            FLAGAV(AV_PIX_FMT_FLAG_HWACCEL, "hw")
            FLAGAV(AV_PIX_FMT_FLAG_PLANAR, "planar")
            FLAGAV(AV_PIX_FMT_FLAG_RGB, "rgb")
            FLAGAV(AV_PIX_FMT_FLAG_ALPHA, "alpha")
            FLAGAV(AV_PIX_FMT_FLAG_BAYER, "bayer")
            FLAGAV(AV_PIX_FMT_FLAG_FLOAT, "float")
            fprintf(f, "\n");
            for (int n = 0; n < avd->nb_components; n++) {
                const AVComponentDescriptor *cd = &avd->comp[n];
                fprintf(f, "    %d: p=%-2d st=%-2d o=%-2d sh=%-2d d=%d\n",
                        n, cd->plane, cd->step, cd->offset, cd->shift, cd->depth);
            }
            for (int n = avd->nb_components; n < 4; n++) {
                const AVComponentDescriptor *cd = &avd->comp[n];
                assert(!cd->plane && !cd->step && !cd->offset && !cd->shift &&
                       !cd->depth);
            }
        }

        const AVPixFmtDescriptor *avd2 = av_pix_fmt_desc_next(NULL);
        for (; avd2; avd2 = av_pix_fmt_desc_next(avd2)) {
            enum AVPixelFormat pixfmt2 = av_pix_fmt_desc_get_id(avd2);
            int mpfmt2 = pixfmt2imgfmt(pixfmt2);
            if (mpfmt2 == mpfmt && pixfmt2 != pixfmt)
                fprintf(f, "  Ambiguous alias: %s\n", avd2->name);
        }
    }

    for (int z = 0; z < num_pixfmt_unsup; z++) {
        const AVPixFmtDescriptor *avd = av_pix_fmt_desc_get(pixfmt_unsup[z]);
        fprintf(f, "Unsupported: %s\n", avd->name);
    }

    fclose(f);

    assert_text_files_equal(refdir, outdir, "img_formats.txt",
                            "This can fail if FFmpeg adds new formats or flags.");
    return 0;
}
