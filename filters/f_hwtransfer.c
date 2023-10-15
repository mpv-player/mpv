#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>

#include "video/fmt-conversion.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#include "f_hwtransfer.h"
#include "f_output_chain.h"
#include "f_utils.h"
#include "filter_internal.h"
#include "user_filters.h"

struct priv {
    AVBufferRef *av_device_ctx;

    AVBufferRef *hw_pool;

    int last_source_fmt;
    int last_hw_output_fmt;
    int last_hw_input_fmt;

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

    // List of source formats that require hwmap instead of hwupload.
    int *map_fmts;
    int num_map_fmts;

    // If the selected hwdec has a conversion filter available for converting
    // between sw formats in hardware, the name will be set. NULL otherwise.
    const char *conversion_filter_name;
};

struct hwmap_pairs {
    int first_fmt;
    int second_fmt;
};

// We cannot discover which pairs of hardware formats need to use hwmap to
// convert between the formats, so we need a lookup table.
static const struct hwmap_pairs hwmap_pairs[] = {
#if HAVE_VULKAN_INTEROP
    {
        .first_fmt = IMGFMT_VAAPI,
        .second_fmt = IMGFMT_VULKAN,
    },
#endif
    {
        .first_fmt = IMGFMT_DRMPRIME,
        .second_fmt = IMGFMT_VAAPI,
    },
    {0}
};

/**
 * @brief Find the closest supported format when hw uploading
 *
 * Return the best format suited for upload that is supported for a given input
 * imgfmt. This returns the same as imgfmt if the format is natively supported,
 * and otherwise a format that likely results in the least loss.
 * Returns 0 if completely unsupported.
 *
 * Some hardware types support implicit format conversion on upload. For these
 * types, it is possible for the set of formats that are accepts as inputs to
 * the upload process to differ from the set of formats that can be outputs of
 * the upload.
 *
 * hw_input_format -> hwupload -> hw_output_format
 *
 * Awareness of this is important because we can avoid doing software conversion
 * if our input_fmt is accepted as a hw_input_format even if it cannot be the
 * hw_output_format.
 */
static bool select_format(struct priv *p, int input_fmt,
                          int *out_hw_input_fmt, int *out_hw_output_fmt)
{
    if (!input_fmt)
        return false;

    // If the input format is a hw format, then we shouldn't be doing this
    // format selection here at all.
    if (IMGFMT_IS_HWACCEL(input_fmt)) {
        return false;
    }

    // First find the closest hw input fmt. Some hwdec APIs return crazy lists of
    // "supported" formats, which then are not supported or crash (???), so
    // the this is a good way to avoid problems.
    // (Actually we should just have hardcoded everything instead of relying on
    // this fragile bullshit FFmpeg API and the fragile bullshit hwdec drivers.)
    int hw_input_fmt = mp_imgfmt_select_best_list(p->fmts, p->num_fmts, input_fmt);
    if (!hw_input_fmt)
        return false;

    // Dumb, but find index for p->fmts[index]==hw_input_fmt.
    int index = -1;
    for (int n = 0; n < p->num_fmts; n++) {
        if (p->fmts[n] == hw_input_fmt)
            index = n;
    }
    if (index < 0)
        return false;

    // Now check the available output formats. This is the format our sw frame
    // will be in after the upload (probably).
    int *upload_fmts = &p->upload_fmts[p->fmt_upload_index[index]];
    int num_upload_fmts = p->fmt_upload_num[index];

    int hw_output_fmt = mp_imgfmt_select_best_list(upload_fmts, num_upload_fmts,
                                            input_fmt);
    if (!hw_output_fmt)
        return false;

    *out_hw_input_fmt = hw_input_fmt;
    *out_hw_output_fmt = hw_output_fmt;
    return true;
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

    /*
     * Just pass though HW frames in the same format. This shouldn't normally
     * occur as the upload filter will not be inserted when the formats already
     * match.
     *
     * Technically, we could have frames from different device contexts,
     * which would require an explicit transfer, but mpv doesn't let you
     * create that configuration.
     */
    if (src->imgfmt == p->hw_imgfmt) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    if (src->imgfmt != p->last_source_fmt) {
        if (IMGFMT_IS_HWACCEL(src->imgfmt)) {
            // Because there cannot be any conversion of the sw format when the
            // input is a hw format, just pick the source sw format.
            p->last_hw_input_fmt = p->last_hw_output_fmt = src->params.hw_subfmt;
        } else {
            if (!select_format(p, src->imgfmt,
                               &p->last_hw_input_fmt, &p->last_hw_output_fmt))
            {
                MP_ERR(f, "no hw upload format found\n");
                goto error;
            }
            if (src->imgfmt != p->last_hw_input_fmt) {
                // Should not fail; if it does, mp_hwupload_find_upload_format()
                // does not return the src->imgfmt format.
                MP_ERR(f, "input format is not an upload format\n");
                goto error;
            }
        }
        p->last_source_fmt = src->imgfmt;
        MP_INFO(f, "upload %s -> %s[%s]\n",
                mp_imgfmt_to_name(p->last_source_fmt),
                mp_imgfmt_to_name(p->hw_imgfmt),
                mp_imgfmt_to_name(p->last_hw_output_fmt));
    }

    if (!mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ctx, p->hw_imgfmt,
                                     p->last_hw_output_fmt, src->w, src->h,
                                     src->imgfmt == IMGFMT_CUDA))
    {
        MP_ERR(f, "failed to create frame pool\n");
        goto error;
    }

    struct mp_image *dst;
    bool map_images = false;
    for (int n = 0; n < p->num_map_fmts; n++) {
        if (src->imgfmt == p->map_fmts[n]) {
            map_images = true;
            break;
        }
    }

    if (map_images)
        dst = mp_av_pool_image_hw_map(p->hw_pool, src);
    else
        dst = mp_av_pool_image_hw_upload(p->hw_pool, src);
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
    if (ctx->hw_imgfmt != hw_fmt)
        return false;
    if (!ctx->supported_formats)
        return true; // if unset, all formats are allowed

    for (int i = 0; ctx->supported_formats &&  ctx->supported_formats[i]; i++) {
        if (ctx->supported_formats[i] == sw_fmt)
            return true;
    }

    return false;
}

