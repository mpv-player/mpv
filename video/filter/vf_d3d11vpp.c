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

#include <initguid.h>
#include <assert.h>
#include <windows.h>
#include <d3d11.h>

#include "common/common.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"
#include "vf.h"
#include "refqueue.h"
#include "video/hwdec.h"
#include "video/mp_image_pool.h"

// missing in MinGW
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND 0x1
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB 0x2
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE 0x4
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION 0x8
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE 0x10
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_FRAME_RATE_CONVERSION 0x20

struct vf_priv_s {
    ID3D11Device *vo_dev;

    ID3D11DeviceContext *device_ctx;
    ID3D11VideoDevice *video_dev;
    ID3D11VideoContext *video_ctx;

    ID3D11VideoProcessor *video_proc;
    ID3D11VideoProcessorEnumerator *vp_enum;
    D3D11_VIDEO_FRAME_FORMAT d3d_frame_format;

    DXGI_FORMAT out_format;
    bool out_shared;
    bool out_rgb;

    bool require_filtering;

    struct mp_image_params params, out_params;
    int c_w, c_h;

    struct mp_image_pool *pool;

    struct mp_refqueue *queue;

    int deint_enabled;
    int interlaced_only;
    int mode;
};

static void release_tex(void *arg)
{
    ID3D11Texture2D *texture = arg;

    ID3D11Texture2D_Release(texture);
}

static struct mp_image *alloc_pool(void *pctx, int fmt, int w, int h)
{
    struct vf_instance *vf = pctx;
    struct vf_priv_s *p = vf->priv;
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
        .MiscFlags = p->out_shared ? D3D11_RESOURCE_MISC_SHARED : 0,
    };
    hr = ID3D11Device_CreateTexture2D(p->vo_dev, &texdesc, NULL, &texture);
    if (FAILED(hr))
        return NULL;

    struct mp_image *mpi = mp_image_new_custom_ref(NULL, texture, release_tex);
    if (!mpi)
        abort();

    mp_image_setfmt(mpi, p->out_params.imgfmt);
    mp_image_set_size(mpi, w, h);
    mpi->params.hw_subfmt = p->out_params.hw_subfmt;

    mpi->planes[1] = (void *)texture;
    mpi->planes[2] = (void *)(intptr_t)0;

    return mpi;
}

static void flush_frames(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    mp_refqueue_flush(p->queue);
}

static int filter_ext(struct vf_instance *vf, struct mp_image *in)
{
    struct vf_priv_s *p = vf->priv;

    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue,
        (p->deint_enabled ? MP_MODE_DEINT : 0) |
        MP_MODE_OUTPUT_FIELDS |
        (p->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0));

    mp_refqueue_add_input(p->queue, in);
    return 0;
}

static void destroy_video_proc(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    if (p->video_proc)
        ID3D11VideoProcessor_Release(p->video_proc);
    p->video_proc = NULL;

    if (p->vp_enum)
        ID3D11VideoProcessorEnumerator_Release(p->vp_enum);
    p->vp_enum = NULL;
}

