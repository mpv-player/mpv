/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <windows.h>
#include <d3d11.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>

#include "common/common.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"
#include "filters/f_autoconvert.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "refqueue.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

// missing in MinGW
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND 0x1
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB 0x2
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE 0x4
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION 0x8
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE 0x10
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_FRAME_RATE_CONVERSION 0x20

// For video procesor extensions identifiers reference see:
// https://chromium.googlesource.com/chromium/src/+/5f354f38/ui/gl/swap_chain_presenter.cc

#ifndef NVIDIA_PPE_INTERFACE_GUID
DEFINE_GUID(NVIDIA_PPE_INTERFACE_GUID,
            0xd43ce1b3, 0x1f4b, 0x48ac, 0xba, 0xee,
            0xc3, 0xc2, 0x53, 0x75, 0xe6, 0xf7);
#endif

#ifndef INTEL_VPE_INTERFACE_GUID
DEFINE_GUID(INTEL_VPE_INTERFACE_GUID,
            0xedd1d4b9, 0x8659, 0x4cbc, 0xa4, 0xd6,
            0x98, 0x31, 0xa2, 0x16, 0x3a, 0xc3);
#endif

static const unsigned int intel_vpe_fn_version = 0x1;
static const unsigned int intel_vpe_version    = 0x3;

static const unsigned int intel_vpe_fn_scaling  = 0x37;
static const unsigned int intel_vpe_scaling_vsr = 0x2;

static const unsigned int intel_vpe_fn_mode      = 0x20;
static const unsigned int intel_vpe_mode_preproc = 0x1;

enum scaling_mode {
    SCALING_BASIC,
    SCALING_INTEL_VSR,
    SCALING_NVIDIA_RTX,
};

struct opts {
    bool deint_enabled;
    float scale;
    int scaling_mode;
    bool interlaced_only;
    int mode;
    int field_parity;
    int format;
};

struct priv {
    struct opts *opts;

    ID3D11Device *vo_dev;

    ID3D11DeviceContext *device_ctx;
    ID3D11VideoDevice *video_dev;
    ID3D11VideoContext *video_ctx;

    ID3D11VideoProcessor *video_proc;
    ID3D11VideoProcessorEnumerator *vp_enum;
    D3D11_VIDEO_FRAME_FORMAT d3d_frame_format;

    bool require_filtering;

    struct mp_image_params params, out_params;
    int c_w, c_h;

    AVBufferRef *av_device_ref;
    AVBufferRef *hw_pool;

    struct mp_refqueue *queue;
};


static void flush_frames(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    mp_refqueue_flush(p->queue);
}

static void destroy_video_proc(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    if (p->video_proc)
        ID3D11VideoProcessor_Release(p->video_proc);
    p->video_proc = NULL;

    if (p->vp_enum)
        ID3D11VideoProcessorEnumerator_Release(p->vp_enum);
    p->vp_enum = NULL;
}

static void enable_nvidia_rtx_extension(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct nvidia_ext {
        unsigned int version;
        unsigned int method;
        unsigned int enable;
    } ext = {1, 2, 1};

    HRESULT hr = ID3D11VideoContext_VideoProcessorSetStreamExtension(p->video_ctx,
                                                                     p->video_proc,
                                                                     0,
                                                                     &NVIDIA_PPE_INTERFACE_GUID,
                                                                     sizeof(ext),
                                                                     &ext);

    if (FAILED(hr)) {
        MP_WARN(vf, "Failed to enable NVIDIA RTX Super Resolution: %s\n", mp_HRESULT_to_str(hr));
    } else {
        MP_VERBOSE(vf, "NVIDIA RTX Super Resolution enabled\n");
    }
}

