#include "angle_common.h"

// Test if Direct3D11 can be used by us. Basically, this prevents trying to use
// D3D11 on Win7, and then failing somewhere in the process.
bool d3d11_check_decoding(ID3D11Device *dev)
{
    HRESULT hr;
    // We assume that NV12 is always supported, if hw decoding is supported at
    // all.
    UINT supported = 0;
    hr = ID3D11Device_CheckFormatSupport(dev, DXGI_FORMAT_NV12, &supported);
    return !FAILED(hr) && (supported & D3D11_BIND_DECODER);
}
