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

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "angle_dynamic.h"

#include "common/common.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"
#include "hwdec.h"
#include "video/hwdec.h"

#ifndef EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE
#define EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE 0x3AAB
#endif

struct priv {
    struct mp_hwdec_ctx hwctx;

    ID3D11Device *d3d11_device;
    ID3D11VideoDevice *video_dev;
    ID3D11VideoContext *video_ctx;

    EGLDisplay egl_display;
    EGLConfig  egl_config;
    EGLSurface egl_surface;

    ID3D11Texture2D *texture;
    ID3D11VideoProcessor *video_proc;
    ID3D11VideoProcessorEnumerator *vp_enum;
    ID3D11VideoProcessorOutputView *out_view;

    struct mp_image_params image_params;
    int c_w, c_h;

    EGLStreamKHR egl_stream;

    GLuint gl_textures[3];

    // EGL_KHR_stream
    EGLStreamKHR (EGLAPIENTRY *CreateStreamKHR)(EGLDisplay dpy,
                                                const EGLint *attrib_list);
    EGLBoolean (EGLAPIENTRY *DestroyStreamKHR)(EGLDisplay dpy,
                                               EGLStreamKHR stream);

    // EGL_KHR_stream_consumer_gltexture
    EGLBoolean (EGLAPIENTRY *StreamConsumerAcquireKHR)
                                        (EGLDisplay dpy, EGLStreamKHR stream);
    EGLBoolean (EGLAPIENTRY *StreamConsumerReleaseKHR)
                                        (EGLDisplay dpy, EGLStreamKHR stream);

    // EGL_NV_stream_consumer_gltexture_yuv
    EGLBoolean (EGLAPIENTRY *StreamConsumerGLTextureExternalAttribsNV)
                (EGLDisplay dpy, EGLStreamKHR stream, EGLAttrib *attrib_list);

