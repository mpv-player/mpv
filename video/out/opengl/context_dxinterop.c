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
#include <versionhelpers.h>
#include <d3d9.h>
#include <dwmapi.h>
#include "osdep/windows_utils.h"
#include "video/out/w32_common.h"
#include "context.h"
#include "utils.h"

// For WGL_ACCESS_WRITE_DISCARD_NV, etc.
#include <GL/wglext.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

// mingw-w64 header typo?
#ifndef IDirect3DSwapChain9Ex_GetBackBuffer
#define IDirect3DSwapChain9Ex_GetBackBuffer IDirect3DSwapChain9EX_GetBackBuffer
#endif

struct priv {
    GL gl;

    HMODULE d3d9_dll;
    HRESULT (WINAPI *Direct3DCreate9Ex)(UINT SDKVersion, IDirect3D9Ex **ppD3D);

    // Direct3D9 device and resources
    IDirect3D9Ex *d3d9ex;
    IDirect3DDevice9Ex *device;
    HANDLE device_h;
    IDirect3DSwapChain9Ex *swapchain;
    IDirect3DSurface9 *backbuffer;
    IDirect3DSurface9 *rtarget;
    HANDLE rtarget_h;

    // OpenGL offscreen context
    HWND os_wnd;
    HDC os_dc;
    HGLRC os_ctx;

    // OpenGL resources
    GLuint texture;
    GLuint main_fb;

    // Did we lose the device?
    bool lost_device;

    // Requested and current parameters
    int requested_swapinterval;
    int width, height, swapinterval;
};

static __thread struct ra_ctx *current_ctx;

static void pump_message_loop(void)
{
    // We have a hidden window on this thread (for the OpenGL context,) so pump
    // its message loop at regular intervals to be safe
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
        DispatchMessageW(&message);
}

static void *w32gpa(const GLubyte *procName)
{
    HMODULE oglmod;
    void *res = wglGetProcAddress(procName);
    if (res)
        return res;
    oglmod = GetModuleHandleW(L"opengl32.dll");
    return GetProcAddress(oglmod, procName);
}

