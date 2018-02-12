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
#include <d3d11.h>
#include <dxgi1_2.h>
#include <pthread.h>

#include "common/common.h"
#include "common/msg.h"
#include "osdep/io.h"
#include "osdep/windows_utils.h"

#include "d3d11_helpers.h"

// Windows 8 enum value, not present in mingw-w64 headers
#define DXGI_ADAPTER_FLAG_SOFTWARE (2)

static pthread_once_t d3d11_once = PTHREAD_ONCE_INIT;
static PFN_D3D11_CREATE_DEVICE pD3D11CreateDevice = NULL;
static void d3d11_load(void)
{
    HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
    if (!d3d11)
        return;
    pD3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)
        GetProcAddress(d3d11, "D3D11CreateDevice");
}

// Get a const array of D3D_FEATURE_LEVELs from max_fl to min_fl (inclusive)
static int get_feature_levels(int max_fl, int min_fl,
                              const D3D_FEATURE_LEVEL **out)
{
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };
    static const int levels_len = MP_ARRAY_SIZE(levels);

    int start = 0;
    for (; start < levels_len; start++) {
        if (levels[start] <= max_fl)
            break;
    }
    int len = 0;
    for (; start + len < levels_len; len++) {
        if (levels[start + len] < min_fl)
            break;
    }
    *out = &levels[start];
    return len;
}

static HRESULT create_device(struct mp_log *log, bool warp, bool debug,
                             int max_fl, int min_fl, ID3D11Device **dev)
{
    const D3D_FEATURE_LEVEL *levels;
    int levels_len = get_feature_levels(max_fl, min_fl, &levels);
    if (!levels_len) {
        mp_fatal(log, "No suitable Direct3D feature level found\n");
        return E_FAIL;
    }

    D3D_DRIVER_TYPE type = warp ? D3D_DRIVER_TYPE_WARP
                                : D3D_DRIVER_TYPE_HARDWARE;
    UINT flags = debug ? D3D11_CREATE_DEVICE_DEBUG : 0;
    return pD3D11CreateDevice(NULL, type, NULL, flags, levels, levels_len,
        D3D11_SDK_VERSION, dev, NULL, NULL);
}

