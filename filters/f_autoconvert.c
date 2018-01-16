#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "video/hwdec.h"
#include "video/mp_image.h"

#include "f_autoconvert.h"
#include "f_hwtransfer.h"
#include "f_swscale.h"
#include "f_utils.h"
#include "filter.h"
#include "filter_internal.h"

struct priv {
    struct mp_log *log;

    struct mp_subfilter sub;

    bool force_update;

    int *imgfmts;
    int *subfmts;
    int num_imgfmts;

    // Enable special conversion for the final stage before the VO.
    bool vo_convert;

    // sws state
    int in_imgfmt, in_subfmt;

    struct mp_autoconvert public;
};

// Dummy filter for bundling sub-conversion filters.
static const struct mp_filter_info convert_filter = {
    .name = "convert",
};

// For hw decoding: thing which can convert between underlying surface formats.
// The filter detects the needed target format from struct mp_hwdec_ctx.
struct subfmt_conv {
    int hw_imgfmt;
    struct mp_filter *(*create)(struct mp_filter *parent);
};

static const struct subfmt_conv subfmt_converters[] = {
#if HAVE_D3D_HWACCEL
    {IMGFMT_D3D11, vf_d3d11_create_outconv},
#endif
    {0}
};

void mp_autoconvert_clear(struct mp_autoconvert *c)
{
    struct priv *p = c->f->priv;

    p->num_imgfmts = 0;
}

void mp_autoconvert_add_imgfmt(struct mp_autoconvert *c, int imgfmt, int subfmt)
{
    struct priv *p = c->f->priv;

    MP_TARRAY_GROW(p, p->imgfmts, p->num_imgfmts);
    MP_TARRAY_GROW(p, p->subfmts, p->num_imgfmts);

    p->imgfmts[p->num_imgfmts] = imgfmt;
    p->subfmts[p->num_imgfmts] = subfmt;

    p->num_imgfmts += 1;
    p->force_update = true;
}

void mp_autoconvert_add_vo_hwdec_subfmts(struct mp_autoconvert *c,
                                         struct mp_hwdec_devices *devs)
{
    struct priv *p = c->f->priv;
    assert(devs);

    int prev_format = 0;

    for (int n = 0; ; n++) {
        struct mp_hwdec_ctx *ctx = hwdec_devices_get_n(devs, n);
        if (!ctx)
            break;
        if (!ctx->hw_imgfmt || !ctx->supported_formats)
            continue;
        // Very hacky: don't let d3d11-egl-rgb overwrite d3d11-egl
        if (ctx->hw_imgfmt == prev_format)
            continue;
        prev_format = ctx->hw_imgfmt;
        // Stupidity: VOs export imgfmt only, so subfmt is always 0. Remove it
        // to fix it up.
        for (int i = 0; i < p->num_imgfmts; i++) {
            if (p->imgfmts[i] != ctx->hw_imgfmt)
                continue;

            int count = p->num_imgfmts;
            MP_TARRAY_REMOVE_AT(p->imgfmts, count, i);
            count = p->num_imgfmts;
            MP_TARRAY_REMOVE_AT(p->subfmts, count, i);
            p->num_imgfmts -= 1;
            break;
        }
        for (int i = 0; ctx->supported_formats[i]; i++)
            mp_autoconvert_add_imgfmt(c, ctx->hw_imgfmt, ctx->supported_formats[i]);
    }

    p->vo_convert = true;
}

