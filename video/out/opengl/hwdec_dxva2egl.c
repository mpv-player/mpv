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
#include <d3d9.h>

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

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    IDirect3D9Ex       *d3d9ex;
    IDirect3DDevice9Ex *device9ex;

    EGLDisplay egl_display;
    EGLConfig  egl_config;
    EGLint     alpha;
};

struct priv {
    IDirect3DDevice9Ex *device9ex; // (no own reference)
    IDirect3DQuery9    *query9;
    IDirect3DTexture9  *texture9;
    IDirect3DSurface9  *surface9;

    EGLDisplay egl_display;
    EGLSurface egl_surface;

    GLuint gl_texture;
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);

    if (p->device9ex)
        IDirect3DDevice9Ex_Release(p->device9ex);

    if (p->d3d9ex)
        IDirect3D9Ex_Release(p->d3d9ex);
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
    if (!exts ||
        !strstr(exts, "EGL_ANGLE_d3d_share_handle_client_buffer")) {
        return -1;
    }

    p->egl_display = egl_display;

    if (!d3d9_dll) {
        MP_FATAL(hw, "Failed to load \"d3d9.dll\": %s\n",
                 mp_LastError_to_str());
        goto fail;
    }

    HRESULT (WINAPI *Direct3DCreate9Ex)(UINT SDKVersion, IDirect3D9Ex **ppD3D);
    Direct3DCreate9Ex = (void *)GetProcAddress(d3d9_dll, "Direct3DCreate9Ex");
    if (!Direct3DCreate9Ex) {
        MP_FATAL(hw, "Direct3D 9Ex not supported\n");
        goto fail;
    }

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &p->d3d9ex);
    if (FAILED(hr)) {
        MP_FATAL(hw, "Couldn't create Direct3D9Ex: %s\n",
                 mp_HRESULT_to_str(hr));
        goto fail;
    }

    // We must create our own Direct3D9Ex device. ANGLE can give us the device
    // it's using, but that's probably a ID3D11Device.
    // (copied from chromium dxva_video_decode_accelerator_win.cc)
    D3DPRESENT_PARAMETERS present_params = {
        .BackBufferWidth = 1,
        .BackBufferHeight = 1,
        .BackBufferFormat = D3DFMT_UNKNOWN,
        .BackBufferCount = 1,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .hDeviceWindow = NULL,
        .Windowed = TRUE,
        .Flags = D3DPRESENTFLAG_VIDEO,
        .FullScreen_RefreshRateInHz = 0,
        .PresentationInterval = 0,
    };
    hr = IDirect3D9Ex_CreateDeviceEx(p->d3d9ex,
                                     D3DADAPTER_DEFAULT,
                                     D3DDEVTYPE_HAL,
                                     NULL,
                                     D3DCREATE_FPU_PRESERVE |
                                     D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                     D3DCREATE_DISABLE_PSGP_THREADING |
                                     D3DCREATE_MULTITHREADED,
                                     &present_params,
                                     NULL,
                                     &p->device9ex);
    if (FAILED(hr)) {
        MP_FATAL(hw, "Failed to create Direct3D9Ex device: %s\n",
                 mp_HRESULT_to_str(hr));
        goto fail;
    }

    EGLint attrs[] = {
        EGL_BUFFER_SIZE, 32,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE
    };
    EGLint count;
    if (!eglChooseConfig(p->egl_display, attrs, &p->egl_config, 1, &count) ||
        !count) {
        MP_ERR(hw, "Failed to get EGL surface configuration\n");
        goto fail;
    }

    if (!eglGetConfigAttrib(p->egl_display, p->egl_config,
                            EGL_BIND_TO_TEXTURE_RGBA, &p->alpha)) {
        MP_FATAL(hw, "Failed to query EGL surface alpha\n");
        goto fail;
    }

    struct mp_image_params dummy_params = {
        .imgfmt = IMGFMT_DXVA2,
        .w = 256,
        .h = 256,
    };
    struct ra_hwdec_mapper *mapper = ra_hwdec_mapper_create(hw, &dummy_params);
    if (!mapper)
        goto fail;
    ra_hwdec_mapper_free(&mapper);

    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .av_device_ref = d3d9_wrap_device_ref((IDirect3DDevice9 *)p->device9ex),
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

    ra_tex_free(mapper->ra, &mapper->tex[0]);
    gl->DeleteTextures(1, &p->gl_texture);

    if (p->egl_display && p->egl_surface) {
        eglReleaseTexImage(p->egl_display, p->egl_surface, EGL_BACK_BUFFER);
        eglDestroySurface(p->egl_display, p->egl_surface);
    }

    if (p->surface9)
        IDirect3DSurface9_Release(p->surface9);

    if (p->texture9)
        IDirect3DTexture9_Release(p->texture9);

    if (p->query9)
        IDirect3DQuery9_Release(p->query9);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    HRESULT hr;

    p->device9ex = p_owner->device9ex;
    p->egl_display = p_owner->egl_display;

    hr = IDirect3DDevice9_CreateQuery(p->device9ex, D3DQUERYTYPE_EVENT,
                                      &p->query9);
    if (FAILED(hr)) {
        MP_FATAL(mapper, "Failed to create Direct3D query interface: %s\n",
                 mp_HRESULT_to_str(hr));
        goto fail;
    }

    // Test the query API
    hr = IDirect3DQuery9_Issue(p->query9, D3DISSUE_END);
    if (FAILED(hr)) {
        MP_FATAL(mapper, "Failed to issue Direct3D END test query: %s\n",
                 mp_HRESULT_to_str(hr));
        goto fail;
    }

    HANDLE share_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateTexture(p->device9ex,
                                          mapper->src_params.w,
                                          mapper->src_params.h,
                                          1, D3DUSAGE_RENDERTARGET,
                                          p_owner->alpha ?
                                            D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8,
                                          D3DPOOL_DEFAULT,
                                          &p->texture9,
                                          &share_handle);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to create Direct3D9 texture: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DTexture9_GetSurfaceLevel(p->texture9, 0, &p->surface9);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to get Direct3D9 surface from texture: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    EGLint attrib_list[] = {
        EGL_WIDTH, mapper->src_params.w,
        EGL_HEIGHT, mapper->src_params.h,
        EGL_TEXTURE_FORMAT, p_owner->alpha ? EGL_TEXTURE_RGBA : EGL_TEXTURE_RGB,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };
    p->egl_surface = eglCreatePbufferFromClientBuffer(
        p->egl_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        share_handle, p_owner->egl_config, attrib_list);
    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_ERR(mapper, "Failed to create EGL surface\n");
        goto fail;
    }

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mapper->src_params.w,
        .h = mapper->src_params.h,
        .d = 1,
        .format = ra_find_unorm_format(mapper->ra, 1, p_owner->alpha ? 4 : 3),
        .render_src = true,
        .src_linear = true,
    };
    if (!params.format)
        goto fail;

    mapper->tex[0] = ra_create_wrapped_tex(mapper->ra, &params, p->gl_texture);
    if (!mapper->tex[0])
        goto fail;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;
    return 0;