    // EGL_ANGLE_stream_producer_d3d_texture_nv12
    EGLBoolean (EGLAPIENTRY *CreateStreamProducerD3DTextureNV12ANGLE)
            (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
    EGLBoolean (EGLAPIENTRY *StreamPostD3DTextureNV12ANGLE)
            (EGLDisplay dpy, EGLStreamKHR stream, void *texture,
             const EGLAttrib *attrib_list);
};

static void destroy_video_proc(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    if (p->out_view)
        ID3D11VideoProcessorOutputView_Release(p->out_view);
    p->out_view = NULL;

    if (p->video_proc)
        ID3D11VideoProcessor_Release(p->video_proc);
    p->video_proc = NULL;

    if (p->vp_enum)
        ID3D11VideoProcessorEnumerator_Release(p->vp_enum);
    p->vp_enum = NULL;
}

static void destroy_objects(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    if (p->egl_stream)
        p->DestroyStreamKHR(p->egl_display, p->egl_stream);
    p->egl_stream = 0;

    for (int n = 0; n < 3; n++) {
        gl->DeleteTextures(1, &p->gl_textures[n]);
        p->gl_textures[n] = 0;
    }

    if (p->egl_display && p->egl_surface) {
        eglReleaseTexImage(p->egl_display, p->egl_surface, EGL_BACK_BUFFER);
        eglDestroySurface(p->egl_display, p->egl_surface);
    }
    p->egl_surface = NULL;

    if (p->texture)
        ID3D11Texture2D_Release(p->texture);
    p->texture = NULL;

    destroy_video_proc(hw);
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    destroy_objects(hw);

    hwdec_devices_remove(hw->devs, &p->hwctx);

    if (p->video_ctx)
        ID3D11VideoContext_Release(p->video_ctx);
    p->video_ctx = NULL;

    if (p->video_dev)
        ID3D11VideoDevice_Release(p->video_dev);
    p->video_dev = NULL;

    if (p->d3d11_device)
        ID3D11Device_Release(p->d3d11_device);
    p->d3d11_device = NULL;
}

static int create(struct gl_hwdec *hw)
{
    if (!angle_load())
        return -1;

    EGLDisplay egl_display = eglGetCurrentDisplay();
    if (!egl_display)
        return -1;

    if (!eglGetCurrentContext())
        return -1;

    const char *exts = eglQueryString(egl_display, EGL_EXTENSIONS);
    if (!exts || !strstr(exts, "EGL_ANGLE_d3d_share_handle_client_buffer"))
        return -1;

    bool use_native_device = !!strstr(exts, "EGL_EXT_device_query");

    HRESULT hr;
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    p->egl_display = egl_display;

    // Optional EGLStream stuff for working without video processor.
    // Note that as long as GL_OES_EGL_image_external_essl3 is not available,
    // this won't work in ES 3.x mode due to missing GLSL mechanisms.
    if (strstr(exts, "EGL_ANGLE_stream_producer_d3d_texture_nv12") &&
        use_native_device && hw->gl->es == 200)
    {
        MP_VERBOSE(hw, "Loading EGL_ANGLE_stream_producer_d3d_texture_nv12\n");

        p->CreateStreamKHR = (void *)eglGetProcAddress("eglCreateStreamKHR");
        p->DestroyStreamKHR = (void *)eglGetProcAddress("eglDestroyStreamKHR");
        p->StreamConsumerAcquireKHR =
            (void *)eglGetProcAddress("eglStreamConsumerAcquireKHR");
        p->StreamConsumerReleaseKHR =
            (void *)eglGetProcAddress("eglStreamConsumerReleaseKHR");
        p->StreamConsumerGLTextureExternalAttribsNV =
            (void *)eglGetProcAddress("eglStreamConsumerGLTextureExternalAttribsNV");
        p->CreateStreamProducerD3DTextureNV12ANGLE =
            (void *)eglGetProcAddress("eglCreateStreamProducerD3DTextureNV12ANGLE");
        p->StreamPostD3DTextureNV12ANGLE =
            (void *)eglGetProcAddress("eglStreamPostD3DTextureNV12ANGLE");

        if (!p->CreateStreamKHR || !p->DestroyStreamKHR ||
            !p->StreamConsumerAcquireKHR || !p->StreamConsumerReleaseKHR ||
            !p->StreamConsumerGLTextureExternalAttribsNV ||
            !p->CreateStreamProducerD3DTextureNV12ANGLE ||
            !p->StreamPostD3DTextureNV12ANGLE)
        {
            MP_ERR(hw, "Failed to load some EGLStream functions.\n");
            goto fail;
        }
    }

    if (use_native_device) {
        PFNEGLQUERYDISPLAYATTRIBEXTPROC p_eglQueryDisplayAttribEXT =
            (void *)eglGetProcAddress("eglQueryDisplayAttribEXT");
        PFNEGLQUERYDEVICEATTRIBEXTPROC p_eglQueryDeviceAttribEXT =
            (void *)eglGetProcAddress("eglQueryDeviceAttribEXT");
        if (!p_eglQueryDisplayAttribEXT || !p_eglQueryDeviceAttribEXT)
            goto fail;

        EGLAttrib device = 0;
        if (!p_eglQueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &device))
            goto fail;
        EGLAttrib d3d_device = 0;
        if (!p_eglQueryDeviceAttribEXT((EGLDeviceEXT)device,
                                       EGL_D3D11_DEVICE_ANGLE, &d3d_device))
        {
            MP_ERR(hw, "Could not get EGL_D3D11_DEVICE_ANGLE from ANGLE.\n");
            goto fail;
        }

        p->d3d11_device = (ID3D11Device *)d3d_device;
        if (!p->d3d11_device)
            goto fail;
        ID3D11Device_AddRef(p->d3d11_device);
    } else {
        HANDLE d3d11_dll = GetModuleHandleW(L"d3d11.dll");
        if (!d3d11_dll) {
            MP_ERR(hw, "Failed to load D3D11 library\n");
            goto fail;
        }

        PFN_D3D11_CREATE_DEVICE CreateDevice =
            (void *)GetProcAddress(d3d11_dll, "D3D11CreateDevice");
        if (!CreateDevice)
            goto fail;

        hr = CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                          D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
                          D3D11_SDK_VERSION, &p->d3d11_device, NULL, NULL);
        if (FAILED(hr)) {
            MP_ERR(hw, "Failed to create D3D11 Device: %s\n",
                mp_HRESULT_to_str(hr));
            goto fail;
        }
    }

    ID3D10Multithread *multithread;
    hr = ID3D11Device_QueryInterface(p->d3d11_device, &IID_ID3D10Multithread,
                                     (void **)&multithread);
    if (FAILED(hr)) {
        ID3D10Multithread_Release(multithread);
        MP_ERR(hw, "Failed to get Multithread interface: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }
    ID3D10Multithread_SetMultithreadProtected(multithread, TRUE);
    ID3D10Multithread_Release(multithread);

    hr = ID3D11Device_QueryInterface(p->d3d11_device, &IID_ID3D11VideoDevice,
                                     (void **)&p->video_dev);
    if (FAILED(hr))
        goto fail;

    ID3D11DeviceContext *device_ctx;
    ID3D11Device_GetImmediateContext(p->d3d11_device, &device_ctx);
    if (!device_ctx)
        goto fail;
    hr = ID3D11DeviceContext_QueryInterface(device_ctx, &IID_ID3D11VideoContext,
                                            (void **)&p->video_ctx);
    ID3D11DeviceContext_Release(device_ctx);
    if (FAILED(hr))
        goto fail;

    EGLint attrs[] = {
        EGL_BUFFER_SIZE, 32,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_ALPHA_SIZE, 8,
        EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
        EGL_NONE
    };
    EGLint count;
    if (!eglChooseConfig(p->egl_display, attrs, &p->egl_config, 1, &count) ||
        !count) {
        MP_ERR(hw, "Failed to get EGL surface configuration\n");
        goto fail;
    }

    p->hwctx = (struct mp_hwdec_ctx){
        .type = HWDEC_D3D11VA,
        .driver_name = hw->driver->name,
        .ctx = p->d3d11_device,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
fail:
    destroy(hw);
    return -1;
}

static int create_egl_stream(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    if (params->hw_subfmt != IMGFMT_NV12)
        return -1;

    if (!p->CreateStreamKHR)
        return -1; // extensions not available

    MP_VERBOSE(hw, "Using EGL_KHR_stream path.\n");

    // Hope that the given texture unit range is not "in use" by anything.
    // The texture units need to be bound during init only, and are free for
    // use again after the initialization here is done.
    int texunits = 0; // [texunits, texunits + num_planes)
    int num_planes = 2;
    int gl_target = GL_TEXTURE_EXTERNAL_OES;

    p->egl_stream = p->CreateStreamKHR(p->egl_display, (EGLint[]){EGL_NONE});
    if (!p->egl_stream)
        goto fail;

    for (int n = 0; n < num_planes; n++) {
        gl->ActiveTexture(GL_TEXTURE0 + texunits + n);
        gl->GenTextures(1, &p->gl_textures[n]);
        gl->BindTexture(gl_target, p->gl_textures[n]);
        gl->TexParameteri(gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(gl_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(gl_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(gl_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    EGLAttrib attrs[] = {
        EGL_COLOR_BUFFER_TYPE,          EGL_YUV_BUFFER_EXT,
        EGL_YUV_NUMBER_OF_PLANES_EXT,   num_planes,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV, texunits + 0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV, texunits + 1,
        EGL_NONE,
    };

    if (!p->StreamConsumerGLTextureExternalAttribsNV(p->egl_display, p->egl_stream,
                                                     attrs))
        goto fail;

    if (!p->CreateStreamProducerD3DTextureNV12ANGLE(p->egl_display, p->egl_stream,
                                                    (EGLAttrib[]){EGL_NONE}))
        goto fail;

    params->imgfmt = params->hw_subfmt;

    for (int n = 0; n < num_planes; n++) {
        gl->ActiveTexture(GL_TEXTURE0 + texunits + n);
        gl->BindTexture(gl_target, 0);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    return 0;
fail:
    MP_ERR(hw, "Failed to create EGLStream\n");
    if (p->egl_stream)
        p->DestroyStreamKHR(p->egl_display, p->egl_stream);
    p->egl_stream = 0;
    gl->ActiveTexture(GL_TEXTURE0);
    return -1;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    HRESULT hr;

    destroy_objects(hw);

    p->image_params = *params;

    // If this does not work, use the video process instead.
    if (create_egl_stream(hw, params) >= 0)
        return 0;

    MP_VERBOSE(hw, "Using ID3D11VideoProcessor path.\n");

    D3D11_TEXTURE2D_DESC texdesc = {
        .Width = params->w,
        .Height = params->h,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .MipLevels = 1,
        .ArraySize = 1,
        .SampleDesc = { .Count = 1 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    hr = ID3D11Device_CreateTexture2D(p->d3d11_device, &texdesc, NULL, &p->texture);
    if (FAILED(hr)) {
        MP_ERR(hw, "Failed to create texture: %s\n", mp_HRESULT_to_str(hr));
        goto fail;
    }

    HANDLE share_handle = NULL;
    IDXGIResource *res;

    hr = IUnknown_QueryInterface(p->texture, &IID_IDXGIResource, (void **)&res);
    if (FAILED(hr))
        goto fail;

    hr = IDXGIResource_GetSharedHandle(res, &share_handle);
    if (FAILED(hr))
        share_handle = NULL;

    IDXGIResource_Release(res);

    if (!share_handle)
        goto fail;

    EGLint attrib_list[] = {
        EGL_WIDTH, params->w,
        EGL_HEIGHT, params->h,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };
    p->egl_surface = eglCreatePbufferFromClientBuffer(
        p->egl_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        share_handle, p->egl_config, attrib_list);
    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_ERR(hw, "Failed to create EGL surface\n");
        goto fail;
    }

    gl->GenTextures(1, &p->gl_textures[0]);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[0]);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    eglBindTexImage(p->egl_display, p->egl_surface, EGL_BACK_BUFFER);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    params->imgfmt = IMGFMT_RGB0;
    return 0;
fail:
    destroy_objects(hw);
    return -1;
}

static int create_video_proc(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    HRESULT hr;

    destroy_video_proc(hw);

    // Note: we skip any deinterlacing considerations for now.
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpdesc = {
        .InputWidth = p->c_w,
        .InputHeight = p->c_h,
        .OutputWidth = params->w,
        .OutputHeight = params->h,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(p->video_dev, &vpdesc,
                                                          &p->vp_enum);
    if (FAILED(hr))
        goto fail;

    // Assume RateConversionIndex==0 always works fine for us.
    hr = ID3D11VideoDevice_CreateVideoProcessor(p->video_dev, p->vp_enum, 0,
                                                &p->video_proc);
    if (FAILED(hr)) {
        MP_ERR(hw, "Failed to create D3D11 video processor.\n");
        goto fail;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outdesc = {
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(p->video_dev,
                                                          (ID3D11Resource *)p->texture,
                                                          p->vp_enum, &outdesc,
                                                          &p->out_view);
    if (FAILED(hr))
        goto fail;

    // Note: libavcodec does not support cropping left/top with hwaccel.
    RECT src_rc = {
        .right = params->w,
        .bottom = params->h,
    };
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(p->video_ctx,
                                                         p->video_proc,
                                                         0, TRUE, &src_rc);

    // This is supposed to stop drivers from fucking up the video quality.
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(p->video_ctx,
                                                                 p->video_proc,
                                                                 0, FALSE);

    if ((params->colorspace != MP_CSP_BT_601 &&
         params->colorspace != MP_CSP_BT_709) ||
        params->colorlevels != MP_CSP_LEVELS_TV)
    {
        MP_WARN(hw, "Unsupported video colorspace (%s/%s). Consider disabling "
                "hardware decoding, or using --hwdec=d3d11va-copy to get "
                "correct output.\n",
                m_opt_choice_str(mp_csp_names, params->colorspace),
                m_opt_choice_str(mp_csp_levels_names, params->colorlevels));
    }

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE csp = {
        .YCbCr_Matrix = params->colorspace != MP_CSP_BT_601,
    };
    ID3D11VideoContext_VideoProcessorSetStreamColorSpace(p->video_ctx,
                                                         p->video_proc,
                                                         0, &csp);

    return 0;
fail:
    destroy_video_proc(hw);
    return -1;
}

static int map_frame_video_proc(struct gl_hwdec *hw, ID3D11Texture2D *d3d_tex,
                                int d3d_subindex, struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;
    HRESULT hr;
    ID3D11VideoProcessorInputView *in_view = NULL;

    D3D11_TEXTURE2D_DESC texdesc;
    ID3D11Texture2D_GetDesc(d3d_tex, &texdesc);
    if (!p->video_proc || p->c_w != texdesc.Width || p->c_h != texdesc.Height) {
        p->c_w = texdesc.Width;
        p->c_h = texdesc.Height;
        if (create_video_proc(hw, &p->image_params) < 0)
            return -1;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC indesc = {
        .FourCC = 0, // huh?
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
        MP_ERR(hw, "Could not create ID3D11VideoProcessorInputView\n");
        return -1;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {
        .Enable = TRUE,
        .pInputSurface = in_view,
    };
    hr = ID3D11VideoContext_VideoProcessorBlt(p->video_ctx, p->video_proc,
                                              p->out_view, 0, 1, &stream);
    ID3D11VideoProcessorInputView_Release(in_view);
    if (FAILED(hr)) {
        MP_ERR(hw, "VideoProcessorBlt failed.\n");
        return -1;
    }

    *out_frame = (struct gl_hwdec_frame){
        .planes = {
            {
                .gl_texture = p->gl_textures[0],
                .gl_target = GL_TEXTURE_2D,
                .tex_w = p->image_params.w,
                .tex_h = p->image_params.h,
            },
        },
    };
    return 0;
}

static int map_frame_egl_stream(struct gl_hwdec *hw, ID3D11Texture2D *d3d_tex,
                                int d3d_subindex, struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;

    EGLAttrib attrs[] = {
        EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, d3d_subindex,
        EGL_NONE,
    };
    if (!p->StreamPostD3DTextureNV12ANGLE(p->egl_display, p->egl_stream,
                                          (void *)d3d_tex, attrs))
        return -1;

    if (!p->StreamConsumerAcquireKHR(p->egl_display, p->egl_stream))
        return -1;

    D3D11_TEXTURE2D_DESC texdesc;
    ID3D11Texture2D_GetDesc(d3d_tex, &texdesc);

    *out_frame = (struct gl_hwdec_frame){
        .planes = {
            {
                .gl_texture = p->gl_textures[0],
                .gl_target = GL_TEXTURE_EXTERNAL_OES,
                .tex_w = texdesc.Width,
                .tex_h = texdesc.Height,
            },
            {
                .gl_texture = p->gl_textures[1],
                .gl_target = GL_TEXTURE_EXTERNAL_OES,
                .tex_w = texdesc.Width / 2,
                .tex_h = texdesc.Height / 2,
                .swizzle = "rgba", // even in ES2 mode (no LUMINANCE_ALPHA)
            },
        },
    };
    return 0;
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;

    if (!p->gl_textures[0])
        return -1;

    ID3D11Texture2D *d3d_tex = (void *)hw_image->planes[1];
    int d3d_subindex = (intptr_t)hw_image->planes[2];
    if (!d3d_tex)
        return -1;

    return p->egl_stream
           ? map_frame_egl_stream(hw, d3d_tex, d3d_subindex, out_frame)
           : map_frame_video_proc(hw, d3d_tex, d3d_subindex, out_frame);
}

static void unmap(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    if (p->egl_stream)
        p->StreamConsumerReleaseKHR(p->egl_display, p->egl_stream);
}

const struct gl_hwdec_driver gl_hwdec_d3d11egl = {
    .name = "d3d11-egl",
    .api = HWDEC_D3D11VA,
    .imgfmt = IMGFMT_D3D11VA,
    .create = create,
    .reinit = reinit,
    .map_frame = map_frame,
    .unmap = unmap,
    .destroy = destroy,
};