static int recreate_video_proc(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
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
               (int)caps.RateConversionCapsCount, p->mode);

    int rindex = -1;
    for (int n = 0; n < caps.RateConversionCapsCount; n++) {
        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rcaps;
        hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps
                (p->vp_enum, n, &rcaps);
        if (FAILED(hr))
            goto fail;
        MP_VERBOSE(vf, "  - %d: 0x%08x\n", n, (unsigned)rcaps.ProcessorCaps);
        if (rcaps.ProcessorCaps & p->mode) {
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
    if (p->out_rgb) {
        if (p->params.color.space != MP_CSP_BT_601 &&
            p->params.color.space != MP_CSP_BT_709)
        {
            MP_WARN(vf, "Unsupported video colorspace (%s/%s). Consider "
                    "disabling hardware decoding, or using "
                    "--hwdec=d3d11va-copy to get correct output.\n",
                    m_opt_choice_str(mp_csp_names, p->params.color.space),
                    m_opt_choice_str(mp_csp_levels_names, p->params.color.levels));
        }
    } else {
        ID3D11VideoContext_VideoProcessorSetOutputColorSpace(p->video_ctx,
                                                             p->video_proc,
                                                             &csp);
    }

    return 0;
fail:
    destroy_video_proc(vf);
    return -1;
}

static int render(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    int res = -1;
    HRESULT hr;
    ID3D11VideoProcessorInputView *in_view = NULL;
    ID3D11VideoProcessorOutputView *out_view = NULL;
    struct mp_image *in = NULL, *out = NULL;
    out = mp_image_pool_get(p->pool, p->out_params.imgfmt, p->params.w, p->params.h);
    if (!out)
        goto cleanup;

    ID3D11Texture2D *d3d_out_tex = (void *)out->planes[1];

    in = mp_refqueue_get(p->queue, 0);
    if (!in)
        goto cleanup;
    ID3D11Texture2D *d3d_tex = (void *)in->planes[1];
    int d3d_subindex = (intptr_t)in->planes[2];

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
    if (FAILED(hr))
        goto cleanup;

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
    if (res >= 0) {
        vf_add_output_frame(vf, out);
    } else {
        talloc_free(out);
    }
    mp_refqueue_next_field(p->queue);
    return res;
}

static int filter_out(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    if (!mp_refqueue_has_output(p->queue))
        return 0;

    // no filtering
    if (!mp_refqueue_should_deint(p->queue) && !p->require_filtering) {
        struct mp_image *in = mp_image_new_ref(mp_refqueue_get(p->queue, 0));
        if (!in)
            return -1;
        mp_image_set_params(in, &p->out_params);
        vf_add_output_frame(vf, in);
        mp_refqueue_next(p->queue);
        return 0;
    }

    return render(vf);
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    struct vf_priv_s *p = vf->priv;

    flush_frames(vf);
    talloc_free(p->pool);
    p->pool = NULL;

    destroy_video_proc(vf);

    *out = *in;

    if (vf_next_query_format(vf, IMGFMT_D3D11VA) ||
        vf_next_query_format(vf, IMGFMT_D3D11NV12))
    {
        out->imgfmt = vf_next_query_format(vf, IMGFMT_D3D11VA)
                    ? IMGFMT_D3D11VA : IMGFMT_D3D11NV12;
        out->hw_subfmt = IMGFMT_NV12;
        p->out_format = DXGI_FORMAT_NV12;
        p->out_shared = false;
        p->out_rgb = false;
    } else {
        out->imgfmt = IMGFMT_D3D11RGB;
        out->hw_subfmt = IMGFMT_RGB0;
        p->out_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        p->out_shared = true;
        p->out_rgb = true;
    }

    p->require_filtering = in->hw_subfmt != out->hw_subfmt;

    p->params = *in;
    p->out_params = *out;

    p->pool = mp_image_pool_new(20);
    mp_image_pool_set_allocator(p->pool, alloc_pool, vf);
    mp_image_pool_set_lru(p->pool);

    return 0;
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    destroy_video_proc(vf);

    flush_frames(vf);
    mp_refqueue_free(p->queue);
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

static int query_format(struct vf_instance *vf, unsigned int imgfmt)
{
    if (imgfmt == IMGFMT_D3D11VA ||
        imgfmt == IMGFMT_D3D11NV12 ||
        imgfmt == IMGFMT_D3D11RGB)
    {
        return vf_next_query_format(vf, IMGFMT_D3D11VA) ||
               vf_next_query_format(vf, IMGFMT_D3D11NV12) ||
               vf_next_query_format(vf, IMGFMT_D3D11RGB);
    }
    return 0;
}

static bool test_conversion(int in, int out)
{
    return (in == IMGFMT_D3D11VA ||
            in == IMGFMT_D3D11NV12 ||
            in == IMGFMT_D3D11RGB) &&
           (out == IMGFMT_D3D11VA ||
            out == IMGFMT_D3D11NV12 ||
            out == IMGFMT_D3D11RGB);
}

static int control(struct vf_instance *vf, int request, void* data)
{
    struct vf_priv_s *p = vf->priv;
    switch (request){
    case VFCTRL_GET_DEINTERLACE:
        *(int*)data = !!p->deint_enabled;
        return true;
    case VFCTRL_SET_DEINTERLACE:
        p->deint_enabled = !!*(int*)data;
        return true;
    case VFCTRL_SEEK_RESET:
        flush_frames(vf);
        return true;
    default:
        return CONTROL_UNKNOWN;
    }
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    vf->reconfig = reconfig;
    vf->filter_ext = filter_ext;
    vf->filter_out = filter_out;
    vf->query_format = query_format;
    vf->uninit = uninit;
    vf->control = control;

    p->queue = mp_refqueue_alloc();

    p->vo_dev = hwdec_devices_load(vf->hwdec_devs, HWDEC_D3D11VA);
    if (!p->vo_dev)
        return 0;

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

    return 1;

fail:
    uninit(vf);
    return 0;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_FLAG("deint", deint_enabled, 0),
    OPT_FLAG("interlaced-only", interlaced_only, 0),
    OPT_CHOICE("mode", mode, 0,
        ({"blend", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND},
         {"bob", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB},
         {"adaptive", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE},
         {"mocomp", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION},
         {"ivctc", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE},
         {"none", 0})),
    {0}
};

const vf_info_t vf_info_d3d11vpp = {
    .description = "D3D11 Video Post-Process Filter",
    .name = "d3d11vpp",
    .test_conversion = test_conversion,
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &(const struct vf_priv_s) {
        .deint_enabled = 1,
        .interlaced_only = 1,
        .mode = D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB,
    },
    .options = vf_opts_fields,
};
