#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mem.h>

#include "video/fmt-conversion.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#include "f_hwtransfer.h"
#include "filter_internal.h"

struct priv {
    AVBufferRef *av_device_ctx;

    AVBufferRef *hw_pool;

    int last_input_fmt;
    int last_upload_fmt;
    int last_sw_fmt;

    struct mp_hwupload public;
};

static bool update_format_decision(struct priv *p, int input_fmt)
{
    struct mp_hwupload *u = &p->public;

    if (!input_fmt)
        return false;

    if (input_fmt == p->last_input_fmt)
        return true;

    p->last_input_fmt = 0;

    int res = mp_imgfmt_select_best_list(u->upload_fmts, u->num_upload_fmts,
                                         input_fmt);

    if (!res)
        return false;

    // Find which sw format we should use.
    // NOTE: if there are ever any hw APIs that actually do expensive
    // conversions on mismatching format uploads, we should probably first look
    // which sw format is preferred?
    int index = -1;
    for (int n = 0; n < u->num_upload_fmts; n++) {
        if (u->upload_fmts[n] == res)
            index = n;
    }

    if (index < 0)
        return false;

    for (int n = 0; n < u->num_fmts; n++) {
        if (index >= u->fmt_upload_index[n] &&
            index < u->fmt_upload_index[n] + u->fmt_upload_num[n])
        {
            p->last_input_fmt = input_fmt;
            p->last_upload_fmt = u->upload_fmts[index];
            p->last_sw_fmt = u->fmts[n];
            MP_INFO(u->f, "upload %s -> %s (%s)\n",
                    mp_imgfmt_to_name(p->last_input_fmt),
                    mp_imgfmt_to_name(p->last_upload_fmt),
                    mp_imgfmt_to_name(p->last_sw_fmt));
            return true;
        }
    }

    return false;
}

int mp_hwupload_find_upload_format(struct mp_hwupload *u, int imgfmt)
{
    struct priv *p = u->f->priv;

    if (!update_format_decision(p, imgfmt))
        return 0;
    return p->last_upload_fmt;
}

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
    if (mp_frame_is_signaling(frame)) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }
    if (frame.type != MP_FRAME_VIDEO) {
        MP_ERR(f, "unsupported frame type\n");
        goto error;
    }
    struct mp_image *src = frame.data;

    // As documented, just pass though HW frames.
    if (IMGFMT_IS_HWACCEL(src->imgfmt)) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    if (src->w % 2 || src->h % 2) {
        MP_ERR(f, "non-mod 2 input frames unsupported\n");
        goto error;
    }

    if (!update_format_decision(p, src->imgfmt)) {
        MP_ERR(f, "no hw upload format found\n");
        goto error;
    }

    if (!mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ctx,
                                     p->public.hw_imgfmt, p->last_sw_fmt,
                                     src->w, src->h))
    {
        MP_ERR(f, "failed to create frame pool\n");
        goto error;
    }

    struct mp_image *dst = mp_av_pool_image_hw_upload(p->hw_pool, src);
    if (!dst)
        goto error;

    mp_frame_unref(&frame);
    mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, dst));

    return;

error:
    mp_frame_unref(&frame);
    MP_ERR(f, "failed to upload frame\n");
    mp_filter_internal_mark_failed(f);
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    av_buffer_unref(&p->hw_pool);
    av_buffer_unref(&p->av_device_ctx);
}

static const struct mp_filter_info hwupload_filter = {
    .name = "hwupload",
    .priv_size = sizeof(struct priv),
    .process = process,
    .destroy = destroy,
};

// The VO layer might have restricted format support. It might actually
// work if this is input to a conversion filter anyway, but our format
// negotiation is too stupid and non-existent to detect this.
// So filter out all not explicitly supported formats.
static bool vo_supports(struct mp_hwdec_ctx *ctx, int hw_fmt, int sw_fmt)
{
    if (!ctx->hw_imgfmt)
        return true; // if unset, all formats are allowed
    if (ctx->hw_imgfmt != hw_fmt)
        return false;

    for (int i = 0; ctx->supported_formats &&  ctx->supported_formats[i]; i++) {
        if (ctx->supported_formats[i] == sw_fmt)
            return true;
    }

    return false;
}