/**
 * Some hwcontexts do not implement constraints, and so cannot
 * report supported formats, so cobble something together from our
 * static metadata.
 */
static AVHWFramesConstraints *build_static_constraints(struct mp_hwdec_ctx *ctx)
{
    AVHWFramesConstraints *cstr = NULL;
    cstr = av_malloc(sizeof(AVHWFramesConstraints));
    if (!cstr)
        return NULL;

    cstr->valid_hw_formats =
        av_malloc_array(2, sizeof(*cstr->valid_hw_formats));
    if (!cstr->valid_hw_formats)
        goto fail;
    cstr->valid_hw_formats[0] = imgfmt2pixfmt(ctx->hw_imgfmt);
    cstr->valid_hw_formats[1] = AV_PIX_FMT_NONE;

    int num_sw_formats;
    for (num_sw_formats = 0;
            ctx->supported_formats[num_sw_formats] != 0;
            num_sw_formats++);

    cstr->valid_sw_formats =
        av_malloc_array(num_sw_formats + 1,
                        sizeof(*cstr->valid_sw_formats));
    if (!cstr->valid_sw_formats)
        goto fail;
    for (int i = 0; i < num_sw_formats; i++) {
        cstr->valid_sw_formats[i] = imgfmt2pixfmt(ctx->supported_formats[i]);
    }
    cstr->valid_sw_formats[num_sw_formats] = AV_PIX_FMT_NONE;

    return cstr;

 fail:
    av_freep(&cstr->valid_hw_formats);
    av_freep(&cstr->valid_sw_formats);
    return NULL;
}

