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
#include "video/out/gpu/hwdec.h"
#include "ra_gl.h"
#include "video/hwdec.h"
#include "video/d3d.h"

#ifndef EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE
#define EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE 0x33AB
#endif

struct priv_owner {
    struct mp_hwdec_ctx hwctx;

    ID3D11Device *d3d11_device;
    EGLDisplay egl_display;

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

struct priv {
    EGLStreamKHR egl_stream;
    GLuint gl_textures[2];
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    hwdec_devices_remove(hw->devs, &p->hwctx);

    if (p->d3d11_device)
        ID3D11Device_Release(p->d3d11_device);
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    HRESULT hr;

    if (!ra_is_gl(hw->ra))
        return -1;
    if (!angle_load())
        return -1;

    EGLDisplay egl_display = eglGetCurrentDisplay();
    if (!egl_display)
        return -1;

    if (!eglGetCurrentContext())
        return -1;

    GL *gl = ra_gl_get(hw->ra);

    const char *exts = eglQueryString(egl_display, EGL_EXTENSIONS);
    if (!exts || !strstr(exts, "EGL_ANGLE_d3d_share_handle_client_buffer") ||
        !strstr(exts, "EGL_ANGLE_stream_producer_d3d_texture_nv12") ||
        !(strstr(gl->extensions, "GL_OES_EGL_image_external_essl3") ||
          gl->es == 200) ||
        !strstr(exts, "EGL_EXT_device_query") ||
        !(gl->mpgl_caps & MPGL_CAP_TEX_RG))
        return -1;

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
    hw->glsl_extensions = gl->es == 200 ? es2_exts : es3_exts;

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

    static const int subfmts[] = {IMGFMT_NV12, 0};
    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .av_device_ref = d3d11_wrap_device_ref(p->d3d11_device),
        .supported_formats = subfmts,
        .hw_imgfmt = IMGFMT_D3D11,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
fail:
    return -1;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    if (p->egl_stream)
        o->DestroyStreamKHR(o->egl_display, p->egl_stream);
    p->egl_stream = 0;

    gl->DeleteTextures(2, p->gl_textures);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    if (mapper->src_params.hw_subfmt != IMGFMT_NV12) {
        MP_FATAL(mapper, "Format not supported.\n");
        return -1;
    }

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    // The texture units need to be bound during init only, and are free for
    // use again after the initialization here is done.
    int texunits = 0; // [texunits, texunits + num_planes)
    int num_planes = 2;
    int gl_target = GL_TEXTURE_EXTERNAL_OES;

    p->egl_stream = o->CreateStreamKHR(o->egl_display, (EGLint[]){EGL_NONE});
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

    if (!o->StreamConsumerGLTextureExternalAttribsNV(o->egl_display, p->egl_stream,
                                                     attrs))
        goto fail;

    if (!o->CreateStreamProducerD3DTextureNV12ANGLE(o->egl_display, p->egl_stream,
                                                    (EGLAttrib[]){EGL_NONE}))
        goto fail;

    for (int n = 0; n < num_planes; n++) {
        gl->ActiveTexture(GL_TEXTURE0 + texunits + n);
        gl->BindTexture(gl_target, 0);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    return 0;
fail:
    gl->ActiveTexture(GL_TEXTURE0);
    MP_ERR(mapper, "Failed to create EGLStream\n");
    return -1;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;

    ID3D11Texture2D *d3d_tex = (void *)mapper->src->planes[0];
    int d3d_subindex = (intptr_t)mapper->src->planes[1];
    if (!d3d_tex)
        return -1;

    EGLAttrib attrs[] = {
        EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, d3d_subindex,
        EGL_NONE,
    };
    if (!o->StreamPostD3DTextureNV12ANGLE(o->egl_display, p->egl_stream,
                                          (void *)d3d_tex, attrs))
    {
        // ANGLE changed the enum ID of this without warning at one point.
        attrs[0] = attrs[0] == 0x33AB ? 0x3AAB : 0x33AB;
        if (!o->StreamPostD3DTextureNV12ANGLE(o->egl_display, p->egl_stream,
                                              (void *)d3d_tex, attrs))
            return -1;
    }

    if (!o->StreamConsumerAcquireKHR(o->egl_display, p->egl_stream))
        return -1;

    D3D11_TEXTURE2D_DESC texdesc;
    ID3D11Texture2D_GetDesc(d3d_tex, &texdesc);

    for (int n = 0; n < 2; n++) {
        struct ra_tex_params params = {
            .dimensions = 2,
            .w = texdesc.Width / (n ? 2 : 1),
            .h = texdesc.Height / (n ? 2 : 1),
            .d = 1,
            .format = ra_find_unorm_format(mapper->ra, 1, n ? 2 : 1),
            .render_src = true,
            .src_linear = true,
            .external_oes = true,
        };
        if (!params.format)
            return -1;

        mapper->tex[n] = ra_create_wrapped_tex(mapper->ra, &params,
                                               p->gl_textures[n]);
        if (!mapper->tex[n])
            return -1;
    }

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;

    for (int n = 0; n < 2; n++)
        ra_tex_free(mapper->ra, &mapper->tex[n]);
    if (p->egl_stream)
        o->StreamConsumerReleaseKHR(o->egl_display, p->egl_stream);
}

const struct ra_hwdec_driver ra_hwdec_d3d11egl = {
    .name = "d3d11-egl",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_D3D11, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
