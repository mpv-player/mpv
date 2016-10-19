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

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "angle_dynamic.h"

#include "common/common.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"
#include "hwdec.h"
#include "video/hwdec.h"
#include "video/decode/d3d.h"

#ifndef EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE
#define EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE 0x3AAB
#endif

struct priv {
    struct mp_hwdec_ctx hwctx;

    ID3D11Device *d3d11_device;
    EGLDisplay egl_display;

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
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    destroy_objects(hw);

    hwdec_devices_remove(hw->devs, &p->hwctx);

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
    if (!exts || !strstr(exts, "EGL_ANGLE_d3d_share_handle_client_buffer") ||
        !strstr(exts, "EGL_ANGLE_stream_producer_d3d_texture_nv12") ||
        !(strstr(hw->gl->extensions, "GL_OES_EGL_image_external_essl3") ||
          hw->gl->es == 200) ||
        !strstr(exts, "EGL_EXT_device_query") ||
        !(hw->gl->mpgl_caps & MPGL_CAP_TEX_RG))
        return -1;

    HRESULT hr;
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    p->egl_display = egl_display;

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

    static const char *es2_exts[] = {"GL_NV_EGL_stream_consumer_external", 0};
    static const char *es3_exts[] = {"GL_NV_EGL_stream_consumer_external",
                                     "GL_OES_EGL_image_external_essl3", 0};
    hw->glsl_extensions = hw->gl->es == 200 ? es2_exts : es3_exts;

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

    if (!d3d11_check_decoding(p->d3d11_device)) {
        MP_VERBOSE(hw, "D3D11 video decoding not supported on this system.\n");
        goto fail;
    }

    ID3D10Multithread *multithread;
    hr = ID3D11Device_QueryInterface(p->d3d11_device, &IID_ID3D10Multithread,
                                     (void **)&multithread);
    if (FAILED(hr)) {
        MP_ERR(hw, "Failed to get Multithread interface: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }
    ID3D10Multithread_SetMultithreadProtected(multithread, TRUE);
    ID3D10Multithread_Release(multithread);

    p->hwctx = (struct mp_hwdec_ctx){
        .type = HWDEC_D3D11VA,
        .driver_name = hw->driver->name,
        .ctx = p->d3d11_device,
        .download_image = d3d11_download_image,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
fail:
    destroy(hw);
    return -1;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    destroy_objects(hw);

    if (params->hw_subfmt != IMGFMT_NV12) {
        MP_FATAL(hw, "Format not supported.\n");
        return -1;
    }

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
    params->hw_subfmt = 0;

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
            },
        },
    };
    return 0;
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
    .imgfmt = IMGFMT_D3D11NV12,
    .create = create,
    .reinit = reinit,
    .map_frame = map_frame,
    .unmap = unmap,
    .destroy = destroy,
};