static bool probe_formats(struct mp_filter *f, int hw_imgfmt, bool use_conversion_filter)
{
    struct priv *p = f->priv;

    p->hw_imgfmt = hw_imgfmt;
    p->num_fmts = 0;
    p->num_upload_fmts = 0;

    struct mp_stream_info *info = mp_filter_find_stream_info(f);
    if (!info || !info->hwdec_devs) {
        MP_ERR(f, "no hw context\n");
        return false;
    }

    struct mp_hwdec_ctx *ctx = NULL;
    AVHWFramesConstraints *cstr = NULL;
    AVHWFramesConstraints *conversion_cstr = NULL;

    struct hwdec_imgfmt_request params = {
        .imgfmt = hw_imgfmt,
        .probing = true,
    };
    hwdec_devices_request_for_img_fmt(info->hwdec_devs, &params);

    for (int n = 0; ; n++) {
        struct mp_hwdec_ctx *cur = hwdec_devices_get_n(info->hwdec_devs, n);
        if (!cur)
            break;
        if (!cur->av_device_ref)
            continue;
        cstr = av_hwdevice_get_hwframe_constraints(cur->av_device_ref, NULL);
        if (!cstr) {
            MP_VERBOSE(f, "hwdec '%s' does not report hwframe constraints. "
                          "Using static metadata.\n", cur->driver_name);
            cstr = build_static_constraints(cur);
        }
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
        MP_INFO(f, "no support for this hw format\n");
        return false;
    }

    // Probe for supported formats. This is very roundabout, because the
    // hwcontext API does not give us this information directly. We resort to
    // creating temporary AVHWFramesContexts in order to retrieve the list of
    // supported formats. This should be relatively cheap as we don't create
    // any real frames (although some backends do for probing info).

    for (int n = 0; hwmap_pairs[n].first_fmt; n++) {
        if (hwmap_pairs[n].first_fmt == hw_imgfmt) {
            MP_TARRAY_APPEND(p, p->map_fmts, p->num_map_fmts,
                             hwmap_pairs[n].second_fmt);
        } else if (hwmap_pairs[n].second_fmt == hw_imgfmt) {
            MP_TARRAY_APPEND(p, p->map_fmts, p->num_map_fmts,
                             hwmap_pairs[n].first_fmt);
        }
    }

    if (use_conversion_filter) {
        // We will not be doing a transfer, so do not probe for transfer
        // formats. This can produce incorrect results. Instead, we need to
        // obtain the constraints for a conversion configuration.

        conversion_cstr =
            av_hwdevice_get_hwframe_constraints(ctx->av_device_ref,
                                                ctx->conversion_config);
    }

    for (int n = 0; cstr->valid_sw_formats &&
                    cstr->valid_sw_formats[n] != AV_PIX_FMT_NONE; n++)
    {
        int *not_supported_by_vo = NULL;
        int num_not_supported = 0;
        int imgfmt = pixfmt2imgfmt(cstr->valid_sw_formats[n]);
        if (!imgfmt)
            continue;

        MP_DBG(f, "looking at format %s/%s\n",
                   mp_imgfmt_to_name(hw_imgfmt),
                   mp_imgfmt_to_name(imgfmt));

        if (IMGFMT_IS_HWACCEL(imgfmt)) {
            // If the enumerated format is a hardware format, we don't need to
            // do any further probing. It will be supported.
            MP_DBG(f, "  supports %s (a hardware format)\n",
                       mp_imgfmt_to_name(imgfmt));
            continue;
        }

        if (use_conversion_filter) {
            // The conversion constraints are universal, and do not vary with
            // source format, so we will associate the same set of target formats
            // with all source formats.
            int index = p->num_fmts;
            MP_TARRAY_APPEND(p, p->fmts, p->num_fmts, imgfmt);
            MP_TARRAY_GROW(p, p->fmt_upload_index, index);
            MP_TARRAY_GROW(p, p->fmt_upload_num, index);

            p->fmt_upload_index[index] = p->num_upload_fmts;

            /*
             * First check if the VO supports the source format. If it does,
             * ensure it is in the target list, so that we never do an
             * unnecessary conversion. This explicit step is required because
             * there can be situations where the conversion filter cannot output
             * the source format, but the VO can accept it, so just looking at
             * the supported conversion targets can make it seem as if a
             * conversion is required.
             */
            if (!ctx->supported_formats) {
                /*
                 * If supported_formats is unset, that means we should assume
                 * the VO can accept all source formats, so append the source
                 * format.
                 */
                MP_TARRAY_APPEND(p, p->upload_fmts, p->num_upload_fmts, imgfmt);
            } else {
                for (int i = 0; ctx->supported_formats[i]; i++) {
                    int fmt = ctx->supported_formats[i];
                    if (fmt == imgfmt) {
                        MP_DBG(f, "  vo accepts %s\n", mp_imgfmt_to_name(fmt));
                        MP_TARRAY_APPEND(p, p->upload_fmts, p->num_upload_fmts, fmt);
                    }
                }
            }

            enum AVPixelFormat *fmts = conversion_cstr ?
                                       conversion_cstr->valid_sw_formats : NULL;
            MP_DBG(f, "  supports:");
            for (int i = 0; fmts && fmts[i] != AV_PIX_FMT_NONE; i++) {
                int fmt = pixfmt2imgfmt(fmts[i]);
                if (!fmt)
                    continue;
                if (!vo_supports(ctx, hw_imgfmt, fmt)) {
                    MP_TARRAY_APPEND(p, not_supported_by_vo, num_not_supported, fmt);
                    continue;
                }
                MP_DBG(f, " %s", mp_imgfmt_to_name(fmt));
                MP_TARRAY_APPEND(p, p->upload_fmts, p->num_upload_fmts, fmt);
            }
            if (num_not_supported) {
                MP_DBG(f, "\n  not supported by VO:");
                for (int i = 0; i < num_not_supported; i++) {
                    MP_DBG(f, " %s", mp_imgfmt_to_name(not_supported_by_vo[i]));
                }
            }
            MP_DBG(f, "\n");

            p->fmt_upload_num[index] =
                p->num_upload_fmts - p->fmt_upload_index[index];
        } else {
            // Creates an AVHWFramesContexts with the given parameters.
            AVBufferRef *frames = NULL;
            if (!mp_update_av_hw_frames_pool(&frames, ctx->av_device_ref,
                                            hw_imgfmt, imgfmt, 128, 128, false))
            {
                MP_WARN(f, "failed to allocate pool\n");
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

                MP_DBG(f, "  supports:");
                for (int i = 0; fmts[i] != AV_PIX_FMT_NONE; i++) {
                    int fmt = pixfmt2imgfmt(fmts[i]);
                    if (!fmt)
                        continue;
                    if (!vo_supports(ctx, hw_imgfmt, fmt)) {
                        MP_TARRAY_APPEND(p, not_supported_by_vo, num_not_supported, fmt);
                        continue;
                    }
                    MP_DBG(f, " %s", mp_imgfmt_to_name(fmt));
                    MP_TARRAY_APPEND(p, p->upload_fmts, p->num_upload_fmts, fmt);
                }
                if (num_not_supported) {
                    MP_DBG(f, "\n  not supported by VO:");
                    for (int i = 0; i < num_not_supported; i++) {
                        MP_DBG(f, " %s", mp_imgfmt_to_name(not_supported_by_vo[i]));
                    }
                }
                MP_DBG(f, "\n");

                p->fmt_upload_num[index] =
                    p->num_upload_fmts - p->fmt_upload_index[index];

                av_free(fmts);
            }

            av_buffer_unref(&frames);
        }
        talloc_free(not_supported_by_vo);
    }

    av_hwframe_constraints_free(&cstr);
    av_hwframe_constraints_free(&conversion_cstr);
    p->av_device_ctx = av_buffer_ref(ctx->av_device_ref);
    if (!p->av_device_ctx)
        return false;
    p->conversion_filter_name = ctx->conversion_filter_name;

    return p->num_upload_fmts > 0;
}

