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

#include <windows.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>

#include "angle_dynamic.h"
#include "egl_helpers.h"
#include "video/out/gpu/d3d11_helpers.h"

#include "common/common.h"
#include "options/m_config.h"
#include "video/out/w32_common.h"
#include "osdep/windows_utils.h"
#include "context.h"
#include "utils.h"

#ifndef EGL_D3D_TEXTURE_ANGLE
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif
#ifndef EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif

enum {
    RENDERER_AUTO,
    RENDERER_D3D9,
    RENDERER_D3D11,
};

struct angle_opts {
    int renderer;
    int d3d11_warp;
    int d3d11_feature_level;
    int egl_windowing;
    int flip;
};

#define OPT_BASE_STRUCT struct angle_opts
const struct m_sub_options angle_conf = {
    .opts = (const struct m_option[]) {
        OPT_CHOICE("angle-renderer", renderer, 0,
                   ({"auto", RENDERER_AUTO},
                    {"d3d9", RENDERER_D3D9},
                    {"d3d11", RENDERER_D3D11})),
        OPT_CHOICE("angle-d3d11-warp", d3d11_warp, 0,
                   ({"auto", -1},
                    {"no", 0},
                    {"yes", 1})),
        OPT_CHOICE("angle-d3d11-feature-level", d3d11_feature_level, 0,
                   ({"11_0", D3D_FEATURE_LEVEL_11_0},
                    {"10_1", D3D_FEATURE_LEVEL_10_1},
                    {"10_0", D3D_FEATURE_LEVEL_10_0},
                    {"9_3", D3D_FEATURE_LEVEL_9_3})),
        OPT_CHOICE("angle-egl-windowing", egl_windowing, 0,
                   ({"auto", -1},
                    {"no", 0},
                    {"yes", 1})),
        OPT_FLAG("angle-flip", flip, 0),
        OPT_REPLACED("angle-max-frame-latency", "swapchain-depth"),
        OPT_REMOVED("angle-swapchain-length", "controlled by --swapchain-depth"),
        {0}
    },
    .defaults = &(const struct angle_opts) {
        .renderer = RENDERER_AUTO,
        .d3d11_warp = -1,
        .d3d11_feature_level = D3D_FEATURE_LEVEL_11_0,
        .egl_windowing = -1,
        .flip = 1,
    },
    .size = sizeof(struct angle_opts),
};

struct priv {
    GL gl;

    IDXGISwapChain *dxgi_swapchain;

    ID3D11Device *d3d11_device;
    ID3D11DeviceContext *d3d11_context;
    ID3D11Texture2D *d3d11_backbuffer;

    EGLConfig egl_config;
    EGLDisplay egl_display;
    EGLDeviceEXT egl_device;
    EGLContext egl_context;
    EGLSurface egl_window; // For the EGL windowing surface only
    EGLSurface egl_backbuffer; // For the DXGI swap chain based surface

    int sc_width, sc_height; // Swap chain width and height
    int swapinterval;
    bool flipped;

    struct angle_opts *opts;
};

static __thread struct ra_ctx *current_ctx;

static void update_sizes(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    p->sc_width = ctx->vo->dwidth ? ctx->vo->dwidth : 1;
    p->sc_height = ctx->vo->dheight ? ctx->vo->dheight : 1;
}

static void d3d11_backbuffer_release(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (p->egl_backbuffer) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(p->egl_display, p->egl_backbuffer);
    }
    p->egl_backbuffer = EGL_NO_SURFACE;

    SAFE_RELEASE(p->d3d11_backbuffer);
}

static bool d3d11_backbuffer_get(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;
    HRESULT hr;

    hr = IDXGISwapChain_GetBuffer(p->dxgi_swapchain, 0, &IID_ID3D11Texture2D,
        (void**)&p->d3d11_backbuffer);
    if (FAILED(hr)) {
        MP_FATAL(vo, "Couldn't get swap chain back buffer\n");
        return false;
    }

    EGLint pbuffer_attributes[] = {
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE,
    };
    p->egl_backbuffer = eglCreatePbufferFromClientBuffer(p->egl_display,
        EGL_D3D_TEXTURE_ANGLE, p->d3d11_backbuffer, p->egl_config,
        pbuffer_attributes);
    if (!p->egl_backbuffer) {
        MP_FATAL(vo, "Couldn't create EGL pbuffer\n");
        return false;
    }

    eglMakeCurrent(p->egl_display, p->egl_backbuffer, p->egl_backbuffer,
                   p->egl_context);
    return true;
}

