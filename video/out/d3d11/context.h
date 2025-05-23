#pragma once

#include <dxgi.h>

#include "video/out/gpu/context.h"

// Get the underlying D3D11 swap chain from an RA context. The returned swap chain is
// refcounted and must be released by the caller.
IDXGISwapChain *ra_d3d11_ctx_get_swapchain(struct ra_ctx *ra);

// Returns true if an 8-bit output format is explicitly requested for
// d3d11-output-format for an RA context.
bool ra_d3d11_ctx_prefer_8bit_output_format(struct ra_ctx *ra);
