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

struct opts {
    int deint_enabled;
    int interlaced_only;
    int mode;
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

    DXGI_FORMAT out_format;

    bool require_filtering;

    struct mp_image_params params, out_params;
    int c_w, c_h;

    struct mp_image_pool *pool;

    struct mp_refqueue *queue;
};

static void release_tex(void *arg)
{
    ID3D11Texture2D *texture = arg;

    ID3D11Texture2D_Release(texture);
}

static struct mp_image *alloc_pool(void *pctx, int fmt, int w, int h)
{
    struct mp_filter *vf = pctx;
    struct priv *p = vf->priv;
    HRESULT hr;

    ID3D11Texture2D *texture = NULL;
    D3D11_TEXTURE2D_DESC texdesc = {
        .Width = w,
        .Height = h,
        .Format = p->out_format,
        .MipLevels = 1,
        .ArraySize = 1,
        .SampleDesc = { .Count = 1 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    };
    hr = ID3D11Device_CreateTexture2D(p->vo_dev, &texdesc, NULL, &texture);
    if (FAILED(hr))
        return NULL;

    struct mp_image *mpi = mp_image_new_custom_ref(NULL, texture, release_tex);
    if (!mpi)
        abort();

    mp_image_setfmt(mpi, IMGFMT_D3D11);
    mp_image_set_size(mpi, w, h);
    mpi->params.hw_subfmt = p->out_params.hw_subfmt;

    mpi->planes[0] = (void *)texture;
    mpi->planes[1] = (void *)(intptr_t)0;

    return mpi;
}

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

static int recreate_video_proc(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    HRESULT hr;

    destroy_video_proc(vf);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpdesc = {
        .InputFrameFormat = p->d3d_frame_format,
        .InputWidth = p->c_w,
        .InputHeight = p->c_h,
        .OutputWidth = p->params.w,
        .OutputHeight = p->params.h,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(p->video_dev, &vpdesc,
                                                          &p->vp_enum);
    if (FAILED(hr))
        goto fail;

    D3D11_VIDEO_PROCESSOR_CAPS caps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(p->vp_enum, &caps);
    if (FAILED(hr))
        goto fail;

    MP_VERBOSE(vf, "Found %d rate conversion caps. Looking for caps=0x%x.\n",
               (int)caps.RateConversionCapsCount, p->opts->mode);

    int rindex = -1;
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

    // TOOD: so, how do we select which rate conversion mode the processor uses?

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
        .YCbCr_Matrix = p->params.color.space != MP_CSP_BT_601,
        .Nominal_Range = p->params.color.levels == MP_CSP_LEVELS_TV ? 1 : 2,
    };
    ID3D11VideoContext_VideoProcessorSetStreamColorSpace(p->video_ctx,
                                                         p->video_proc,
                                                         0, &csp);
    ID3D11VideoContext_VideoProcessorSetOutputColorSpace(p->video_ctx,
                                                         p->video_proc,
                                                         &csp);

    return 0;
fail:
    destroy_video_proc(vf);
    return -1;
}

static struct mp_image *render(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    int res = -1;
    HRESULT hr;
    ID3D11VideoProcessorInputView *in_view = NULL;
    ID3D11VideoProcessorOutputView *out_view = NULL;
    struct mp_image *in = NULL, *out = NULL;
    out = mp_image_pool_get(p->pool, IMGFMT_D3D11, p->params.w, p->params.h);
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
        mp_image_pool_clear(p->pool);

        destroy_video_proc(vf);

        p->params = in_fmt->params;
        p->out_params = p->params;

        p->out_params.hw_subfmt = IMGFMT_NV12;
        p->out_format = DXGI_FORMAT_NV12;

        p->require_filtering = p->params.hw_subfmt != p->out_params.hw_subfmt;
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
    talloc_free(p->pool);

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

    hwdec_devices_request_all(info->hwdec_devs);

    struct mp_hwdec_ctx *hwctx =
        hwdec_devices_get_by_lavc(info->hwdec_devs, AV_HWDEVICE_TYPE_D3D11VA);
    if (!hwctx || !hwctx->av_device_ref)
        goto fail;
    AVHWDeviceContext *avhwctx = (void *)hwctx->av_device_ref->data;
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

    p->pool = mp_image_pool_new(f);
    mp_image_pool_set_allocator(p->pool, alloc_pool, f);
    mp_image_pool_set_lru(p->pool);

    mp_refqueue_add_in_format(p->queue, IMGFMT_D3D11, 0);

    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue,
        (p->opts->deint_enabled ? MP_MODE_DEINT : 0) |
        MP_MODE_OUTPUT_FIELDS |
        (p->opts->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0));

    return f;

fail:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct opts
static const m_option_t vf_opts_fields[] = {
    {"deint", OPT_FLAG(deint_enabled)},
    {"interlaced-only", OPT_FLAG(interlaced_only)},
    {"mode", OPT_CHOICE(mode,
        {"blend", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND},
        {"bob", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB},
        {"adaptive", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE},
        {"mocomp", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION},
        {"ivctc", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE},
        {"none", 0})},
    {0}
};

const struct mp_user_filter_entry vf_d3d11vpp = {
    .desc = {
        .description = "D3D11 Video Post-Process Filter",
        .name = "d3d11vpp",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .deint_enabled = 1,
            .interlaced_only = 0,
            .mode = D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB,
        },
        .options = vf_opts_fields,
    },
    .create = vf_d3d11vpp_create,
};