static void d3d11_backbuffer_resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;
    HRESULT hr;

    int old_sc_width = p->sc_width;
    int old_sc_height = p->sc_height;

    update_sizes(ctx);
    // Avoid unnecessary resizing
    if (old_sc_width == p->sc_width && old_sc_height == p->sc_height)
        return;

    // All references to backbuffers must be released before ResizeBuffers
    // (including references held by ANGLE)
    d3d11_backbuffer_release(ctx);

    // The DirectX runtime may report errors related to the device like
    // DXGI_ERROR_DEVICE_REMOVED at this point
    hr = IDXGISwapChain_ResizeBuffers(p->dxgi_swapchain, 0, p->sc_width,
        p->sc_height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
        MP_FATAL(vo, "Couldn't resize swapchain: %s\n", mp_HRESULT_to_str(hr));

    if (!d3d11_backbuffer_get(ctx))
        MP_FATAL(vo, "Couldn't get back buffer after resize\n");
}

static void d3d11_device_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    PFNEGLRELEASEDEVICEANGLEPROC eglReleaseDeviceANGLE =
        (PFNEGLRELEASEDEVICEANGLEPROC)eglGetProcAddress("eglReleaseDeviceANGLE");

    if (p->egl_display)
        eglTerminate(p->egl_display);
    p->egl_display = EGL_NO_DISPLAY;

    if (p->egl_device && eglReleaseDeviceANGLE)
        eglReleaseDeviceANGLE(p->egl_device);
    p->egl_device = 0;

    SAFE_RELEASE(p->d3d11_device);
}

static bool d3d11_device_create(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;
    struct angle_opts *o = p->opts;

    struct d3d11_device_opts device_opts = {
        .allow_warp = o->d3d11_warp != 0,
        .force_warp = o->d3d11_warp == 1,
        .max_feature_level = o->d3d11_feature_level,
        .min_feature_level = D3D_FEATURE_LEVEL_9_3,
        .max_frame_latency = ctx->opts.swapchain_depth,
    };
    if (!mp_d3d11_create_present_device(vo->log, &device_opts, &p->d3d11_device))
        return false;
    ID3D11Device_GetImmediateContext(p->d3d11_device, &p->d3d11_context);

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT) {
        MP_FATAL(vo, "Missing EGL_EXT_platform_base\n");
        return false;
    }
    PFNEGLCREATEDEVICEANGLEPROC eglCreateDeviceANGLE =
        (PFNEGLCREATEDEVICEANGLEPROC)eglGetProcAddress("eglCreateDeviceANGLE");
    if (!eglCreateDeviceANGLE) {
        MP_FATAL(vo, "Missing EGL_EXT_platform_device\n");
        return false;
    }

    p->egl_device = eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE,
        p->d3d11_device, NULL);
    if (!p->egl_device) {
        MP_FATAL(vo, "Couldn't create EGL device\n");
        return false;
    }

    p->egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
        p->egl_device, NULL);
    if (!p->egl_display) {
        MP_FATAL(vo, "Couldn't get EGL display\n");
        return false;
    }

    return true;
}

static void d3d11_swapchain_surface_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    bool had_swapchain = p->dxgi_swapchain;
    SAFE_RELEASE(p->dxgi_swapchain);
    d3d11_backbuffer_release(ctx);

    // Ensure the swapchain is destroyed by flushing the D3D11 immediate
    // context. This is needed because the HWND may be reused. See:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476425.aspx
    if (had_swapchain && p->d3d11_context)
        ID3D11DeviceContext_Flush(p->d3d11_context);
}

static bool d3d11_swapchain_surface_create(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;
    struct angle_opts *o = p->opts;

    if (!p->d3d11_device)
        goto fail;

    update_sizes(ctx);
    struct d3d11_swapchain_opts swapchain_opts = {
        .window = vo_w32_hwnd(vo),
        .width = p->sc_width,
        .height = p->sc_height,
        .flip = o->flip,
        // Add one frame for the backbuffer and one frame of "slack" to reduce
        // contention with the window manager when acquiring the backbuffer
        .length = ctx->opts.swapchain_depth + 2,
        .usage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT,
    };
    if (!mp_d3d11_create_swapchain(p->d3d11_device, vo->log, &swapchain_opts,
                                   &p->dxgi_swapchain))
        goto fail;
    if (!d3d11_backbuffer_get(ctx))
        goto fail;

    p->flipped = true;
    return true;

fail:
    d3d11_swapchain_surface_destroy(ctx);
    return false;
}

