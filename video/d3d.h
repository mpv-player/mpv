#ifndef MP_D3D_H_
#define MP_D3D_H_

#include <d3d9.h>
#include <d3d11.h>

#include "hwdec.h"

struct mp_d3d_ctx {
    struct mp_hwdec_ctx hwctx;
    IDirect3DDevice9 *d3d9_device;
    ID3D11Device *d3d11_device;
};

#endif
