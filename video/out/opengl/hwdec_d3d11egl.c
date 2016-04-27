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
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/common.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"
#include "hwdec.h"
#include "video/d3d11va.h"
#include "video/d3d.h"
#include "video/hwdec.h"

struct priv {
    struct mp_d3d_ctx ctx;

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
    int c_w, c_h;

    GLuint gl_texture;
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

    gl->DeleteTextures(1, &p->gl_texture);
    p->gl_texture = 0;

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
    if (hw->hwctx)
        return -1;

    EGLDisplay egl_display = eglGetCurrentDisplay();
    if (!egl_display)
        return -1;

    const char *exts = eglQueryString(egl_display, EGL_EXTENSIONS);
    if (!exts || !strstr(exts, "EGL_ANGLE_d3d_share_handle_client_buffer") ||
        !strstr(exts, "EGL_EXT_device_query"))
        return -1;

    PFNEGLQUERYDISPLAYATTRIBEXTPROC p_eglQueryDisplayAttribEXT =
        (void *)eglGetProcAddress("eglQueryDisplayAttribEXT");
    PFNEGLQUERYDEVICEATTRIBEXTPROC p_eglQueryDeviceAttribEXT =
        (void *)eglGetProcAddress("eglQueryDeviceAttribEXT");
    if (!p_eglQueryDisplayAttribEXT || !p_eglQueryDeviceAttribEXT)
        return -1;

    HRESULT hr;
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    p->egl_display = egl_display;

    EGLAttrib device = 0;
    if (!p_eglQueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &device))
        goto fail;
    EGLAttrib d3d_device = 0;
    if (!p_eglQueryDeviceAttribEXT((EGLDeviceEXT)device, EGL_D3D11_DEVICE_ANGLE,
                                   &d3d_device))
    {
        MP_ERR(hw, "Could not get EGL_D3D11_DEVICE_ANGLE from ANGLE.\n");
        goto fail;
    }

    p->d3d11_device = (ID3D11Device *)d3d_device;
    if (!p->d3d11_device)
        goto fail;
    ID3D11Device_AddRef(p->d3d11_device);

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

    hw->converted_imgfmt = IMGFMT_RGB0;

    p->ctx.d3d11_device = p->d3d11_device;
    p->ctx.hwctx.type = HWDEC_D3D11VA;
    p->ctx.hwctx.d3d_ctx = &p->ctx;

    hw->hwctx = &p->ctx.hwctx;
    return 0;
fail:
    destroy(hw);
    return -1;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    HRESULT hr;

    destroy_objects(hw);

    assert(params->imgfmt == hw->driver->imgfmt);

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

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

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

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    HRESULT hr;
    ID3D11VideoProcessorInputView *in_view = NULL;

    if (!p->gl_texture)
        return -1;

    ID3D11Texture2D *d3d_tex = d3d11_texture_in_mp_image(hw_image);
    int d3d_subindex = d3d11_subindex_in_mp_image(hw_image);
    if (!d3d_tex)
        return -1;

    D3D11_TEXTURE2D_DESC texdesc;
    ID3D11Texture2D_GetDesc(d3d_tex, &texdesc);
    if (!p->video_proc || p->c_w != texdesc.Width || p->c_h != texdesc.Height) {
        p->c_w = texdesc.Width;
        p->c_h = texdesc.Height;
        if (create_video_proc(hw, &hw_image->params) < 0)
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

    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    eglBindTexImage(p->egl_display, p->egl_surface, EGL_BACK_BUFFER);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    out_textures[0] = p->gl_texture;
    return 0;
}

const struct gl_hwdec_driver gl_hwdec_d3d11egl = {
    .name = "d3d11-egl",
    .api = HWDEC_D3D11VA,
    .imgfmt = IMGFMT_D3D11VA,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
