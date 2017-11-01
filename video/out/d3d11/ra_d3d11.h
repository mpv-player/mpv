#pragma once

#include <stdbool.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include "video/out/gpu/ra.h"
#include "video/out/gpu/spirv.h"

// Create an RA instance from a D3D11 device. This takes a reference to the
// device, which is released when the RA instance is destroyed.
struct ra *ra_d3d11_create(ID3D11Device *device, struct mp_log *log,
                           struct spirv_compiler *spirv);

// Flush the immediate context of the wrapped D3D11 device
void ra_d3d11_flush(struct ra *ra);

// Create an RA texture from a D3D11 resource. This takes a reference to the
// texture, which is released when the RA texture is destroyed.
struct ra_tex *ra_d3d11_wrap_tex(struct ra *ra, ID3D11Resource *res);

// As above, but for a D3D11VA video resource. The fmt parameter selects which
// plane of a planar format will be mapped when the RA texture is used.
// array_slice should be set for texture arrays and is ignored for non-arrays.
struct ra_tex *ra_d3d11_wrap_tex_video(struct ra *ra, ID3D11Texture2D *res,
                                       int w, int h, int array_slice,
                                       const struct ra_format *fmt);

// Get the underlying D3D11 device from an RA instance. The returned device is
// refcounted and must be released by the caller.
ID3D11Device *ra_d3d11_get_device(struct ra *ra);

// True if the RA instance was created with ra_d3d11_create()
bool ra_is_d3d11(struct ra *ra);