static void enable_intel_vsr_extension(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct intel_vpe_ext {
        unsigned int function;
        const void* param;
    } ext;

    ext = (struct intel_vpe_ext){intel_vpe_fn_version, &intel_vpe_version};
    HRESULT hr = ID3D11VideoContext_VideoProcessorSetOutputExtension(p->video_ctx,
                                                                     p->video_proc,
                                                                     &INTEL_VPE_INTERFACE_GUID,
                                                                     sizeof(ext),
                                                                     &ext);
    if (FAILED(hr))
        goto failed;

    ext = (struct intel_vpe_ext){intel_vpe_fn_mode, &intel_vpe_mode_preproc};
    hr = ID3D11VideoContext_VideoProcessorSetOutputExtension(p->video_ctx,
                                                             p->video_proc,
                                                             &INTEL_VPE_INTERFACE_GUID,
                                                             sizeof(ext),
                                                             &ext);
    if (FAILED(hr))
        goto failed;

    ext = (struct intel_vpe_ext){intel_vpe_fn_scaling, &intel_vpe_scaling_vsr};
    hr = ID3D11VideoContext_VideoProcessorSetStreamExtension(p->video_ctx,
                                                             p->video_proc,
                                                             0,
                                                             &INTEL_VPE_INTERFACE_GUID,
                                                             sizeof(ext),
                                                             &ext);
    if (FAILED(hr))
        goto failed;

    MP_VERBOSE(vf, "Intel Video Super Resolution enabled\n");
    return;

failed:
    MP_WARN(vf, "Failed to enable Intel Video Super Resolution: %s\n", mp_HRESULT_to_str(hr));
}

static int recreate_video_proc(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    HRESULT hr;

    destroy_video_proc(vf);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpdesc = {
        .InputFrameFormat = p->d3d_frame_format,
        .InputWidth = p->c_w,
        .InputHeight = p->c_h,
        .OutputWidth = p->out_params.w,
        .OutputHeight = p->out_params.h,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(p->video_dev, &vpdesc,
                                                          &p->vp_enum);
    if (FAILED(hr))
        goto fail;

    int rindex = p->opts->mode ? -1 : 0;
    if (rindex == 0)
        goto create;

    D3D11_VIDEO_PROCESSOR_CAPS caps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(p->vp_enum, &caps);
    if (FAILED(hr))
        goto fail;

    MP_VERBOSE(vf, "Found %d rate conversion caps. Looking for caps=0x%x.\n",
               (int)caps.RateConversionCapsCount, p->opts->mode);

    for (int n = 0; n < caps.RateConversionCapsCount; n++) {
        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rcaps;
        hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps
                (p->vp_enum, n, &rcaps);
        if (FAILED(hr))
            goto fail;
        MP_VERBOSE(vf, "  - %d: 0x%08x\n", n, (unsigned)rcaps.ProcessorCaps);
        if (rcaps.ProcessorCaps & p->opts->mode) {
            MP_VERBOSE(vf, "       (matching)\n");
            if (rindex < 0)
                rindex = n;
        }
    }

    if (rindex < 0) {
        MP_WARN(vf, "No fitting video processor found, picking #0.\n");
        rindex = 0;
    }

    // TODO: so, how do we select which rate conversion mode the processor uses?

create:
    hr = ID3D11VideoDevice_CreateVideoProcessor(p->video_dev, p->vp_enum, rindex,
                                                &p->video_proc);
    if (FAILED(hr)) {
        MP_ERR(vf, "Failed to create D3D11 video processor.\n");
        goto fail;
    }

    // Note: libavcodec does not support cropping left/top with hwaccel.
    RECT src_rc = {
        .right = p->params.w,
        .bottom = p->params.h,
    };
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(p->video_ctx,
                                                         p->video_proc,
                                                         0, TRUE, &src_rc);

    // This is supposed to stop drivers from fucking up the video quality.
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(p->video_ctx,
                                                                 p->video_proc,
                                                                 0, FALSE);

    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(p->video_ctx,
                                                         p->video_proc,
                                                         0,
                                                         D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL,
                                                         FALSE, 0);

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE csp = {
        .YCbCr_Matrix = p->params.repr.sys != PL_COLOR_SYSTEM_BT_601,
        .Nominal_Range = p->params.repr.levels == PL_COLOR_LEVELS_LIMITED ? 1 : 2,
    };
    ID3D11VideoContext_VideoProcessorSetStreamColorSpace(p->video_ctx,
                                                         p->video_proc,
                                                         0, &csp);
    ID3D11VideoContext_VideoProcessorSetOutputColorSpace(p->video_ctx,
                                                         p->video_proc,
                                                         &csp);

    switch (p->opts->scaling_mode) {
    case SCALING_INTEL_VSR:
        enable_intel_vsr_extension(vf);
        break;
    case SCALING_NVIDIA_RTX:
        enable_nvidia_rtx_extension(vf);
        break;
    }

    return 0;
fail:
    destroy_video_proc(vf);
    return -1;
}

static struct mp_image *alloc_out(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    if (!mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ref,
                                     IMGFMT_D3D11, p->out_params.hw_subfmt,
                                     p->out_params.w, p->out_params.h, false))
    {
        MP_ERR(vf, "Failed to create hw pool\n");
        return NULL;
    }

    AVHWFramesContext *hw_frame_ctx = (void *)p->hw_pool->data;
    AVD3D11VAFramesContext *d3d11va_frames_ctx = hw_frame_ctx->hwctx;
    d3d11va_frames_ctx->BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    AVFrame *av_frame = av_frame_alloc();
    MP_HANDLE_OOM(av_frame);
    if (av_hwframe_get_buffer(p->hw_pool, av_frame, 0) < 0) {
        MP_ERR(vf, "Failed to allocate frame from hw pool\n");
        av_frame_free(&av_frame);
        return NULL;
    }

    struct mp_image *img = mp_image_from_av_frame(av_frame);
    av_frame_free(&av_frame);
    if (!img) {
        MP_ERR(vf, "Internal error when converting AVFrame\n");
        return NULL;
    }

    return img;
}