static bool probe_formats(struct mp_hwupload *u, int hw_imgfmt)
{
    struct priv *p = u->f->priv;

    u->hw_imgfmt = hw_imgfmt;
    u->num_fmts = 0;
    u->num_upload_fmts = 0;

    struct mp_stream_info *info = mp_filter_find_stream_info(u->f);
    if (!info || !info->hwdec_devs) {
        MP_ERR(u->f, "no hw context\n");
        return false;
    }

    struct mp_hwdec_ctx *ctx = NULL;
    AVHWFramesConstraints *cstr = NULL;

    for (int n = 0; ; n++) {
        struct mp_hwdec_ctx *cur = hwdec_devices_get_n(info->hwdec_devs, n);
        if (!cur)
            break;
        if (!cur->av_device_ref)
            continue;
        cstr = av_hwdevice_get_hwframe_constraints(cur->av_device_ref, NULL);
        if (!cstr)
            continue;
        bool found = false;
        for (int i = 0; cstr->valid_hw_formats &&
                        cstr->valid_hw_formats[i] != AV_PIX_FMT_NONE; i++)
        {
            found |= cstr->valid_hw_formats[i] == imgfmt2pixfmt(hw_imgfmt);
        }
        if (found && (!cur->hw_imgfmt || cur->hw_imgfmt == hw_imgfmt)) {
            ctx = cur;
            break;
        }
        av_hwframe_constraints_free(&cstr);
    }

    if (!ctx) {
        MP_ERR(u->f, "no support for this hw format\n");
        return false;
    }

    // Probe for supported formats. This is very roundabout, because the
    // hwcontext API does not give us this information directly. We resort to
    // creating temporary AVHWFramesContexts in order to retrieve the list of
    // supported formats. This should be relatively cheap as we don't create
    // any real frames (although some backends do for probing info).

    for (int n = 0; cstr->valid_sw_formats &&
                    cstr->valid_sw_formats[n] != AV_PIX_FMT_NONE; n++)
    {
        int imgfmt = pixfmt2imgfmt(cstr->valid_sw_formats[n]);
        if (!imgfmt)
            continue;

        MP_VERBOSE(u->f, "looking at format %s\n", mp_imgfmt_to_name(imgfmt));

        // Creates an AVHWFramesContexts with the given parameters.
        AVBufferRef *frames = NULL;
        if (!mp_update_av_hw_frames_pool(&frames, ctx->av_device_ref,
                                        hw_imgfmt, imgfmt, 128, 128))
        {
            MP_WARN(u->f, "failed to allocate pool\n");
            continue;
        }

        enum AVPixelFormat *fmts;
        if (av_hwframe_transfer_get_formats(frames,
                            AV_HWFRAME_TRANSFER_DIRECTION_TO, &fmts, 0) >= 0)
        {
            int index = u->num_fmts;
            MP_TARRAY_APPEND(p, u->fmts, u->num_fmts, imgfmt);
            MP_TARRAY_GROW(p, u->fmt_upload_index, index);
            MP_TARRAY_GROW(p, u->fmt_upload_num, index);

            u->fmt_upload_index[index] = u->num_upload_fmts;

            for (int i = 0; fmts[i] != AV_PIX_FMT_NONE; i++) {
                int fmt = pixfmt2imgfmt(fmts[i]);
                if (!fmt)
                    continue;
                MP_VERBOSE(u->f, "supports %s\n", mp_imgfmt_to_name(fmt));
                if (vo_supports(ctx, hw_imgfmt, fmt))
                    MP_TARRAY_APPEND(p, u->upload_fmts, u->num_upload_fmts, fmt);
            }

            u->fmt_upload_num[index] =
                u->num_upload_fmts - u->fmt_upload_index[index];

            av_free(fmts);
        }

        av_buffer_unref(&frames);
    }

    p->av_device_ctx = av_buffer_ref(ctx->av_device_ref);
    if (!p->av_device_ctx)
        return false;

    return u->num_upload_fmts > 0;
}

struct mp_hwupload *mp_hwupload_create(struct mp_filter *parent, int hw_imgfmt)
{
    struct mp_filter *f = mp_filter_create(parent, &hwupload_filter);
    if (!f)
        return NULL;

    struct priv *p = f->priv;
    struct mp_hwupload *u = &p->public;
    u->f = f;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    if (!probe_formats(u, hw_imgfmt)) {
        MP_ERR(f, "hardware format not supported\n");
        goto error;
    }

    return u;
error:
    talloc_free(f);
    return NULL;
}
