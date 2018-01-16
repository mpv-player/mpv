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
#define EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE 0x3AAB
#endif

struct priv_owner {
    struct mp_hwdec_ctx hwctx;

    ID3D11Device *d3d11_device;

    EGLDisplay egl_display;
    EGLConfig  egl_config;
};

struct priv {
    EGLSurface egl_surface;
    GLuint gl_texture;
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

    d3d_load_dlls();

    EGLDisplay egl_display = eglGetCurrentDisplay();
    if (!egl_display)
        return -1;

    if (!eglGetCurrentContext())
        return -1;

    const char *exts = eglQueryString(egl_display, EGL_EXTENSIONS);
    if (!exts || !strstr(exts, "EGL_ANGLE_d3d_share_handle_client_buffer"))
        return -1;

    p->egl_display = egl_display;

    if (!d3d11_D3D11CreateDevice) {
        if (!hw->probing)
            MP_ERR(hw, "Failed to load D3D11 library\n");
        goto fail;
    }

    hr = d3d11_D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                 D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
                                 D3D11_SDK_VERSION, &p->d3d11_device, NULL, NULL);
    if (FAILED(hr)) {
        int lev = hw->probing ? MSGL_V : MSGL_ERR;
        mp_msg(hw->log, lev, "Failed to create D3D11 Device: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
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

    if (!d3d11_check_decoding(p->d3d11_device)) {
        MP_VERBOSE(hw, "D3D11 video decoding not supported on this system.\n");
        goto fail;
    }

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

    static const int subfmts[] = {IMGFMT_RGB0, 0};
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
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    gl->DeleteTextures(1, &p->gl_texture);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    if (mapper->src_params.hw_subfmt != IMGFMT_RGB0) {
        MP_FATAL(mapper, "Format not supported.\n");
        return -1;
    }

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;
    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;
    ra_tex_free(mapper->ra, &mapper->tex[0]);
    if (p->egl_surface) {
        eglReleaseTexImage(o->egl_display, p->egl_surface, EGL_BACK_BUFFER);
        eglDestroySurface(o->egl_display, p->egl_surface);
    }
    p->egl_surface = NULL;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    HRESULT hr;

    if (!p->gl_texture)
        return -1;

    ID3D11Texture2D *d3d_tex = (void *)mapper->src->planes[0];
    if (!d3d_tex)
        return -1;

    IDXGIResource *res;
    hr = IUnknown_QueryInterface(d3d_tex, &IID_IDXGIResource, (void **)&res);
    if (FAILED(hr))
        return -1;

    HANDLE share_handle = NULL;
    hr = IDXGIResource_GetSharedHandle(res, &share_handle);
    if (FAILED(hr))
        share_handle = NULL;

    IDXGIResource_Release(res);

    if (!share_handle)
        return -1;

    D3D11_TEXTURE2D_DESC texdesc;
    ID3D11Texture2D_GetDesc(d3d_tex, &texdesc);

    EGLint attrib_list[] = {
        EGL_WIDTH, texdesc.Width,
        EGL_HEIGHT, texdesc.Height,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };
    p->egl_surface = eglCreatePbufferFromClientBuffer(
        o->egl_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        share_handle, o->egl_config, attrib_list);
    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_ERR(mapper, "Failed to create EGL surface\n");
        return -1;
    }

    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    eglBindTexImage(o->egl_display, p->egl_surface, EGL_BACK_BUFFER);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mapper->src_params.w,
        .h = mapper->src_params.h,
        .d = 1,
        .format = ra_find_unorm_format(mapper->ra, 1, 4),
        .render_src = true,
        .src_linear = true,
    };
    if (!params.format)
        return -1;

    mapper->tex[0] = ra_create_wrapped_tex(mapper->ra, &params, p->gl_texture);
    if (!mapper->tex[0])
        return -1;

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_d3d11eglrgb = {
    .name = "d3d11-egl-rgb",
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