static void d3d9_device_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (p->egl_display)
        eglTerminate(p->egl_display);
    p->egl_display = EGL_NO_DISPLAY;
}

static bool d3d9_device_create(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT) {
        MP_FATAL(vo, "Missing EGL_EXT_platform_base\n");
        return false;
    }

    EGLint display_attributes[] = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE,
            EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
        EGL_NONE,
    };
    p->egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
        EGL_DEFAULT_DISPLAY, display_attributes);
    if (p->egl_display == EGL_NO_DISPLAY) {
        MP_FATAL(vo, "Couldn't get display\n");
        return false;
    }

    return true;
}

static void egl_window_surface_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->egl_window) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
    }
}

static bool egl_window_surface_create(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    int window_attribs_len = 0;
    EGLint *window_attribs = NULL;

    EGLint flip_val;
    if (eglGetConfigAttrib(p->egl_display, p->egl_config,
                           EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, &flip_val))
    {
        if (flip_val == EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE) {
            MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len,
                EGL_SURFACE_ORIENTATION_ANGLE);
            MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len,
                EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);
            p->flipped = true;
            MP_VERBOSE(vo, "Rendering flipped.\n");
        }
    }

    MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len, EGL_NONE);
    p->egl_window = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                           vo_w32_hwnd(vo), window_attribs);
    talloc_free(window_attribs);
    if (!p->egl_window) {
        MP_FATAL(vo, "Could not create EGL surface!\n");
        goto fail;
    }

    eglMakeCurrent(p->egl_display, p->egl_window, p->egl_window,
                   p->egl_context);
    return true;
fail:
    egl_window_surface_destroy(ctx);
    return false;
}

static void context_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->egl_context) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroyContext(p->egl_display, p->egl_context);
    }
    p->egl_context = EGL_NO_CONTEXT;
}

static bool context_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(vo, "Couldn't initialize EGL\n");
        goto fail;
    }

    const char *exts = eglQueryString(p->egl_display, EGL_EXTENSIONS);
    if (exts)
        MP_DBG(vo, "EGL extensions: %s\n", exts);

    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context,
                              &p->egl_config))
    {
        MP_FATAL(vo, "Could not create EGL context!\n");
        goto fail;
    }

    return true;
fail:
    context_destroy(ctx);
    return false;
}

static void angle_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_gl_ctx_uninit(ctx);

    DwmEnableMMCSS(FALSE);

    // Uninit the EGL surface implementation that is being used. Note: This may
    // result in the *_destroy function being called twice since it is also
    // called when the surface create function fails. This is fine because the
    // *_destroy functions are idempotent.
    if (p->dxgi_swapchain)
        d3d11_swapchain_surface_destroy(ctx);
    else
        egl_window_surface_destroy(ctx);

    context_destroy(ctx);

    // Uninit the EGL device implementation that is being used
    if (p->d3d11_device)
        d3d11_device_destroy(ctx);
    else
        d3d9_device_destroy(ctx);

    vo_w32_uninit(ctx->vo);
}

static int GLAPIENTRY angle_swap_interval(int interval)
{
    if (!current_ctx)
        return 0;
    struct priv *p = current_ctx->priv;

    if (p->dxgi_swapchain) {
        p->swapinterval = MPCLAMP(interval, 0, 4);
        return 1;
    } else {
        return eglSwapInterval(p->egl_display, interval);
    }
}

static void d3d11_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    // Calling Present() on a flip-sequential swap chain will silently change
    // the underlying storage of the back buffer to point to the next buffer in
    // the chain. This results in the RTVs for the back buffer becoming
    // unbound. Since ANGLE doesn't know we called Present(), it will continue
    // using the unbound RTVs, so we must save and restore them ourselves.
    ID3D11RenderTargetView *rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
    ID3D11DepthStencilView *dsv = NULL;
    ID3D11DeviceContext_OMGetRenderTargets(p->d3d11_context,
        MP_ARRAY_SIZE(rtvs), rtvs, &dsv);

    HRESULT hr = IDXGISwapChain_Present(p->dxgi_swapchain, p->swapinterval, 0);
    if (FAILED(hr))
        MP_FATAL(ctx->vo, "Couldn't present: %s\n", mp_HRESULT_to_str(hr));

    // Restore the RTVs and release the objects
    ID3D11DeviceContext_OMSetRenderTargets(p->d3d11_context,
        MP_ARRAY_SIZE(rtvs), rtvs, dsv);
    for (int i = 0; i < MP_ARRAY_SIZE(rtvs); i++)
        SAFE_RELEASE(rtvs[i]);
    SAFE_RELEASE(dsv);
}