static int os_ctx_create(struct ra_ctx *ctx)
{
    static const wchar_t os_wnd_class[] = L"mpv offscreen gl";
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;
    HGLRC legacy_context = NULL;

    RegisterClassExW(&(WNDCLASSEXW) {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_OWNDC,
        .lpfnWndProc = DefWindowProc,
        .hInstance = HINST_THISCOMPONENT,
        .lpszClassName = os_wnd_class,
    });

    // Create a hidden window for an offscreen OpenGL context. It might also be
    // possible to use the VO window, but MSDN recommends against drawing to
    // the same window with flip mode present and other APIs, so play it safe.
    p->os_wnd = CreateWindowExW(0, os_wnd_class, os_wnd_class, 0, 0, 0, 200,
        200, NULL, NULL, HINST_THISCOMPONENT, NULL);
    p->os_dc = GetDC(p->os_wnd);
    if (!p->os_dc) {
        MP_FATAL(ctx->vo, "Couldn't create window for offscreen rendering\n");
        goto fail;
    }

    // Choose a pixel format. It probably doesn't matter what this is because
    // the primary framebuffer will not be used.
    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof pfd,
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 24,
        .iLayerType = PFD_MAIN_PLANE,
    };
    int pf = ChoosePixelFormat(p->os_dc, &pfd);
    if (!pf) {
        MP_FATAL(ctx->vo,
                 "Couldn't choose pixelformat for offscreen rendering: %s\n",
                 mp_LastError_to_str());
        goto fail;
    }
    SetPixelFormat(p->os_dc, pf, &pfd);

    legacy_context = wglCreateContext(p->os_dc);
    if (!legacy_context || !wglMakeCurrent(p->os_dc, legacy_context)) {
        MP_FATAL(ctx->vo, "Couldn't create OpenGL context for offscreen rendering: %s\n",
                 mp_LastError_to_str());
        goto fail;
    }

    const char *(GLAPIENTRY *wglGetExtensionsStringARB)(HDC hdc)
        = w32gpa((const GLubyte*)"wglGetExtensionsStringARB");
    if (!wglGetExtensionsStringARB) {
        MP_FATAL(ctx->vo, "The OpenGL driver does not support OpenGL 3.x\n");
        goto fail;
    }

    const char *wgl_exts = wglGetExtensionsStringARB(p->os_dc);
    if (!strstr(wgl_exts, "WGL_ARB_create_context")) {
        MP_FATAL(ctx->vo, "The OpenGL driver does not support OpenGL 3.x\n");
        goto fail;
    }

    HGLRC (GLAPIENTRY *wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext,
                                                   const int *attribList)
        = w32gpa((const GLubyte*)"wglCreateContextAttribsARB");
    if (!wglCreateContextAttribsARB) {
        MP_FATAL(ctx->vo, "The OpenGL driver does not support OpenGL 3.x\n");
        goto fail;
    }

    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 0,
        WGL_CONTEXT_FLAGS_ARB, 0,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    p->os_ctx = wglCreateContextAttribsARB(p->os_dc, 0, attribs);
    if (!p->os_ctx) {
        // NVidia, instead of ignoring WGL_CONTEXT_FLAGS_ARB, will error out if
        // it's present on pre-3.2 contexts.
        // Remove it from attribs and retry the context creation.
        attribs[6] = attribs[7] = 0;
        p->os_ctx = wglCreateContextAttribsARB(p->os_dc, 0, attribs);
    }
    if (!p->os_ctx) {
        MP_FATAL(ctx->vo,
                 "Couldn't create OpenGL 3.x context for offscreen rendering: %s\n",
                 mp_LastError_to_str());
        goto fail;
    }

    wglMakeCurrent(p->os_dc, NULL);
    wglDeleteContext(legacy_context);
    legacy_context = NULL;

    if (!wglMakeCurrent(p->os_dc, p->os_ctx)) {
        MP_FATAL(ctx->vo,
                 "Couldn't activate OpenGL 3.x context for offscreen rendering: %s\n",
                 mp_LastError_to_str());
        goto fail;
    }

    mpgl_load_functions(gl, w32gpa, wgl_exts, ctx->vo->log);
    if (!(gl->mpgl_caps & MPGL_CAP_DXINTEROP)) {
        MP_FATAL(ctx->vo, "WGL_NV_DX_interop is not supported\n");
        goto fail;
    }

    return 0;
fail:
    if (legacy_context) {
        wglMakeCurrent(p->os_dc, NULL);
        wglDeleteContext(legacy_context);
    }
    return -1;
}

static void os_ctx_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (p->os_ctx) {
        wglMakeCurrent(p->os_dc, NULL);
        wglDeleteContext(p->os_ctx);
    }
    if (p->os_dc)
        ReleaseDC(p->os_wnd, p->os_dc);
    if (p->os_wnd)
        DestroyWindow(p->os_wnd);
}