struct mp_hwupload mp_hwupload_create(struct mp_filter *parent, int hw_imgfmt,
                                       int sw_imgfmt, bool src_is_same_hw)
{
    struct mp_hwupload u = {0,};
    struct mp_filter *f = mp_filter_create(parent, &hwupload_filter);
    if (!f)
        return u;

    struct priv *p = f->priv;
    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    if (!probe_formats(f, hw_imgfmt, src_is_same_hw)) {
        MP_INFO(f, "hardware format not supported\n");
        goto fail;
    }

    int hw_input_fmt = 0, hw_output_fmt = 0;
    if (!select_format(p, sw_imgfmt, &hw_input_fmt, &hw_output_fmt)) {
        MP_ERR(f, "Unable to find a compatible upload format for %s\n",
               mp_imgfmt_to_name(sw_imgfmt));
        goto fail;
    }

    if (src_is_same_hw) {
        if (p->conversion_filter_name) {
            /*
            * If we are converting from one sw format to another within the same
            * hw format, we will use that hw format's conversion filter rather
            * than the actual hwupload filter.
            */
            u.selected_sw_imgfmt = hw_output_fmt;
            if (sw_imgfmt != u.selected_sw_imgfmt) {
                enum AVPixelFormat pixfmt = imgfmt2pixfmt(u.selected_sw_imgfmt);
                const char *avfmt_name = av_get_pix_fmt_name(pixfmt);
                char *args[] = {"format", (char *)avfmt_name, NULL};
                MP_VERBOSE(f, "Hardware conversion: %s -> %s\n",
                           p->conversion_filter_name, avfmt_name);
                struct mp_filter *sv =
                    mp_create_user_filter(parent, MP_OUTPUT_CHAIN_VIDEO,
                                        p->conversion_filter_name, args);
                u.f = sv;
                talloc_free(f);
            }
        }
    } else {
        u.f = f;
        /*
         * In the case where the imgfmt is not natively supported, it must be
         * converted, either before or during upload. If the imgfmt is supported
         * as a hw input format, then prefer that, and if the upload has to do
         * implicit conversion, that's fine. On the other hand, if the imgfmt is
         * not a supported input format, then pick the output format as the
         * conversion target to avoid doing two conversions (one before upload,
         * and one during upload). Note that for most hardware types, there is
         * no ability to convert during upload, and the two formats will always
         * be the same.
         */
        u.selected_sw_imgfmt =
            sw_imgfmt == hw_input_fmt ? hw_input_fmt : hw_output_fmt;
    }

    u.successful_init = true;
    return u;
fail:
    talloc_free(f);
    return u;
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
