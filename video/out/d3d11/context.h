#pragma once

#include <dxgi.h>

#include "video/out/gpu/context.h"

// Get the underlying D3D11 swap chain from an RA context. The returned swap chain is
// refcounted and must be released by the caller.
IDXGISwapChain *ra_d3d11_ctx_get_swapchain(struct ra_ctx *ra);