static int d3d_size_dependent_create(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;
    HRESULT hr;

    IDirect3DSwapChain9 *sw9;
    hr = IDirect3DDevice9Ex_GetSwapChain(p->device, 0, &sw9);
    if (FAILED(hr)) {
        MP_ERR(ctx->vo, "Couldn't get swap chain: %s\n", mp_HRESULT_to_str(hr));
        return -1;
    }

    hr = IDirect3DSwapChain9_QueryInterface(sw9, &IID_IDirect3DSwapChain9Ex,
        (void**)&p->swapchain);
    if (FAILED(hr)) {
        SAFE_RELEASE(sw9);
        MP_ERR(ctx->vo, "Obtained swap chain is not IDirect3DSwapChain9Ex: %s\n",
               mp_HRESULT_to_str(hr));
        return -1;
    }
    SAFE_RELEASE(sw9);

    hr = IDirect3DSwapChain9Ex_GetBackBuffer(p->swapchain, 0,
        D3DBACKBUFFER_TYPE_MONO, &p->backbuffer);
    if (FAILED(hr)) {
        MP_ERR(ctx->vo, "Couldn't get backbuffer: %s\n", mp_HRESULT_to_str(hr));
        return -1;
    }

    // Get the format of the backbuffer
    D3DSURFACE_DESC bb_desc = { 0 };
    IDirect3DSurface9_GetDesc(p->backbuffer, &bb_desc);

    MP_VERBOSE(ctx->vo, "DX_interop backbuffer size: %ux%u\n",
        (unsigned)bb_desc.Width, (unsigned)bb_desc.Height);
    MP_VERBOSE(ctx->vo, "DX_interop backbuffer format: %u\n",
        (unsigned)bb_desc.Format);

    // Create a rendertarget with the same format as the backbuffer for
    // rendering from OpenGL
    HANDLE share_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(p->device, bb_desc.Width,
        bb_desc.Height, bb_desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
        &p->rtarget, &share_handle);
    if (FAILED(hr)) {
        MP_ERR(ctx->vo, "Couldn't create rendertarget: %s\n", mp_HRESULT_to_str(hr));
        return -1;
    }

    // Register the share handle with WGL_NV_DX_interop. Nvidia does not
    // require the use of share handles, but Intel does.
    if (share_handle)
        gl->DXSetResourceShareHandleNV(p->rtarget, share_handle);

    // Create the OpenGL-side texture
    gl->GenTextures(1, &p->texture);

    // Now share the rendertarget with OpenGL as a texture
    p->rtarget_h = gl->DXRegisterObjectNV(p->device_h, p->rtarget, p->texture,
        GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
    if (!p->rtarget_h) {
        MP_ERR(ctx->vo, "Couldn't share rendertarget with OpenGL: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    // Lock the rendertarget for use from OpenGL. This will only be unlocked in
    // swap_buffers() when it is blitted to the real Direct3D backbuffer.
    if (!gl->DXLockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(ctx->vo, "Couldn't lock rendertarget: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    gl->BindFramebuffer(GL_FRAMEBUFFER, p->main_fb);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, p->texture, 0);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    return 0;
}

static void d3d_size_dependent_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;

    if (p->rtarget_h) {
        gl->DXUnlockObjectsNV(p->device_h, 1, &p->rtarget_h);
        gl->DXUnregisterObjectNV(p->device_h, p->rtarget_h);
    }
    p->rtarget_h = 0;
    if (p->texture)
        gl->DeleteTextures(1, &p->texture);
    p->texture = 0;

    SAFE_RELEASE(p->rtarget);
    SAFE_RELEASE(p->backbuffer);
    SAFE_RELEASE(p->swapchain);
}

static void fill_presentparams(struct ra_ctx *ctx,
                               D3DPRESENT_PARAMETERS *pparams)
{
    struct priv *p = ctx->priv;

    // Present intervals other than IMMEDIATE and ONE don't seem to work. It's
    // possible that they're not compatible with FLIPEX.
    UINT presentation_interval;
    switch (p->requested_swapinterval) {
    case 0:  presentation_interval = D3DPRESENT_INTERVAL_IMMEDIATE; break;
    case 1:  presentation_interval = D3DPRESENT_INTERVAL_ONE;       break;
    default: presentation_interval = D3DPRESENT_INTERVAL_ONE;       break;
    }

    *pparams = (D3DPRESENT_PARAMETERS) {
        .Windowed = TRUE,
        .BackBufferWidth = ctx->vo->dwidth ? ctx->vo->dwidth : 1,
        .BackBufferHeight = ctx->vo->dheight ? ctx->vo->dheight : 1,
        // Add one frame for the backbuffer and one frame of "slack" to reduce
        // contention with the window manager when acquiring the backbuffer
        .BackBufferCount = ctx->opts.swapchain_depth + 2,
        .SwapEffect = IsWindows7OrGreater() ? D3DSWAPEFFECT_FLIPEX : D3DSWAPEFFECT_FLIP,
        // Automatically get the backbuffer format from the display format
        .BackBufferFormat = D3DFMT_UNKNOWN,
        .PresentationInterval = presentation_interval,
        .hDeviceWindow = vo_w32_hwnd(ctx->vo),
    };
}

static int d3d_create(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;
    HRESULT hr;

    p->d3d9_dll = LoadLibraryW(L"d3d9.dll");
    if (!p->d3d9_dll) {
        MP_FATAL(ctx->vo, "Failed to load \"d3d9.dll\": %s\n",
                 mp_LastError_to_str());
        return -1;
    }

    // WGL_NV_dx_interop requires Direct3D 9Ex on WDDM systems. Direct3D 9Ex
    // also enables flip mode present for efficient rendering with the DWM.
    p->Direct3DCreate9Ex = (void*)GetProcAddress(p->d3d9_dll,
        "Direct3DCreate9Ex");
    if (!p->Direct3DCreate9Ex) {
        MP_FATAL(ctx->vo, "Direct3D 9Ex not supported\n");
        return -1;
    }

    hr = p->Direct3DCreate9Ex(D3D_SDK_VERSION, &p->d3d9ex);
    if (FAILED(hr)) {
        MP_FATAL(ctx->vo, "Couldn't create Direct3D9Ex: %s\n",
                 mp_HRESULT_to_str(hr));
        return -1;
    }

    D3DPRESENT_PARAMETERS pparams;
    fill_presentparams(ctx, &pparams);

    hr = IDirect3D9Ex_CreateDeviceEx(p->d3d9ex, D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL, vo_w32_hwnd(ctx->vo),
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE |
        D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED |
        D3DCREATE_NOWINDOWCHANGES,
        &pparams, NULL, &p->device);
    if (FAILED(hr)) {
        MP_FATAL(ctx->vo, "Couldn't create device: %s\n", mp_HRESULT_to_str(hr));
        return -1;
    }

    IDirect3DDevice9Ex_SetMaximumFrameLatency(p->device, ctx->opts.swapchain_depth);

    // Register the Direct3D device with WGL_NV_dx_interop
    p->device_h = gl->DXOpenDeviceNV(p->device);
    if (!p->device_h) {
        MP_FATAL(ctx->vo, "Couldn't open Direct3D device from OpenGL: %s\n",
                 mp_LastError_to_str());
        return -1;
    }

    return 0;
}

static void d3d_destroy(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;

    if (p->device_h)
        gl->DXCloseDeviceNV(p->device_h);
    SAFE_RELEASE(p->device);
    SAFE_RELEASE(p->d3d9ex);
    if (p->d3d9_dll)
        FreeLibrary(p->d3d9_dll);
}

static void dxgl_uninit(struct ra_ctx *ctx)
{
    ra_gl_ctx_uninit(ctx);
    d3d_size_dependent_destroy(ctx);
    d3d_destroy(ctx);
    os_ctx_destroy(ctx);
    vo_w32_uninit(ctx->vo);
    DwmEnableMMCSS(FALSE);
    pump_message_loop();
}

static void dxgl_reset(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    HRESULT hr;

    // Check if the device actually needs to be reset
    if (ctx->vo->dwidth == p->width && ctx->vo->dheight == p->height &&
        p->requested_swapinterval == p->swapinterval && !p->lost_device)
        return;

    d3d_size_dependent_destroy(ctx);

    D3DPRESENT_PARAMETERS pparams;
    fill_presentparams(ctx, &pparams);

    hr = IDirect3DDevice9Ex_ResetEx(p->device, &pparams, NULL);
    if (FAILED(hr)) {
        p->lost_device = true;
        MP_ERR(ctx->vo, "Couldn't reset device: %s\n", mp_HRESULT_to_str(hr));
        return;
    }

    if (d3d_size_dependent_create(ctx) < 0) {
        p->lost_device = true;
        MP_ERR(ctx->vo, "Couldn't recreate Direct3D objects after reset\n");
        return;
    }

    MP_VERBOSE(ctx->vo, "Direct3D device reset\n");
    p->width = ctx->vo->dwidth;
    p->height = ctx->vo->dheight;
    p->swapinterval = p->requested_swapinterval;
    p->lost_device = false;
}

static int GLAPIENTRY dxgl_swap_interval(int interval)
{
    if (!current_ctx)
        return 0;
    struct priv *p = current_ctx->priv;

    p->requested_swapinterval = interval;
    dxgl_reset(current_ctx);
    return 1;
}

static void dxgl_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;
    HRESULT hr;

    pump_message_loop();

    // If the device is still lost, try to reset it again
    if (p->lost_device)
        dxgl_reset(ctx);
    if (p->lost_device)
        return;

    if (!gl->DXUnlockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(ctx->vo, "Couldn't unlock rendertarget for present: %s\n",
               mp_LastError_to_str());
        return;
    }

    // Blit the OpenGL rendertarget to the backbuffer
    hr = IDirect3DDevice9Ex_StretchRect(p->device, p->rtarget, NULL,
                                        p->backbuffer, NULL, D3DTEXF_NONE);
    if (FAILED(hr)) {
        MP_ERR(ctx->vo, "Couldn't stretchrect for present: %s\n",
               mp_HRESULT_to_str(hr));
        return;
    }

    hr = IDirect3DDevice9Ex_PresentEx(p->device, NULL, NULL, NULL, NULL, 0);
    switch (hr) {
    case D3DERR_DEVICELOST:
    case D3DERR_DEVICEHUNG:
        MP_VERBOSE(ctx->vo, "Direct3D device lost! Resetting.\n");
        p->lost_device = true;
        dxgl_reset(ctx);
        return;
    default:
        if (FAILED(hr))
            MP_ERR(ctx->vo, "Failed to present: %s\n", mp_HRESULT_to_str(hr));
    }

    if (!gl->DXLockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(ctx->vo, "Couldn't lock rendertarget after present: %s\n",
               mp_LastError_to_str());
    }
}

static bool dxgl_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    GL *gl = &p->gl;

    p->requested_swapinterval = 1;

    if (!vo_w32_init(ctx->vo))
        goto fail;
    if (os_ctx_create(ctx) < 0)
        goto fail;

    // Create the shared framebuffer
    gl->GenFramebuffers(1, &p->main_fb);

    current_ctx = ctx;
    gl->SwapInterval = dxgl_swap_interval;

    if (d3d_create(ctx) < 0)
        goto fail;
    if (d3d_size_dependent_create(ctx) < 0)
        goto fail;

    static const struct ra_swapchain_fns empty_swapchain_fns = {0};
    struct ra_gl_ctx_params params = {
        .swap_buffers = dxgl_swap_buffers,
        .flipped = true,
        .external_swapchain = &empty_swapchain_fns,
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        goto fail;

    ra_add_native_resource(ctx->ra, "IDirect3DDevice9Ex", p->device);
    ra_add_native_resource(ctx->ra, "dxinterop_device_HANDLE", p->device_h);

    DwmEnableMMCSS(TRUE);
    return true;
fail:
    dxgl_uninit(ctx);
    return false;
}

static void resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    dxgl_reset(ctx);
    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, p->main_fb);
}

static bool dxgl_reconfig(struct ra_ctx *ctx)
{
    vo_w32_config(ctx->vo);
    resize(ctx);
    return true;
}

static int dxgl_control(struct ra_ctx *ctx, int *events, int request,
                             void *arg)
{
    int ret = vo_w32_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

const struct ra_ctx_fns ra_ctx_dxgl = {
    .type         = "opengl",
    .name         = "dxinterop",
    .init         = dxgl_init,
    .reconfig     = dxgl_reconfig,
    .control      = dxgl_control,
    .uninit       = dxgl_uninit,
};