fail:
    return -1;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    HRESULT hr;
    RECT rc = {0, 0, mapper->src->w, mapper->src->h};
    IDirect3DSurface9* hw_surface = (IDirect3DSurface9 *)mapper->src->planes[3];
    hr = IDirect3DDevice9Ex_StretchRect(p->device9ex,
                                        hw_surface, &rc,
                                        p->surface9, &rc,
                                        D3DTEXF_NONE);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Direct3D RGB conversion failed: %s\n",
               mp_HRESULT_to_str(hr));
        return -1;
    }

    hr = IDirect3DQuery9_Issue(p->query9, D3DISSUE_END);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to issue Direct3D END query\n");
        return -1;
    }

    // There doesn't appear to be an efficient way to do a blocking flush
    // of the above StretchRect. Timeout of 8ms is required to reliably
    // render 4k on Intel Haswell, Ivybridge and Cherry Trail Atom.
    const int max_retries = 8;
    const int64_t wait_us = 1000;
    int retries = 0;
    while (true) {
        hr = IDirect3DQuery9_GetData(p->query9, NULL, 0, D3DGETDATA_FLUSH);
        if (FAILED(hr)) {
            MP_ERR(mapper, "Failed to query Direct3D flush state\n");
            return -1;
        } else if (hr == S_FALSE) {
            if (++retries > max_retries) {
                MP_VERBOSE(mapper, "Failed to flush frame after %lld ms\n",
                           (long long)(wait_us * max_retries) / 1000);
                break;
            }
            mp_sleep_us(wait_us);
        } else {
            break;
        }
    }

    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    eglBindTexImage(p->egl_display, p->egl_surface, EGL_BACK_BUFFER);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_dxva2egl = {
    .name = "dxva2-egl",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_DXVA2, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
    },
};