// Create a Direct3D 11 device for rendering and presentation. This is meant to
// reduce boilerplate in backends that D3D11, while also making sure they share
// the same device creation logic and log the same information.
bool mp_d3d11_create_present_device(struct mp_log *log,
                                    struct d3d11_device_opts *opts,
                                    ID3D11Device **dev_out)
{
    bool warp = opts->force_warp;
    int max_fl = opts->max_feature_level;
    int min_fl = opts->min_feature_level;
    ID3D11Device *dev = NULL;
    IDXGIDevice1 *dxgi_dev = NULL;
    IDXGIAdapter1 *adapter = NULL;
    bool success = false;
    HRESULT hr;

    pthread_once(&d3d11_once, d3d11_load);
    if (!pD3D11CreateDevice) {
        mp_fatal(log, "Failed to load d3d11.dll\n");
        goto done;
    }

    // Return here to retry creating the device
    do {
        // Use these default feature levels if they are not set
        max_fl = max_fl ? max_fl : D3D_FEATURE_LEVEL_11_0;
        min_fl = min_fl ? min_fl : D3D_FEATURE_LEVEL_9_1;

        hr = create_device(log, warp, opts->debug, max_fl, min_fl, &dev);
        if (SUCCEEDED(hr))
            break;

        // Trying to create a D3D_FEATURE_LEVEL_12_0 device on Windows 8.1 or
        // below will not succeed. Try an 11_1 device.
        if (max_fl >= D3D_FEATURE_LEVEL_12_0 &&
            min_fl <= D3D_FEATURE_LEVEL_11_1)
        {
            mp_dbg(log, "Failed to create 12_0+ device, trying 11_1\n");
            max_fl = D3D_FEATURE_LEVEL_11_1;
            continue;
        }

        // Trying to create a D3D_FEATURE_LEVEL_11_1 device on Windows 7
        // without the platform update will not succeed. Try an 11_0 device.
        if (max_fl >= D3D_FEATURE_LEVEL_11_1 &&
            min_fl <= D3D_FEATURE_LEVEL_11_0)
        {
            mp_dbg(log, "Failed to create 11_1+ device, trying 11_0\n");
            max_fl = D3D_FEATURE_LEVEL_11_0;
            continue;
        }

        // Retry with WARP if allowed
        if (!warp && opts->allow_warp) {
            mp_dbg(log, "Failed to create hardware device, trying WARP\n");
            warp = true;
            max_fl = opts->max_feature_level;
            min_fl = opts->min_feature_level;
            continue;
        }

        mp_fatal(log, "Failed to create Direct3D 11 device: %s\n",
                 mp_HRESULT_to_str(hr));
        goto done;
    } while (true);

    hr = ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice1, (void**)&dxgi_dev);
    if (FAILED(hr)) {
        mp_fatal(log, "Failed to get DXGI device\n");
        goto done;
    }
    hr = IDXGIDevice1_GetParent(dxgi_dev, &IID_IDXGIAdapter1, (void**)&adapter);
    if (FAILED(hr)) {
        mp_fatal(log, "Failed to get DXGI adapter\n");
        goto done;
    }

    IDXGIDevice1_SetMaximumFrameLatency(dxgi_dev, opts->max_frame_latency);

    DXGI_ADAPTER_DESC1 desc;
    hr = IDXGIAdapter1_GetDesc1(adapter, &desc);
    if (FAILED(hr)) {
        mp_fatal(log, "Failed to get adapter description\n");
        goto done;
    }

    D3D_FEATURE_LEVEL selected_level = ID3D11Device_GetFeatureLevel(dev);
    mp_verbose(log, "Using Direct3D 11 feature level %u_%u\n",
               ((unsigned)selected_level) >> 12,
               (((unsigned)selected_level) >> 8) & 0xf);

    char *dev_name = mp_to_utf8(NULL, desc.Description);
    mp_verbose(log, "Device Name: %s\n"
                    "Device ID: %04x:%04x (rev %02x)\n"
                    "Subsystem ID: %04x:%04x\n"
                    "LUID: %08lx%08lx\n",
               dev_name,
               desc.VendorId, desc.DeviceId, desc.Revision,
               LOWORD(desc.SubSysId), HIWORD(desc.SubSysId),
               desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart);
    talloc_free(dev_name);

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        warp = true;
    // If the primary display adapter is a software adapter, the
    // DXGI_ADAPTER_FLAG_SOFTWARE flag won't be set, but the device IDs should
    // still match the Microsoft Basic Render Driver
    if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
        warp = true;
    if (warp) {
        mp_msg(log, opts->force_warp ? MSGL_V : MSGL_WARN,
               "Using a software adapter\n");
    }

    *dev_out = dev;
    dev = NULL;
    success = true;

done:
    SAFE_RELEASE(adapter);
    SAFE_RELEASE(dxgi_dev);
    SAFE_RELEASE(dev);
    return success;
}

static HRESULT create_swapchain_1_2(ID3D11Device *dev, IDXGIFactory2 *factory,
                                    struct mp_log *log,
                                    struct d3d11_swapchain_opts *opts,
                                    bool flip, DXGI_FORMAT format,
                                    IDXGISwapChain **swapchain_out)
{
    IDXGISwapChain *swapchain = NULL;
    IDXGISwapChain1 *swapchain1 = NULL;
    HRESULT hr;

    DXGI_SWAP_CHAIN_DESC1 desc = {
        .Width = opts->width ? opts->width : 1,
        .Height = opts->height ? opts->height : 1,
        .Format = format,
        .SampleDesc = { .Count = 1 },
        .BufferUsage = opts->usage,
    };

    if (flip) {
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.BufferCount = opts->length;
    } else {
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        desc.BufferCount = 1;
    }

    hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown*)dev,
        opts->window, &desc, NULL, NULL, &swapchain1);
    if (FAILED(hr))
        goto done;
    hr = IDXGISwapChain1_QueryInterface(swapchain1, &IID_IDXGISwapChain,
                                        (void**)&swapchain);
    if (FAILED(hr))
        goto done;

    *swapchain_out = swapchain;
    swapchain = NULL;

done:
    SAFE_RELEASE(swapchain1);
    SAFE_RELEASE(swapchain);
    return hr;
}

