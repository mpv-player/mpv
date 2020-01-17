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

    // Hardware wrapper format, e.g. IMGFMT_VAAPI.
    int hw_imgfmt;

    // List of supported underlying surface formats.
    int *fmts;
    int num_fmts;
    // List of supported upload image formats. May contain duplicate entries
    // (which should be ignored).
    int *upload_fmts;
    int num_upload_fmts;
    // For fmts[n], fmt_upload_index[n] gives the index of the first supported
    // upload format in upload_fmts[], and fmt_upload_num[n] gives the number
    // of formats at this position.
    int *fmt_upload_index;
    int *fmt_upload_num;

    struct mp_hwupload public;
};

struct ffmpeg_and_other_bugs {
    int imgfmt;                             // hw format
    const int *const whitelist_formats;     // if non-NULL, allow only these
                                            // sw formats
    bool force_same_upload_fmt;             // force upload fmt == sw fmt
};

// This garbage is so complex and buggy. Hardcoding knowledge makes it work,
// trying to use the dynamic information returned by the API does not. So fuck
// this shit, I'll just whitelist the cases that work, what the fuck.
static const struct ffmpeg_and_other_bugs shitlist[] = {
    {
        .imgfmt = IMGFMT_VAAPI,
        .whitelist_formats = (const int[]){IMGFMT_NV12, IMGFMT_P010, IMGFMT_BGRA,
                                           IMGFMT_ABGR, IMGFMT_RGB0, 0},
        .force_same_upload_fmt = true,
    },
    {0}
};

static bool select_format(struct priv *p, int input_fmt, int *out_sw_fmt,
                          int *out_upload_fmt)
{
    if (!input_fmt)
        return false;

    // First find the closest sw fmt. Some hwdec APIs return crazy lists of
    // "supported" formats, which then are not supported or crash (???), so
    // the this is a good way to avoid problems.
    // (Actually we should just have hardcoded everything instead of relying on
    // this fragile bullshit FFmpeg API and the fragile bullshit hwdec drivers.)
    int sw_fmt = mp_imgfmt_select_best_list(p->fmts, p->num_fmts, input_fmt);
    if (!sw_fmt)
        return false;

    // Dumb, but find index for p->fmts[index]==sw_fmt.
    int index = -1;
    for (int n = 0; n < p->num_fmts; n++) {
        if (p->fmts[n] == sw_fmt)
            index = n;
    }
    if (index < 0)
        return false;

    // Now check the available upload formats. This is the format our sw frame
    // has to be in, and which the upload API will take (probably).

    int *upload_fmts = &p->upload_fmts[p->fmt_upload_index[index]];
    int num_upload_fmts = p->fmt_upload_num[index];

    int up_fmt = mp_imgfmt_select_best_list(upload_fmts, num_upload_fmts,
                                            input_fmt);
    if (!up_fmt)
        return false;

    *out_sw_fmt = sw_fmt;
    *out_upload_fmt = up_fmt;
    return true;
}