static void handle_video_frame(struct mp_filter *f)
{
    struct priv *p = f->priv;

    struct mp_frame frame = p->sub.frame;
    if (frame.type != MP_FRAME_VIDEO) {
        MP_ERR(p, "video input required!\n");
        mp_filter_internal_mark_failed(f);
        return;
    }

    struct mp_image *img = frame.data;

    if (p->force_update)
        p->in_imgfmt = p->in_subfmt = 0;

    if (img->imgfmt == p->in_imgfmt && img->params.hw_subfmt == p->in_subfmt) {
        mp_subfilter_continue(&p->sub);
        return;
    }

    if (!mp_subfilter_drain_destroy(&p->sub)) {
        p->in_imgfmt = p->in_subfmt = 0;
        return;
    }

    p->in_imgfmt = img->params.imgfmt;
    p->in_subfmt = img->params.hw_subfmt;
    p->force_update = false;

    bool different_subfmt = false;

    for (int n = 0; n < p->num_imgfmts; n++) {
        bool samefmt = img->params.imgfmt == p->imgfmts[n];
        bool samesubffmt = img->params.hw_subfmt == p->subfmts[n];
        if (samefmt && !samesubffmt)
            different_subfmt = true;
        if (samefmt && (samesubffmt || !p->subfmts[n])) {
            mp_subfilter_continue(&p->sub);
            return;
        }
    }

    struct mp_stream_info *info = mp_filter_find_stream_info(f);

    struct mp_filter *conv = mp_filter_create(f, &convert_filter);
    mp_filter_add_pin(conv, MP_PIN_IN, "in");
    mp_filter_add_pin(conv, MP_PIN_OUT, "out");

    struct mp_filter *filters[2] = {0};
    bool need_sws = true;

    int *fmts = p->imgfmts;
    int num_fmts = p->num_imgfmts;

    // Source is sw, all targets are hw -> try to upload.
    bool sw_to_hw = !IMGFMT_IS_HWACCEL(img->imgfmt);
    for (int n = 0; n < num_fmts; n++)
        sw_to_hw &= IMGFMT_IS_HWACCEL(fmts[n]);

    if (sw_to_hw && num_fmts > 0) {
        // We can probably use this! Very lazy and very approximate.
        struct mp_hwupload *upload = mp_hwupload_create(conv, fmts[0]);
        if (upload) {
            MP_INFO(p, "HW-uploading to %s\n", mp_imgfmt_to_name(fmts[0]));
            filters[1] = upload->f;
            fmts = upload->upload_fmts;
            num_fmts = upload->num_upload_fmts;
        }
    } else if (p->vo_convert && different_subfmt && info && info->hwdec_devs) {
        for (int n = 0; subfmt_converters[n].hw_imgfmt; n++) {
            if (subfmt_converters[n].hw_imgfmt == img->imgfmt) {
                MP_INFO(p, "Using HW sub-conversion.\n");
                filters[1] = subfmt_converters[n].create(conv);
                if (filters[1]) {
                    need_sws = false;
                    break;
                }
            }
        }
    }

    if (need_sws) {
        // Create a new conversion filter.
        struct mp_sws_filter *sws = mp_sws_filter_create(conv);
        if (!sws) {
            MP_ERR(p, "error creating conversion filter\n");
            return;
        }

        int out = mp_sws_find_best_out_format(img->imgfmt,  fmts, num_fmts);
        if (!out) {
            MP_ERR(p, "can't find video conversion for %s/%s\n",
                   mp_imgfmt_to_name(img->imgfmt),
                   mp_imgfmt_to_name(img->params.hw_subfmt));
            talloc_free(conv);
            mp_filter_internal_mark_failed(f);
            return;
        }

        if (out == img->imgfmt) {
            // Can happen if hwupload goes to same format.
            talloc_free(sws->f);
        } else {
            sws->out_format = out;
            MP_INFO(p, "Converting %s -> %s\n", mp_imgfmt_to_name(img->imgfmt),
                    mp_imgfmt_to_name(sws->out_format));
            filters[0] = sws->f;
        }
    }

    mp_chain_filters(conv->ppins[0], conv->ppins[1], filters, 2);

    p->sub.filter = conv;
    mp_subfilter_continue(&p->sub);
}

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_subfilter_read(&p->sub))
        return;

    struct mp_frame frame = p->sub.frame;

    if (!mp_frame_is_signaling(frame)) {
        if (p->num_imgfmts) {
            handle_video_frame(f);
            return;
        }
    }

    mp_subfilter_continue(&p->sub);
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    mp_subfilter_reset(&p->sub);
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    mp_subfilter_reset(&p->sub);
    TA_FREEP(&p->sub.filter);
}

static const struct mp_filter_info autoconvert_filter = {
    .name = "autoconvert",
    .priv_size = sizeof(struct priv),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

struct mp_autoconvert *mp_autoconvert_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &autoconvert_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->public.f = f;
    p->log = f->log;
    p->sub.in = f->ppins[0];
    p->sub.out = f->ppins[1];

    return &p->public;
}