static HRESULT create_swapchain_1_1(ID3D11Device *dev, IDXGIFactory1 *factory,
                                    struct mp_log *log,
                                    struct d3d11_swapchain_opts *opts,
                                    DXGI_FORMAT format,
                                    IDXGISwapChain **swapchain_out)
{
    DXGI_SWAP_CHAIN_DESC desc = {
        .BufferDesc = {
            .Width = opts->width ? opts->width : 1,
            .Height = opts->height ? opts->height : 1,
            .Format = format,
        },
        .SampleDesc = { .Count = 1 },
        .BufferUsage = opts->usage,
        .BufferCount = 1,
        .OutputWindow = opts->window,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
    };

    return IDXGIFactory1_CreateSwapChain(factory, (IUnknown*)dev, &desc,
                                         swapchain_out);
}

// Create a Direct3D 11 swapchain
bool mp_d3d11_create_swapchain(ID3D11Device *dev, struct mp_log *log,
                               struct d3d11_swapchain_opts *opts,
                               IDXGISwapChain **swapchain_out)
{
    IDXGIDevice1 *dxgi_dev = NULL;
    IDXGIAdapter1 *adapter = NULL;
    IDXGIFactory1 *factory = NULL;
    IDXGIFactory2 *factory2 = NULL;
    IDXGISwapChain *swapchain = NULL;
    bool success = false;
    HRESULT hr;

    hr = ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice1, (void**)&dxgi_dev);
    if (FAILED(hr)) {
        mp_fatal(log, "Failed to get DXGI device\n");
        goto done;
    }
    hr = IDXGIDevice1_GetParent(dxgi_dev, &IID_IDXGIAdapter1, (void**)&adapter);
    if (FAILED(hr)) {
        mp_fatal(log, "Failed to get DXGI adapter\n");
        goto done;
    }
    hr = IDXGIAdapter1_GetParent(adapter, &IID_IDXGIFactory1, (void**)&factory);
    if (FAILED(hr)) {
        mp_fatal(log, "Failed to get DXGI factory\n");
        goto done;
    }
    hr = IDXGIFactory1_QueryInterface(factory, &IID_IDXGIFactory2,
                                      (void**)&factory2);
    if (FAILED(hr))
        factory2 = NULL;

    bool flip = factory2 && opts->flip;

    // Return here to retry creating the swapchain
    do {
        if (factory2) {
            // Create a DXGI 1.2+ (Windows 8+) swap chain if possible
            hr = create_swapchain_1_2(dev, factory2, log, opts, flip,
                                      DXGI_FORMAT_R8G8B8A8_UNORM, &swapchain);
        } else {
            // Fall back to DXGI 1.1 (Windows 7)
            hr = create_swapchain_1_1(dev, factory, log, opts,
                                      DXGI_FORMAT_R8G8B8A8_UNORM, &swapchain);
        }
        if (SUCCEEDED(hr))
            break;

        if (flip) {
            mp_dbg(log, "Failed to create flip-model swapchain, trying bitblt\n");
            flip = false;
            continue;
        }

        mp_fatal(log, "Failed to create swapchain: %s\n", mp_HRESULT_to_str(hr));
        goto done;
    } while (true);

    // Prevent DXGI from making changes to the VO window, otherwise it will
    // hook the Alt+Enter keystroke and make it trigger an ugly transition to
    // exclusive fullscreen mode instead of running the user-set command.
    IDXGIFactory_MakeWindowAssociation(factory, opts->window,
        DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER |
        DXGI_MWA_NO_PRINT_SCREEN);

    if (factory2) {
        mp_verbose(log, "Using DXGI 1.2+\n");
    } else {
        mp_verbose(log, "Using DXGI 1.1\n");
    }

    DXGI_SWAP_CHAIN_DESC scd = {0};
    IDXGISwapChain_GetDesc(swapchain, &scd);
    if (scd.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL) {
        mp_verbose(log, "Using flip-model presentation\n");
    } else {
        mp_verbose(log, "Using bitblt-model presentation\n");
    }

    *swapchain_out = swapchain;
    swapchain = NULL;
    success = true;

done:
    SAFE_RELEASE(swapchain);
    SAFE_RELEASE(factory2);
    SAFE_RELEASE(factory);
    SAFE_RELEASE(adapter);
    SAFE_RELEASE(dxgi_dev);
    return success;
}