int mp_hwupload_find_upload_format(struct mp_hwupload *u, int imgfmt)
{
    struct priv *p = u->f->priv;

    int sw = 0, up = 0;
    select_format(p, imgfmt, &sw, &up);
    return up;
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

    if (src->imgfmt != p->last_input_fmt) {
        if (!select_format(p, src->imgfmt, &p->last_sw_fmt, &p->last_upload_fmt))
        {
            MP_ERR(f, "no hw upload format found\n");
            goto error;
        }
        if (src->imgfmt != p->last_upload_fmt) {
            // Should not fail; if it does, mp_hwupload_find_upload_format()
            // does not return the src->imgfmt format.
            MP_ERR(f, "input format not an upload format\n");
            goto error;
        }
        p->last_input_fmt = src->imgfmt;
        MP_INFO(f, "upload %s -> %s[%s]\n",
                mp_imgfmt_to_name(p->last_input_fmt),
                mp_imgfmt_to_name(p->hw_imgfmt),
                mp_imgfmt_to_name(p->last_sw_fmt));
    }

    if (!mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ctx, p->hw_imgfmt,
                                     p->last_sw_fmt, src->w, src->h))
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

    p->hw_imgfmt = hw_imgfmt;
    p->num_fmts = 0;
    p->num_upload_fmts = 0;

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

    const struct ffmpeg_and_other_bugs *bugs = NULL;
    for (int n = 0; shitlist[n].imgfmt; n++) {
        if (shitlist[n].imgfmt == hw_imgfmt) {
            bugs = &shitlist[n];
            break;
        }
    }

    for (int n = 0; cstr->valid_sw_formats &&
                    cstr->valid_sw_formats[n] != AV_PIX_FMT_NONE; n++)
    {
        int imgfmt = pixfmt2imgfmt(cstr->valid_sw_formats[n]);
        if (!imgfmt)
            continue;

        MP_VERBOSE(u->f, "looking at format %s/%s\n",
                   mp_imgfmt_to_name(hw_imgfmt),
                   mp_imgfmt_to_name(imgfmt));

        if (bugs && bugs->whitelist_formats) {
            bool found = false;
            for (int i = 0; bugs->whitelist_formats[i]; i++) {
                if (bugs->whitelist_formats[i] == imgfmt) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                MP_VERBOSE(u->f, "... skipping blacklisted format\n");
                continue;
            }
        }

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
            int index = p->num_fmts;
            MP_TARRAY_APPEND(p, p->fmts, p->num_fmts, imgfmt);
            MP_TARRAY_GROW(p, p->fmt_upload_index, index);
            MP_TARRAY_GROW(p, p->fmt_upload_num, index);

            p->fmt_upload_index[index] = p->num_upload_fmts;

            for (int i = 0; fmts[i] != AV_PIX_FMT_NONE; i++) {
                int fmt = pixfmt2imgfmt(fmts[i]);
                if (!fmt)
                    continue;
                MP_VERBOSE(u->f, "  supports %s\n", mp_imgfmt_to_name(fmt));
                if (bugs && bugs->force_same_upload_fmt && imgfmt != fmt) {
                    MP_VERBOSE(u->f, "  ... skipping blacklisted format\n");
                    continue;
                }
                if (!vo_supports(ctx, hw_imgfmt, fmt)) {
                    MP_VERBOSE(u->f, "  ... not supported by VO\n");
                    continue;
                }
                MP_TARRAY_APPEND(p, p->upload_fmts, p->num_upload_fmts, fmt);
            }

            p->fmt_upload_num[index] =
                p->num_upload_fmts - p->fmt_upload_index[index];

            av_free(fmts);
        }

        av_buffer_unref(&frames);
    }

    p->av_device_ctx = av_buffer_ref(ctx->av_device_ref);
    if (!p->av_device_ctx)
        return false;

    return p->num_upload_fmts > 0;
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

static void hwdownload_process(struct mp_filter *f)
{
    struct mp_hwdownload *d = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
    if (frame.type != MP_FRAME_VIDEO)
        goto passthrough;

    struct mp_image *src = frame.data;
    if (!src->hwctx)
        goto passthrough;

    struct mp_image *dst = mp_image_hw_download(src, d->pool);
    if (!dst) {
        MP_ERR(f, "Could not copy hardware frame to CPU memory.\n");
        goto passthrough;
    }

    mp_frame_unref(&frame);
    mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, dst));
    return;

passthrough:
    mp_pin_in_write(f->ppins[1], frame);
    return;
}

static const struct mp_filter_info hwdownload_filter = {
    .name = "hwdownload",
    .priv_size = sizeof(struct mp_hwdownload),
    .process = hwdownload_process,
};

struct mp_hwdownload *mp_hwdownload_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &hwdownload_filter);
    if (!f)
        return NULL;

    struct mp_hwdownload *d = f->priv;

    d->f = f;
    d->pool = mp_image_pool_new(d);

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    return d;
}
