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

#ifndef MP_D3D11_HELPERS_H_
#define MP_D3D11_HELPERS_H_

#include <stdbool.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#include "video/mp_image.h"

#if !HAVE_DXGI_DEBUG_D3D11
DEFINE_GUID(DXGI_DEBUG_D3D11, 0x4b99317b, 0xac39, 0x4aa6, 0xbb, 0xb, 0xba, 0xa0, 0x47, 0x84, 0x79, 0x8f);
#endif

struct d3d11_device_opts {
    // Enable the debug layer (D3D11_CREATE_DEVICE_DEBUG)
    bool debug;

    // Allow a software (WARP) adapter. Note, sometimes a software adapter will
    // be used even when allow_warp is false. This is because, on Windows 8 and
    // up, if there are no hardware adapters, Windows will pretend the WARP
    // adapter is the primary hardware adapter.
    bool allow_warp;

    // Always use a WARP adapter. This is mainly for testing purposes.
    bool force_warp;

    // The maximum number of pending frames allowed to be queued to a swapchain
    int max_frame_latency;

    // The maximum Direct3D 11 feature level to attempt to create
    // If unset, defaults to D3D_FEATURE_LEVEL_11_0
    int max_feature_level;

    // The minimum Direct3D 11 feature level to attempt to create. If this is
    // not supported, device creation will fail.
    // If unset, defaults to D3D_FEATURE_LEVEL_9_1
    int min_feature_level;

    // The adapter name to utilize if a specific adapter is required
    // If unset, the default adapter will be utilized when creating
    // a device.
    char *adapter_name;
};

IDXGIAdapter1 *mp_get_dxgi_adapter(struct mp_log *log,
                                   bstr requested_adapter_name,
                                   bstr *listing);

bool mp_get_dxgi_output_desc(IDXGISwapChain *swapchain, DXGI_OUTPUT_DESC1 *desc);

OPT_STRING_VALIDATE_FUNC(mp_dxgi_validate_adapter);

bool mp_dxgi_list_or_verify_adapters(struct mp_log *log,
                                     bstr adapter_name,
                                     bstr *listing);

bool mp_d3d11_create_present_device(struct mp_log *log,
                                    struct d3d11_device_opts *opts,
                                    ID3D11Device **dev_out);

struct d3d11_swapchain_opts {
    HWND window;
    int width;
    int height;
    DXGI_FORMAT format;
    DXGI_COLOR_SPACE_TYPE color_space;

    // pl_color_space mapping of the configured swapchain colorspace
    // shall be written into this memory location if configuration
    // succeeds. Will be ignored if NULL.
    struct pl_color_space *configured_csp;

    // Use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL if possible
    bool flip;

    // Number of surfaces in the swapchain
    int length;

    // The BufferUsage value for swapchain surfaces. This should probably
    // contain DXGI_USAGE_RENDER_TARGET_OUTPUT.
    DXGI_USAGE usage;
};

bool mp_d3d11_create_swapchain(ID3D11Device *dev, struct mp_log *log,
                               struct d3d11_swapchain_opts *opts,
                               IDXGISwapChain **swapchain_out);

void mp_d3d11_get_debug_interfaces(struct mp_log *log, IDXGIDebug **debug,
                                   IDXGIInfoQueue **iqueue);

#endif