static void egl_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_window);
}

static void angle_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->dxgi_swapchain)
        d3d11_swap_buffers(ctx);
    else
        egl_swap_buffers(ctx);
}


static int angle_color_depth(struct ra_swapchain *sw)
{
    // Only 8-bit output is supported at the moment
    return 8;
}

static bool angle_submit_frame(struct ra_swapchain *sw,
                               const struct vo_frame *frame)
{
    struct priv *p = sw->ctx->priv;
    bool ret = ra_gl_ctx_submit_frame(sw, frame);
    if (p->d3d11_context) {
        // DXGI Present doesn't flush the immediate context, which can make
        // timers inaccurate, since the end queries might not be sent until the
        // next frame. Fix this by flushing the context now.
        ID3D11DeviceContext_Flush(p->d3d11_context);
    }
    return ret;
}

static bool angle_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct vo *vo = ctx->vo;
    GL *gl = &p->gl;

    p->opts = mp_get_config_group(ctx, ctx->global, &angle_conf);
    struct angle_opts *o = p->opts;

    if (!angle_load()) {
        MP_VERBOSE(vo, "Failed to load LIBEGL.DLL\n");
        goto fail;
    }

    // Create the underlying EGL device implementation
    bool context_ok = false;
    if ((!context_ok && !o->renderer) || o->renderer == RENDERER_D3D11) {
        context_ok = d3d11_device_create(ctx);
        if (context_ok) {
            context_ok = context_init(ctx);
            if (!context_ok)
                d3d11_device_destroy(ctx);
        }
    }
    if ((!context_ok && !o->renderer) || o->renderer == RENDERER_D3D9) {
        context_ok = d3d9_device_create(ctx);
        if (context_ok) {
            MP_VERBOSE(vo, "Using Direct3D 9\n");

            context_ok = context_init(ctx);
            if (!context_ok)
                d3d9_device_destroy(ctx);
        }
    }
    if (!context_ok)
        goto fail;

    if (!vo_w32_init(vo))
        goto fail;

    // Create the underlying EGL surface implementation
    bool surface_ok = false;
    if ((!surface_ok && o->egl_windowing == -1) || o->egl_windowing == 0) {
        surface_ok = d3d11_swapchain_surface_create(ctx);
    }
    if ((!surface_ok && o->egl_windowing == -1) || o->egl_windowing == 1) {
        surface_ok = egl_window_surface_create(ctx);
        if (surface_ok)
            MP_VERBOSE(vo, "Using EGL windowing\n");
    }
    if (!surface_ok)
        goto fail;

    mpegl_load_functions(gl, vo->log);

    current_ctx = ctx;
    gl->SwapInterval = angle_swap_interval;

    // Custom swapchain impl for the D3D11 swapchain-based surface
    static const struct ra_swapchain_fns dxgi_swapchain_fns = {
        .color_depth = angle_color_depth,
        .submit_frame = angle_submit_frame,
    };
    struct ra_gl_ctx_params params = {
        .swap_buffers = angle_swap_buffers,
        .flipped = p->flipped,
        .external_swapchain = p->dxgi_swapchain ? &dxgi_swapchain_fns : NULL,
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        goto fail;

    DwmEnableMMCSS(TRUE); // DWM MMCSS cargo-cult. The dxgl backend also does this.

    return true;
fail:
    angle_uninit(ctx);
    return false;
}

static void resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (p->dxgi_swapchain)
        d3d11_backbuffer_resize(ctx);
    else
        eglWaitClient(); // Should get ANGLE to resize its swapchain
    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
}

static bool angle_reconfig(struct ra_ctx *ctx)
{
    vo_w32_config(ctx->vo);
    resize(ctx);
    return true;
}

static int angle_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_w32_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

const struct ra_ctx_fns ra_ctx_angle = {
    .type           = "opengl",
    .name           = "angle",
    .init           = angle_init,
    .reconfig       = angle_reconfig,
    .control        = angle_control,
    .uninit         = angle_uninit,
};