static struct mp_image *render(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    int res = -1;
    HRESULT hr;
    ID3D11VideoProcessorInputView *in_view = NULL;
    ID3D11VideoProcessorOutputView *out_view = NULL;
    struct mp_image *in = NULL, *out = NULL;
    out = alloc_out(vf);
    if (!out) {
        MP_WARN(vf, "failed to allocate frame\n");
        goto cleanup;
    }

    ID3D11Texture2D *d3d_out_tex = (void *)out->planes[0];

    in = mp_refqueue_get(p->queue, 0);
    if (!in)
        goto cleanup;
    ID3D11Texture2D *d3d_tex = (void *)in->planes[0];
    int d3d_subindex = (intptr_t)in->planes[1];

    mp_image_copy_attributes(out, in);
    // mp_image_copy_attributes overwrites the height and width
    // set it the size back if we are using scale
    mp_image_set_size(out, p->out_params.w, p->out_params.h);
    // mp_image_copy_attributes will set the crop value to the origin
    // width and height, set the crop back to the default state
    out->params.crop = p->out_params.crop;

    D3D11_VIDEO_FRAME_FORMAT d3d_frame_format;
    if (!mp_refqueue_should_deint(p->queue)) {
        d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    } else if (mp_refqueue_top_field_first(p->queue)) {
        d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
    } else {
        d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
    }

    D3D11_TEXTURE2D_DESC texdesc;
    ID3D11Texture2D_GetDesc(d3d_tex, &texdesc);
    if (!p->video_proc || p->c_w != texdesc.Width || p->c_h != texdesc.Height ||
        p->d3d_frame_format != d3d_frame_format)
    {
        p->c_w = texdesc.Width;
        p->c_h = texdesc.Height;
        p->d3d_frame_format = d3d_frame_format;
        if (recreate_video_proc(vf) < 0)
            goto cleanup;
    }

    if (!mp_refqueue_should_deint(p->queue)) {
        d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    } else if (mp_refqueue_is_top_field(p->queue)) {
        d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
    } else {
        d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
    }

    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(p->video_ctx,
                                                          p->video_proc,
                                                          0, d3d_frame_format);

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC indesc = {
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
        .Texture2D = {
            .ArraySlice = d3d_subindex,
        },
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(p->video_dev,
                                                         (ID3D11Resource *)d3d_tex,
                                                         p->vp_enum, &indesc,
                                                         &in_view);
    if (FAILED(hr)) {
        MP_ERR(vf, "Could not create ID3D11VideoProcessorInputView\n");
        goto cleanup;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outdesc = {
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(p->video_dev,
                                                          (ID3D11Resource *)d3d_out_tex,
                                                          p->vp_enum, &outdesc,
                                                          &out_view);
    if (FAILED(hr)) {
        MP_ERR(vf, "Could not create ID3D11VideoProcessorOutputView\n");
        goto cleanup;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {
        .Enable = TRUE,
        .pInputSurface = in_view,
    };
    int frame = mp_refqueue_is_second_field(p->queue);
    hr = ID3D11VideoContext_VideoProcessorBlt(p->video_ctx, p->video_proc,
                                              out_view, frame, 1, &stream);
    if (FAILED(hr)) {
        MP_ERR(vf, "VideoProcessorBlt failed.\n");
        goto cleanup;
    }

    res = 0;
cleanup:
    if (in_view)
        ID3D11VideoProcessorInputView_Release(in_view);
    if (out_view)
        ID3D11VideoProcessorOutputView_Release(out_view);
    if (res < 0)
        TA_FREEP(&out);
    return out;
}

static void vf_d3d11vpp_process(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct mp_image *in_fmt = mp_refqueue_execute_reinit(p->queue);
    if (in_fmt) {
        av_buffer_unref(&p->hw_pool);

        destroy_video_proc(vf);

        p->params = in_fmt->params;
        p->out_params = p->params;
        p->out_params.w = (int)(p->opts->scale * p->params.w);
        p->out_params.w += p->out_params.w % 2 != 0;
        p->out_params.h = (int)(p->opts->scale * p->params.h);
        p->out_params.h += p->out_params.h % 2 != 0;
        p->out_params.crop.x0 = lrintf(p->opts->scale * p->out_params.crop.x0);
        p->out_params.crop.x1 = lrintf(p->opts->scale * p->out_params.crop.x1);
        p->out_params.crop.y0 = lrintf(p->opts->scale * p->out_params.crop.y0);
        p->out_params.crop.y1 = lrintf(p->opts->scale * p->out_params.crop.y1);

        if (p->opts->format)
            p->out_params.hw_subfmt = p->opts->format;

        p->require_filtering = p->params.hw_subfmt != p->out_params.hw_subfmt ||
                               p->params.w != p->out_params.w ||
                               p->params.h != p->out_params.h;
    }

    if (!mp_refqueue_can_output(p->queue))
        return;

    if (!mp_refqueue_should_deint(p->queue) && !p->require_filtering) {
        // no filtering
        struct mp_image *in = mp_image_new_ref(mp_refqueue_get(p->queue, 0));
        if (!in) {
            mp_filter_internal_mark_failed(vf);
            return;
        }
        mp_refqueue_write_out_pin(p->queue, in);
    } else {
        mp_refqueue_write_out_pin(p->queue, render(vf));
    }
}

static void uninit(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    destroy_video_proc(vf);

    flush_frames(vf);
    talloc_free(p->queue);
    av_buffer_unref(&p->hw_pool);
    av_buffer_unref(&p->av_device_ref);

    if (p->video_ctx)
        ID3D11VideoContext_Release(p->video_ctx);

    if (p->video_dev)
        ID3D11VideoDevice_Release(p->video_dev);

    if (p->device_ctx)
        ID3D11DeviceContext_Release(p->device_ctx);

    if (p->vo_dev)
        ID3D11Device_Release(p->vo_dev);
}

static const struct mp_filter_info vf_d3d11vpp_filter = {
    .name = "d3d11vpp",
    .process = vf_d3d11vpp_process,
    .reset = flush_frames,
    .destroy = uninit,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_d3d11vpp_create(struct mp_filter *parent,
                                            void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_d3d11vpp_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);

    // Special path for vf_d3d11_create_outconv(): disable all processing except
    // possibly surface format conversions.
    if (!p->opts) {
        static const struct opts opts = {0};
        p->opts = (struct opts *)&opts;
    }

    p->queue = mp_refqueue_alloc(f);

    struct mp_stream_info *info = mp_filter_find_stream_info(f);
    if (!info || !info->hwdec_devs)
        goto fail;

    struct hwdec_imgfmt_request params = {
        .imgfmt = IMGFMT_D3D11,
        .probing = false,
    };
    hwdec_devices_request_for_img_fmt(info->hwdec_devs, &params);

    struct mp_hwdec_ctx *hwctx =
        hwdec_devices_get_by_imgfmt_and_type(info->hwdec_devs, IMGFMT_D3D11,
                                             AV_HWDEVICE_TYPE_D3D11VA);
    if (!hwctx || !hwctx->av_device_ref)
        goto fail;
    p->av_device_ref = av_buffer_ref(hwctx->av_device_ref);
    AVHWDeviceContext *avhwctx = (void *)p->av_device_ref->data;
    AVD3D11VADeviceContext *d3dctx = avhwctx->hwctx;

    p->vo_dev = d3dctx->device;
    ID3D11Device_AddRef(p->vo_dev);

    HRESULT hr;

    hr = ID3D11Device_QueryInterface(p->vo_dev, &IID_ID3D11VideoDevice,
                                     (void **)&p->video_dev);
    if (FAILED(hr))
        goto fail;

    ID3D11Device_GetImmediateContext(p->vo_dev, &p->device_ctx);
    if (!p->device_ctx)
        goto fail;
    hr = ID3D11DeviceContext_QueryInterface(p->device_ctx, &IID_ID3D11VideoContext,
                                            (void **)&p->video_ctx);
    if (FAILED(hr))
        goto fail;

    mp_refqueue_add_in_format(p->queue, IMGFMT_D3D11, 0);

    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue,
        (p->opts->deint_enabled ? MP_MODE_DEINT : 0) |
        MP_MODE_OUTPUT_FIELDS |
        (p->opts->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0));
    mp_refqueue_set_parity(p->queue, p->opts->field_parity);

    return f;

fail:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct opts
static const m_option_t vf_opts_fields[] = {
    {"format", OPT_IMAGEFORMAT(format)},
    {"deint", OPT_BOOL(deint_enabled)},
    {"scale", OPT_FLOAT(scale)},
    {"scaling-mode", OPT_CHOICE(scaling_mode,
        {"standard", SCALING_BASIC},
        {"intel", SCALING_INTEL_VSR},
        {"nvidia", SCALING_NVIDIA_RTX})},
    {"interlaced-only", OPT_BOOL(interlaced_only)},
    {"mode", OPT_CHOICE(mode,
        {"blend", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND},
        {"bob", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB},
        {"adaptive", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE},
        {"mocomp", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION},
        {"ivctc", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE},
        {"none", 0})},
    {"parity", OPT_CHOICE(field_parity,
        {"tff", MP_FIELD_PARITY_TFF},
        {"bff", MP_FIELD_PARITY_BFF},
        {"auto", MP_FIELD_PARITY_AUTO})},
    {0}
};

const struct mp_user_filter_entry vf_d3d11vpp = {
    .desc = {
        .description = "D3D11 Video Post-Process Filter",
        .name = "d3d11vpp",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .deint_enabled = false,
            .scale = 1.0,
            .scaling_mode = SCALING_BASIC,
            .mode = 0,
            .field_parity = MP_FIELD_PARITY_AUTO,
        },
        .options = vf_opts_fields,
    },
    .create = vf_d3d11vpp_create,
};
