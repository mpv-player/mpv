#include <windows.h>
#include <versionhelpers.h>
#include <d3d11_1.h>
#include <d3d11sdklayers.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <crossc.h>

#include "common/msg.h"
#include "osdep/io.h"
#include "osdep/subprocess.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"
#include "video/out/gpu/spirv.h"
#include "video/out/gpu/utils.h"

#include "ra_d3d11.h"

#ifndef D3D11_1_UAV_SLOT_COUNT
#define D3D11_1_UAV_SLOT_COUNT (64)
#endif

struct dll_version {
    uint16_t major;
    uint16_t minor;
    uint16_t build;
    uint16_t revision;
};

struct ra_d3d11 {
    struct spirv_compiler *spirv;

    ID3D11Device *dev;
    ID3D11Device1 *dev1;
    ID3D11DeviceContext *ctx;
    ID3D11DeviceContext1 *ctx1;
    pD3DCompile D3DCompile;

    struct dll_version d3d_compiler_ver;

    // Debug interfaces (--gpu-debug)
    ID3D11Debug *debug;
    ID3D11InfoQueue *iqueue;

    // Device capabilities
    D3D_FEATURE_LEVEL fl;
    bool has_clear_view;
    bool has_timestamp_queries;
    int max_uavs;

    // Streaming dynamic vertex buffer, which is used for all renderpasses
    ID3D11Buffer *vbuf;
    size_t vbuf_size;
    size_t vbuf_used;

    // clear() renderpass resources (only used when has_clear_view is false)
    ID3D11PixelShader *clear_ps;
    ID3D11VertexShader *clear_vs;
    ID3D11InputLayout *clear_layout;
    ID3D11Buffer *clear_vbuf;
    ID3D11Buffer *clear_cbuf;

    // blit() renderpass resources
    ID3D11PixelShader *blit_float_ps;
    ID3D11VertexShader *blit_vs;
    ID3D11InputLayout *blit_layout;
    ID3D11Buffer *blit_vbuf;
    ID3D11SamplerState *blit_sampler;
};

struct d3d_tex {
    // res mirrors one of tex1d, tex2d or tex3d for convenience. It does not
    // hold an additional reference to the texture object.
    ID3D11Resource *res;

    ID3D11Texture1D *tex1d;
    ID3D11Texture2D *tex2d;
    ID3D11Texture3D *tex3d;
    int array_slice;

    // Staging texture for tex_download(), 2D only
    ID3D11Texture2D *staging;

    ID3D11ShaderResourceView *srv;
    ID3D11RenderTargetView *rtv;
    ID3D11UnorderedAccessView *uav;
    ID3D11SamplerState *sampler;
};

struct d3d_buf {
    ID3D11Buffer *buf;
    ID3D11UnorderedAccessView *uav;
    void *data; // System-memory mirror of the data in buf
    bool dirty; // Is buf out of date?
};

struct d3d_rpass {
    ID3D11PixelShader *ps;
    ID3D11VertexShader *vs;
    ID3D11ComputeShader *cs;
    ID3D11InputLayout *layout;
    ID3D11BlendState *bstate;
};

struct d3d_timer {
    ID3D11Query *ts_start;
    ID3D11Query *ts_end;
    ID3D11Query *disjoint;
    uint64_t result; // Latches the result from the previous use of the timer
};

struct d3d_fmt {
    const char *name;
    int components;
    int bytes;
    int bits[4];
    DXGI_FORMAT fmt;
    enum ra_ctype ctype;
    bool unordered;
};

static const char clear_vs[] = "\
float4 main(float2 pos : POSITION) : SV_Position\n\
{\n\
    return float4(pos, 0.0, 1.0);\n\
}\n\
";

static const char clear_ps[] = "\
cbuffer ps_cbuf : register(b0) {\n\
    float4 color : packoffset(c0);\n\
}\n\
\n\
float4 main(float4 pos : SV_Position) : SV_Target\n\
{\n\
    return color;\n\
}\n\
";

struct blit_vert {
    float x, y, u, v;
};

static const char blit_vs[] = "\
void main(float2 pos : POSITION, float2 coord : TEXCOORD0,\n\
          out float4 out_pos : SV_Position, out float2 out_coord : TEXCOORD0)\n\
{\n\
    out_pos = float4(pos, 0.0, 1.0);\n\
    out_coord = coord;\n\
}\n\
";

static const char blit_float_ps[] = "\
Texture2D<float4> tex : register(t0);\n\
SamplerState samp : register(s0);\n\
\n\
float4 main(float4 pos : SV_Position, float2 coord : TEXCOORD0) : SV_Target\n\
{\n\
    return tex.Sample(samp, coord);\n\
}\n\
";

#define DXFMT(f, t) .fmt = DXGI_FORMAT_##f##_##t, .ctype = RA_CTYPE_##t
static struct d3d_fmt formats[] = {
    { "r8",       1,  1, { 8},             DXFMT(R8, UNORM)           },
    { "rg8",      2,  2, { 8,  8},         DXFMT(R8G8, UNORM)         },
    { "rgba8",    4,  4, { 8,  8,  8,  8}, DXFMT(R8G8B8A8, UNORM)     },
    { "r16",      1,  2, {16},             DXFMT(R16, UNORM)          },
    { "rg16",     2,  4, {16, 16},         DXFMT(R16G16, UNORM)       },
    { "rgba16",   4,  8, {16, 16, 16, 16}, DXFMT(R16G16B16A16, UNORM) },

    { "r32ui",    1,  4, {32},             DXFMT(R32, UINT)           },
    { "rg32ui",   2,  8, {32, 32},         DXFMT(R32G32, UINT)        },
    { "rgb32ui",  3, 12, {32, 32, 32},     DXFMT(R32G32B32, UINT)     },
    { "rgba32ui", 4, 16, {32, 32, 32, 32}, DXFMT(R32G32B32A32, UINT)  },

    { "r16hf",    1,  2, {16},             DXFMT(R16, FLOAT)          },
    { "rg16hf",   2,  4, {16, 16},         DXFMT(R16G16, FLOAT)       },
    { "rgba16hf", 4,  8, {16, 16, 16, 16}, DXFMT(R16G16B16A16, FLOAT) },
    { "r32f",     1,  4, {32},             DXFMT(R32, FLOAT)          },
    { "rg32f",    2,  8, {32, 32},         DXFMT(R32G32, FLOAT)       },
    { "rgb32f",   3, 12, {32, 32, 32},     DXFMT(R32G32B32, FLOAT)    },
    { "rgba32f",  4, 16, {32, 32, 32, 32}, DXFMT(R32G32B32A32, FLOAT) },

    { "rgb10_a2", 4,  4, {10, 10, 10,  2}, DXFMT(R10G10B10A2, UNORM)  },
    { "bgra8",    4,  4, { 8,  8,  8,  8}, DXFMT(B8G8R8A8, UNORM), .unordered = true },
    { "bgrx8",    3,  4, { 8,  8,  8},     DXFMT(B8G8R8X8, UNORM), .unordered = true },
};

static bool dll_version_equal(struct dll_version a, struct dll_version b)
{
    return a.major == b.major &&
           a.minor == b.minor &&
           a.build == b.build &&
           a.revision == b.revision;
}

static DXGI_FORMAT fmt_to_dxgi(const struct ra_format *fmt)
{
    struct d3d_fmt *d3d = fmt->priv;
    return d3d->fmt;
}

static void setup_formats(struct ra *ra)
{
    // All formats must be usable as a 2D texture
    static const UINT sup_basic = D3D11_FORMAT_SUPPORT_TEXTURE2D;
    // SHADER_SAMPLE indicates support for linear sampling, point always works
    static const UINT sup_filter = D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
    // RA requires renderable surfaces to be blendable as well
    static const UINT sup_render = D3D11_FORMAT_SUPPORT_RENDER_TARGET |
                                   D3D11_FORMAT_SUPPORT_BLENDABLE;

    struct ra_d3d11 *p = ra->priv;
    HRESULT hr;

    for (int i = 0; i < MP_ARRAY_SIZE(formats); i++) {
        struct d3d_fmt *d3dfmt = &formats[i];
        UINT support = 0;
        hr = ID3D11Device_CheckFormatSupport(p->dev, d3dfmt->fmt, &support);
        if (FAILED(hr))
            continue;
        if ((support & sup_basic) != sup_basic)
            continue;

        struct ra_format *fmt = talloc_zero(ra, struct ra_format);
        *fmt = (struct ra_format) {
            .name           = d3dfmt->name,
            .priv           = d3dfmt,
            .ctype          = d3dfmt->ctype,
            .ordered        = !d3dfmt->unordered,
            .num_components = d3dfmt->components,
            .pixel_size     = d3dfmt->bytes,
            .linear_filter  = (support & sup_filter) == sup_filter,
            .renderable     = (support & sup_render) == sup_render,
        };

        if (support & D3D11_FORMAT_SUPPORT_TEXTURE1D)
            ra->caps |= RA_CAP_TEX_1D;

        for (int j = 0; j < d3dfmt->components; j++)
            fmt->component_size[j] = fmt->component_depth[j] = d3dfmt->bits[j];

        fmt->glsl_format = ra_fmt_glsl_format(fmt);

        MP_TARRAY_APPEND(ra, ra->formats, ra->num_formats, fmt);
    }
}

static bool tex_init(struct ra *ra, struct ra_tex *tex)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_tex *tex_p = tex->priv;
    struct ra_tex_params *params = &tex->params;
    HRESULT hr;

    // A SRV is required for renderpasses and blitting, since blitting can use
    // a renderpass internally
    if (params->render_src || params->blit_src) {
        // Always specify the SRV format for simplicity. This will match the
        // texture format for textures created with tex_create, but it can be
        // different for wrapped planar video textures.
        D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc = {
            .Format = fmt_to_dxgi(params->format),
        };
        switch (params->dimensions) {
        case 1:
            if (tex_p->array_slice >= 0) {
                srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                srvdesc.Texture1DArray.MipLevels = 1;
                srvdesc.Texture1DArray.FirstArraySlice = tex_p->array_slice;
                srvdesc.Texture1DArray.ArraySize = 1;
            } else {
                srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                srvdesc.Texture1D.MipLevels = 1;
            }
            break;
        case 2:
            if (tex_p->array_slice >= 0) {
                srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srvdesc.Texture2DArray.MipLevels = 1;
                srvdesc.Texture2DArray.FirstArraySlice = tex_p->array_slice;
                srvdesc.Texture2DArray.ArraySize = 1;
            } else {
                srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvdesc.Texture2D.MipLevels = 1;
            }
            break;
        case 3:
            // D3D11 does not have Texture3D arrays
            srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            srvdesc.Texture3D.MipLevels = 1;
            break;
        }
        hr = ID3D11Device_CreateShaderResourceView(p->dev, tex_p->res, &srvdesc,
                                                   &tex_p->srv);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create SRV: %s\n", mp_HRESULT_to_str(hr));
            goto error;
        }
    }

    // Samplers are required for renderpasses, but not blitting, since the blit
    // code uses its own point sampler
    if (params->render_src) {
        D3D11_SAMPLER_DESC sdesc = {
            .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
            .ComparisonFunc = D3D11_COMPARISON_NEVER,
            .MinLOD = 0,
            .MaxLOD = D3D11_FLOAT32_MAX,
            .MaxAnisotropy = 1,
        };
        if (params->src_linear)
            sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        if (params->src_repeat) {
            sdesc.AddressU = sdesc.AddressV = sdesc.AddressW =
                D3D11_TEXTURE_ADDRESS_WRAP;
        }
        // The runtime pools sampler state objects internally, so we don't have
        // to worry about resource usage when creating one for every ra_tex
        hr = ID3D11Device_CreateSamplerState(p->dev, &sdesc, &tex_p->sampler);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create sampler: %s\n", mp_HRESULT_to_str(hr));
            goto error;
        }
    }

    // Like SRVs, an RTV is required for renderpass output and blitting
    if (params->render_dst || params->blit_dst) {
        hr = ID3D11Device_CreateRenderTargetView(p->dev, tex_p->res, NULL,
                                                 &tex_p->rtv);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create RTV: %s\n", mp_HRESULT_to_str(hr));
            goto error;
        }
    }

    if (p->fl >= D3D_FEATURE_LEVEL_11_0 && params->storage_dst) {
        hr = ID3D11Device_CreateUnorderedAccessView(p->dev, tex_p->res, NULL,
                                                    &tex_p->uav);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create UAV: %s\n", mp_HRESULT_to_str(hr));
            goto error;
        }
    }

    return true;
error:
    return false;
}

static void tex_destroy(struct ra *ra, struct ra_tex *tex)
{
    if (!tex)
        return;
    struct d3d_tex *tex_p = tex->priv;

    SAFE_RELEASE(tex_p->srv);
    SAFE_RELEASE(tex_p->rtv);
    SAFE_RELEASE(tex_p->uav);
    SAFE_RELEASE(tex_p->sampler);
    SAFE_RELEASE(tex_p->res);
    SAFE_RELEASE(tex_p->staging);
    talloc_free(tex);
}

static struct ra_tex *tex_create(struct ra *ra,
                                 const struct ra_tex_params *params)
{
    // Only 2D textures may be downloaded for now
    if (params->downloadable && params->dimensions != 2)
        return NULL;

    struct ra_d3d11 *p = ra->priv;
    HRESULT hr;

    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    tex->params = *params;
    tex->params.initial_data = NULL;

    struct d3d_tex *tex_p = tex->priv = talloc_zero(tex, struct d3d_tex);
    DXGI_FORMAT fmt = fmt_to_dxgi(params->format);

    D3D11_SUBRESOURCE_DATA *pdata = NULL;
    if (params->initial_data) {
        pdata = &(D3D11_SUBRESOURCE_DATA) {
            .pSysMem = params->initial_data,
            .SysMemPitch = params->w * params->format->pixel_size,
        };
        if (params->dimensions >= 3)
            pdata->SysMemSlicePitch = pdata->SysMemPitch * params->h;
    }

    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    D3D11_BIND_FLAG bind_flags = 0;

    if (params->render_src || params->blit_src)
        bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    if (params->render_dst || params->blit_dst)
        bind_flags |= D3D11_BIND_RENDER_TARGET;
    if (p->fl >= D3D_FEATURE_LEVEL_11_0 && params->storage_dst)
        bind_flags |= D3D11_BIND_UNORDERED_ACCESS;

    // Apparently IMMUTABLE textures are efficient, so try to infer whether we
    // can use one
    if (params->initial_data && !params->render_dst && !params->storage_dst &&
        !params->blit_dst && !params->host_mutable)
        usage = D3D11_USAGE_IMMUTABLE;

    switch (params->dimensions) {
    case 1:;
        D3D11_TEXTURE1D_DESC desc1d = {
            .Width = params->w,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = fmt,
            .Usage = usage,
            .BindFlags = bind_flags,
        };
        hr = ID3D11Device_CreateTexture1D(p->dev, &desc1d, pdata, &tex_p->tex1d);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create Texture1D: %s\n",
                   mp_HRESULT_to_str(hr));
            goto error;
        }
        tex_p->res = (ID3D11Resource *)tex_p->tex1d;
        break;
    case 2:;
        D3D11_TEXTURE2D_DESC desc2d = {
            .Width = params->w,
            .Height = params->h,
            .MipLevels = 1,
            .ArraySize = 1,
            .SampleDesc.Count = 1,
            .Format = fmt,
            .Usage = usage,
            .BindFlags = bind_flags,
        };
        hr = ID3D11Device_CreateTexture2D(p->dev, &desc2d, pdata, &tex_p->tex2d);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create Texture2D: %s\n",
                   mp_HRESULT_to_str(hr));
            goto error;
        }
        tex_p->res = (ID3D11Resource *)tex_p->tex2d;

        // Create a staging texture with CPU access for tex_download()
        if (params->downloadable) {
            desc2d.BindFlags = 0;
            desc2d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc2d.Usage = D3D11_USAGE_STAGING;

            hr = ID3D11Device_CreateTexture2D(p->dev, &desc2d, NULL,
                                              &tex_p->staging);
            if (FAILED(hr)) {
                MP_ERR(ra, "Failed to staging texture: %s\n",
                       mp_HRESULT_to_str(hr));
                goto error;
            }
        }
        break;
    case 3:;
        D3D11_TEXTURE3D_DESC desc3d = {
            .Width = params->w,
            .Height = params->h,
            .Depth = params->d,
            .MipLevels = 1,
            .Format = fmt,
            .Usage = usage,
            .BindFlags = bind_flags,
        };
        hr = ID3D11Device_CreateTexture3D(p->dev, &desc3d, pdata, &tex_p->tex3d);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create Texture3D: %s\n",
                   mp_HRESULT_to_str(hr));
            goto error;
        }
        tex_p->res = (ID3D11Resource *)tex_p->tex3d;
        break;
    default:
        abort();
    }

    tex_p->array_slice = -1;

    if (!tex_init(ra, tex))
        goto error;

    return tex;

error:
    tex_destroy(ra, tex);
    return NULL;
}

struct ra_tex *ra_d3d11_wrap_tex(struct ra *ra, ID3D11Resource *res)
{
    HRESULT hr;

    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    struct ra_tex_params *params = &tex->params;
    struct d3d_tex *tex_p = tex->priv = talloc_zero(tex, struct d3d_tex);

    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    D3D11_BIND_FLAG bind_flags = 0;

    D3D11_RESOURCE_DIMENSION type;
    ID3D11Resource_GetType(res, &type);
    switch (type) {
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        hr = ID3D11Resource_QueryInterface(res, &IID_ID3D11Texture2D,
                                           (void**)&tex_p->tex2d);
        if (FAILED(hr)) {
            MP_ERR(ra, "Resource is not a ID3D11Texture2D\n");
            goto error;
        }
        tex_p->res = (ID3D11Resource *)tex_p->tex2d;

        D3D11_TEXTURE2D_DESC desc2d;
        ID3D11Texture2D_GetDesc(tex_p->tex2d, &desc2d);
        if (desc2d.MipLevels != 1) {
            MP_ERR(ra, "Mipmapped textures not supported for wrapping\n");
            goto error;
        }
        if (desc2d.ArraySize != 1) {
            MP_ERR(ra, "Texture arrays not supported for wrapping\n");
            goto error;
        }
        if (desc2d.SampleDesc.Count != 1) {
            MP_ERR(ra, "Multisampled textures not supported for wrapping\n");
            goto error;
        }

        params->dimensions = 2;
        params->w = desc2d.Width;
        params->h = desc2d.Height;
        params->d = 1;
        usage = desc2d.Usage;
        bind_flags = desc2d.BindFlags;
        fmt = desc2d.Format;
        break;
    default:
        // We could wrap Texture1D/3D as well, but keep it simple, since this
        // function is only used for swapchain backbuffers at the moment
        MP_ERR(ra, "Resource is not suitable to wrap\n");
        goto error;
    }

    for (int i = 0; i < ra->num_formats; i++) {
        DXGI_FORMAT target_fmt = fmt_to_dxgi(ra->formats[i]);
        if (fmt == target_fmt) {
            params->format = ra->formats[i];
            break;
        }
    }
    if (!params->format) {
        MP_ERR(ra, "Could not find a suitable RA format for wrapped resource\n");
        goto error;
    }

    if (bind_flags & D3D11_BIND_SHADER_RESOURCE)
        params->render_src = params->blit_src = true;
    if (bind_flags & D3D11_BIND_RENDER_TARGET)
        params->render_dst = params->blit_dst = true;
    if (bind_flags & D3D11_BIND_UNORDERED_ACCESS)
        params->storage_dst = true;

    if (usage != D3D11_USAGE_DEFAULT) {
        MP_ERR(ra, "Resource is not D3D11_USAGE_DEFAULT\n");
        goto error;
    }

    tex_p->array_slice = -1;

    if (!tex_init(ra, tex))
        goto error;

    return tex;
error:
    tex_destroy(ra, tex);
    return NULL;
}

struct ra_tex *ra_d3d11_wrap_tex_video(struct ra *ra, ID3D11Texture2D *res,
                                       int w, int h, int array_slice,
                                       const struct ra_format *fmt)
{
    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    struct ra_tex_params *params = &tex->params;
    struct d3d_tex *tex_p = tex->priv = talloc_zero(tex, struct d3d_tex);

    tex_p->tex2d = res;
    tex_p->res = (ID3D11Resource *)tex_p->tex2d;
    ID3D11Texture2D_AddRef(res);

    D3D11_TEXTURE2D_DESC desc2d;
    ID3D11Texture2D_GetDesc(tex_p->tex2d, &desc2d);
    if (!(desc2d.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
        MP_ERR(ra, "Video resource is not bindable\n");
        goto error;
    }

    params->dimensions = 2;
    params->w = w;
    params->h = h;
    params->d = 1;
    params->render_src = true;
    params->src_linear = true;
    // fmt can be different to the texture format for planar video textures
    params->format = fmt;

    if (desc2d.ArraySize > 1) {
        tex_p->array_slice = array_slice;
    } else {
        tex_p->array_slice = -1;
    }

    if (!tex_init(ra, tex))
        goto error;

    return tex;
error:
    tex_destroy(ra, tex);
    return NULL;
}

static bool tex_upload(struct ra *ra, const struct ra_tex_upload_params *params)
{
    struct ra_d3d11 *p = ra->priv;
    struct ra_tex *tex = params->tex;
    struct d3d_tex *tex_p = tex->priv;

    if (!params->src) {
        MP_ERR(ra, "Pixel buffers are not supported\n");
        return false;
    }

    const char *src = params->src;
    ptrdiff_t stride = tex->params.dimensions >= 2 ? tex->params.w : 0;
    ptrdiff_t pitch = tex->params.dimensions >= 3 ? stride * tex->params.h : 0;
    bool invalidate = true;
    D3D11_BOX *rc = NULL;

    if (tex->params.dimensions == 2) {
        stride = params->stride;

        if (params->rc && (params->rc->x0 != 0 || params->rc->y0 != 0 ||
            params->rc->x1 != tex->params.w || params->rc->y1 != tex->params.h))
        {
            rc = &(D3D11_BOX) {
                .left = params->rc->x0,
                .top = params->rc->y0,
                .front = 0,
                .right = params->rc->x1,
                .bottom = params->rc->y1,
                .back = 1,
            };
            invalidate = params->invalidate;
        }
    }

    int subresource = tex_p->array_slice >= 0 ? tex_p->array_slice : 0;
    if (p->ctx1) {
        ID3D11DeviceContext1_UpdateSubresource1(p->ctx1, tex_p->res,
            subresource, rc, src, stride, pitch,
            invalidate ? D3D11_COPY_DISCARD : 0);
    } else {
        ID3D11DeviceContext_UpdateSubresource(p->ctx, tex_p->res, subresource,
            rc, src, stride, pitch);
    }

    return true;
}

static bool tex_download(struct ra *ra, struct ra_tex_download_params *params)
{
    struct ra_d3d11 *p = ra->priv;
    struct ra_tex *tex = params->tex;
    struct d3d_tex *tex_p = tex->priv;
    HRESULT hr;

    if (!tex_p->staging)
        return false;

    ID3D11DeviceContext_CopyResource(p->ctx, (ID3D11Resource*)tex_p->staging,
        tex_p->res);

    D3D11_MAPPED_SUBRESOURCE lock;
    hr = ID3D11DeviceContext_Map(p->ctx, (ID3D11Resource*)tex_p->staging, 0,
                                 D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to map staging texture: %s\n", mp_HRESULT_to_str(hr));
        return false;
    }

    char *cdst = params->dst;
    char *csrc = lock.pData;
    for (int y = 0; y < tex->params.h; y++) {
        memcpy(cdst + y * params->stride, csrc + y * lock.RowPitch,
               MPMIN(params->stride, lock.RowPitch));
    }

    ID3D11DeviceContext_Unmap(p->ctx, (ID3D11Resource*)tex_p->staging, 0);

    return true;
}

static void buf_destroy(struct ra *ra, struct ra_buf *buf)
{
    if (!buf)
        return;
    struct d3d_buf *buf_p = buf->priv;
    SAFE_RELEASE(buf_p->buf);
    SAFE_RELEASE(buf_p->uav);
    talloc_free(buf);
}

static struct ra_buf *buf_create(struct ra *ra,
                                 const struct ra_buf_params *params)
{
    // D3D11 does not support permanent mapping or pixel buffers
    if (params->host_mapped || params->type == RA_BUF_TYPE_TEX_UPLOAD)
        return NULL;

    struct ra_d3d11 *p = ra->priv;
    HRESULT hr;

    struct ra_buf *buf = talloc_zero(NULL, struct ra_buf);
    buf->params = *params;
    buf->params.initial_data = NULL;

    struct d3d_buf *buf_p = buf->priv = talloc_zero(buf, struct d3d_buf);

    D3D11_SUBRESOURCE_DATA *pdata = NULL;
    if (params->initial_data)
        pdata = &(D3D11_SUBRESOURCE_DATA) { .pSysMem = params->initial_data };

    D3D11_BUFFER_DESC desc = { .ByteWidth = params->size };
    switch (params->type) {
    case RA_BUF_TYPE_SHADER_STORAGE:
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        desc.ByteWidth = MP_ALIGN_UP(desc.ByteWidth, sizeof(float));
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        break;
    case RA_BUF_TYPE_UNIFORM:
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.ByteWidth = MP_ALIGN_UP(desc.ByteWidth, sizeof(float[4]));
        break;
    }

    hr = ID3D11Device_CreateBuffer(p->dev, &desc, pdata, &buf_p->buf);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create buffer: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }

    // D3D11 doesn't allow constant buffer updates that aren't aligned to a
    // full constant boundary (vec4,) and some drivers don't allow partial
    // constant buffer updates at all. To support partial buffer updates, keep
    // a mirror of the buffer data in system memory and upload the whole thing
    // before the buffer is used.
    if (params->host_mutable)
        buf_p->data = talloc_zero_size(buf, desc.ByteWidth);

    if (params->type == RA_BUF_TYPE_SHADER_STORAGE) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC udesc = {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
            .Buffer = {
                .NumElements = desc.ByteWidth / sizeof(float),
                .Flags = D3D11_BUFFER_UAV_FLAG_RAW,
            },
        };
        hr = ID3D11Device_CreateUnorderedAccessView(p->dev,
            (ID3D11Resource *)buf_p->buf, &udesc, &buf_p->uav);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create UAV: %s\n", mp_HRESULT_to_str(hr));
            goto error;
        }
    }

    return buf;
error:
    buf_destroy(ra, buf);
    return NULL;
}

static void buf_resolve(struct ra *ra, struct ra_buf *buf)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_buf *buf_p = buf->priv;

    if (!buf->params.host_mutable || !buf_p->dirty)
        return;

    // Synchronize the GPU buffer with the system-memory copy
    ID3D11DeviceContext_UpdateSubresource(p->ctx, (ID3D11Resource *)buf_p->buf,
        0, NULL, buf_p->data, 0, 0);
    buf_p->dirty = false;
}

static void buf_update(struct ra *ra, struct ra_buf *buf, ptrdiff_t offset,
                       const void *data, size_t size)
{
    struct d3d_buf *buf_p = buf->priv;

    char *cdata = buf_p->data;
    memcpy(cdata + offset, data, size);
    buf_p->dirty = true;
}

static const char *get_shader_target(struct ra *ra, enum glsl_shader type)
{
    struct ra_d3d11 *p = ra->priv;
    switch (p->fl) {
    default:
        switch (type) {
        case GLSL_SHADER_VERTEX:   return "vs_5_0";
        case GLSL_SHADER_FRAGMENT: return "ps_5_0";
        case GLSL_SHADER_COMPUTE:  return "cs_5_0";
        }
        break;
    case D3D_FEATURE_LEVEL_10_1:
        switch (type) {
        case GLSL_SHADER_VERTEX:   return "vs_4_1";
        case GLSL_SHADER_FRAGMENT: return "ps_4_1";
        case GLSL_SHADER_COMPUTE:  return "cs_4_1";
        }
        break;
    case D3D_FEATURE_LEVEL_10_0:
        switch (type) {
        case GLSL_SHADER_VERTEX:   return "vs_4_0";
        case GLSL_SHADER_FRAGMENT: return "ps_4_0";
        case GLSL_SHADER_COMPUTE:  return "cs_4_0";
        }
        break;
    case D3D_FEATURE_LEVEL_9_3:
        switch (type) {
        case GLSL_SHADER_VERTEX:   return "vs_4_0_level_9_3";
        case GLSL_SHADER_FRAGMENT: return "ps_4_0_level_9_3";
        }
        break;
    case D3D_FEATURE_LEVEL_9_2:
    case D3D_FEATURE_LEVEL_9_1:
        switch (type) {
        case GLSL_SHADER_VERTEX:   return "vs_4_0_level_9_1";
        case GLSL_SHADER_FRAGMENT: return "ps_4_0_level_9_1";
        }
        break;
    }
    return NULL;
}

static const char *shader_type_name(enum glsl_shader type)
{
    switch (type) {
    case GLSL_SHADER_VERTEX:   return "vertex";
    case GLSL_SHADER_FRAGMENT: return "fragment";
    case GLSL_SHADER_COMPUTE:  return "compute";
    default:                   return "unknown";
    }
}

static bool setup_clear_rpass(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    ID3DBlob *vs_blob = NULL;
    ID3DBlob *ps_blob = NULL;
    HRESULT hr;

    hr = p->D3DCompile(clear_vs, sizeof(clear_vs), NULL, NULL, NULL, "main",
        get_shader_target(ra, GLSL_SHADER_VERTEX),
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs_blob, NULL);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to compile clear() vertex shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = ID3D11Device_CreateVertexShader(p->dev,
        ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob),
        NULL, &p->clear_vs);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create clear() vertex shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = p->D3DCompile(clear_ps, sizeof(clear_ps), NULL, NULL, NULL, "main",
        get_shader_target(ra, GLSL_SHADER_FRAGMENT),
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps_blob, NULL);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to compile clear() pixel shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = ID3D11Device_CreatePixelShader(p->dev,
        ID3D10Blob_GetBufferPointer(ps_blob), ID3D10Blob_GetBufferSize(ps_blob),
        NULL, &p->clear_ps);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create clear() pixel shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    D3D11_INPUT_ELEMENT_DESC in_descs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0 },
    };
    hr = ID3D11Device_CreateInputLayout(p->dev, in_descs,
        MP_ARRAY_SIZE(in_descs), ID3D10Blob_GetBufferPointer(vs_blob),
        ID3D10Blob_GetBufferSize(vs_blob), &p->clear_layout);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create clear() IA layout: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    // clear() always draws to a quad covering the whole viewport
    static const float verts[] = {
        -1, -1,
         1, -1,
         1,  1,
        -1,  1,
        -1, -1,
         1,  1,
    };
    D3D11_BUFFER_DESC vdesc = {
        .ByteWidth = sizeof(verts),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    };
    D3D11_SUBRESOURCE_DATA vdata = {
        .pSysMem = verts,
    };
    hr = ID3D11Device_CreateBuffer(p->dev, &vdesc, &vdata, &p->clear_vbuf);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create clear() vertex buffer: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    D3D11_BUFFER_DESC cdesc = {
        .ByteWidth = sizeof(float[4]),
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };
    hr = ID3D11Device_CreateBuffer(p->dev, &cdesc, NULL, &p->clear_cbuf);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create clear() constant buffer: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    SAFE_RELEASE(vs_blob);
    SAFE_RELEASE(ps_blob);
    return true;
error:
    SAFE_RELEASE(vs_blob);
    SAFE_RELEASE(ps_blob);
    return false;
}

static void clear_rpass(struct ra *ra, struct ra_tex *tex, float color[4],
                        struct mp_rect *rc)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_tex *tex_p = tex->priv;
    struct ra_tex_params *params = &tex->params;

    ID3D11DeviceContext_UpdateSubresource(p->ctx,
        (ID3D11Resource *)p->clear_cbuf, 0, NULL, color, 0, 0);

    ID3D11DeviceContext_IASetInputLayout(p->ctx, p->clear_layout);
    ID3D11DeviceContext_IASetVertexBuffers(p->ctx, 0, 1, &p->clear_vbuf,
        &(UINT) { sizeof(float[2]) }, &(UINT) { 0 });
    ID3D11DeviceContext_IASetPrimitiveTopology(p->ctx,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11DeviceContext_VSSetShader(p->ctx, p->clear_vs, NULL, 0);

    ID3D11DeviceContext_RSSetViewports(p->ctx, 1, (&(D3D11_VIEWPORT) {
        .Width = params->w,
        .Height = params->h,
        .MinDepth = 0,
        .MaxDepth = 1,
    }));
    ID3D11DeviceContext_RSSetScissorRects(p->ctx, 1, (&(D3D11_RECT) {
        .left = rc->x0,
        .top = rc->y0,
        .right = rc->x1,
        .bottom = rc->y1,
    }));
    ID3D11DeviceContext_PSSetShader(p->ctx, p->clear_ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(p->ctx, 0, 1, &p->clear_cbuf);

    ID3D11DeviceContext_OMSetRenderTargets(p->ctx, 1, &tex_p->rtv, NULL);
    ID3D11DeviceContext_OMSetBlendState(p->ctx, NULL, NULL,
                                        D3D11_DEFAULT_SAMPLE_MASK);

    ID3D11DeviceContext_Draw(p->ctx, 6, 0);

    ID3D11DeviceContext_PSSetConstantBuffers(p->ctx, 0, 1,
        &(ID3D11Buffer *){ NULL });
    ID3D11DeviceContext_OMSetRenderTargets(p->ctx, 0, NULL, NULL);
}

static void clear(struct ra *ra, struct ra_tex *tex, float color[4],
                  struct mp_rect *rc)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_tex *tex_p = tex->priv;
    struct ra_tex_params *params = &tex->params;

    if (!tex_p->rtv)
        return;

    if (rc->x0 || rc->y0 || rc->x1 != params->w || rc->y1 != params->h) {
        if (p->has_clear_view) {
            ID3D11DeviceContext1_ClearView(p->ctx1, (ID3D11View *)tex_p->rtv,
                color, (&(D3D11_RECT) {
                    .left = rc->x0,
                    .top = rc->y0,
                    .right = rc->x1,
                    .bottom = rc->y1,
                }), 1);
        } else {
            clear_rpass(ra, tex, color, rc);
        }
    } else {
        ID3D11DeviceContext_ClearRenderTargetView(p->ctx, tex_p->rtv, color);
    }
}

static bool setup_blit_rpass(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    ID3DBlob *vs_blob = NULL;
    ID3DBlob *float_ps_blob = NULL;
    HRESULT hr;

    hr = p->D3DCompile(blit_vs, sizeof(blit_vs), NULL, NULL, NULL, "main",
        get_shader_target(ra, GLSL_SHADER_VERTEX),
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs_blob, NULL);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to compile blit() vertex shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = ID3D11Device_CreateVertexShader(p->dev,
        ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob),
        NULL, &p->blit_vs);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create blit() vertex shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = p->D3DCompile(blit_float_ps, sizeof(blit_float_ps), NULL, NULL, NULL,
        "main", get_shader_target(ra, GLSL_SHADER_FRAGMENT),
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &float_ps_blob, NULL);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to compile blit() pixel shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = ID3D11Device_CreatePixelShader(p->dev,
        ID3D10Blob_GetBufferPointer(float_ps_blob),
        ID3D10Blob_GetBufferSize(float_ps_blob),
        NULL, &p->blit_float_ps);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create blit() pixel shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    D3D11_INPUT_ELEMENT_DESC in_descs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8 },
    };
    hr = ID3D11Device_CreateInputLayout(p->dev, in_descs,
        MP_ARRAY_SIZE(in_descs), ID3D10Blob_GetBufferPointer(vs_blob),
        ID3D10Blob_GetBufferSize(vs_blob), &p->blit_layout);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create blit() IA layout: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    D3D11_BUFFER_DESC vdesc = {
        .ByteWidth = sizeof(struct blit_vert[6]),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    };
    hr = ID3D11Device_CreateBuffer(p->dev, &vdesc, NULL, &p->blit_vbuf);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create blit() vertex buffer: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    // Blit always uses point sampling, regardless of the source texture
    D3D11_SAMPLER_DESC sdesc = {
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .MinLOD = 0,
        .MaxLOD = D3D11_FLOAT32_MAX,
        .MaxAnisotropy = 1,
    };
    hr = ID3D11Device_CreateSamplerState(p->dev, &sdesc, &p->blit_sampler);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create blit() sampler: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    SAFE_RELEASE(vs_blob);
    SAFE_RELEASE(float_ps_blob);
    return true;
error:
    SAFE_RELEASE(vs_blob);
    SAFE_RELEASE(float_ps_blob);
    return false;
}

static void blit_rpass(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                       struct mp_rect *dst_rc, struct mp_rect *src_rc)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_tex *dst_p = dst->priv;
    struct d3d_tex *src_p = src->priv;

    float u_min = (double)src_rc->x0 / src->params.w;
    float u_max = (double)src_rc->x1 / src->params.w;
    float v_min = (double)src_rc->y0 / src->params.h;
    float v_max = (double)src_rc->y1 / src->params.h;

    struct blit_vert verts[6] = {
        { .x = -1, .y = -1, .u = u_min, .v = v_max },
        { .x =  1, .y = -1, .u = u_max, .v = v_max },
        { .x =  1, .y =  1, .u = u_max, .v = v_min },
        { .x = -1, .y =  1, .u = u_min, .v = v_min },
    };
    verts[4] = verts[0];
    verts[5] = verts[2];
    ID3D11DeviceContext_UpdateSubresource(p->ctx,
        (ID3D11Resource *)p->blit_vbuf, 0, NULL, verts, 0, 0);

    ID3D11DeviceContext_IASetInputLayout(p->ctx, p->blit_layout);
    ID3D11DeviceContext_IASetVertexBuffers(p->ctx, 0, 1, &p->blit_vbuf,
        &(UINT) { sizeof(verts[0]) }, &(UINT) { 0 });
    ID3D11DeviceContext_IASetPrimitiveTopology(p->ctx,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11DeviceContext_VSSetShader(p->ctx, p->blit_vs, NULL, 0);

    ID3D11DeviceContext_RSSetViewports(p->ctx, 1, (&(D3D11_VIEWPORT) {
        .TopLeftX = dst_rc->x0,
        .TopLeftY = dst_rc->y0,
        .Width = mp_rect_w(*dst_rc),
        .Height = mp_rect_h(*dst_rc),
        .MinDepth = 0,
        .MaxDepth = 1,
    }));
    ID3D11DeviceContext_RSSetScissorRects(p->ctx, 1, (&(D3D11_RECT) {
        .left = dst_rc->x0,
        .top = dst_rc->y0,
        .right = dst_rc->x1,
        .bottom = dst_rc->y1,
    }));

    ID3D11DeviceContext_PSSetShader(p->ctx, p->blit_float_ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(p->ctx, 0, 1, &src_p->srv);
    ID3D11DeviceContext_PSSetSamplers(p->ctx, 0, 1, &p->blit_sampler);

    ID3D11DeviceContext_OMSetRenderTargets(p->ctx, 1, &dst_p->rtv, NULL);
    ID3D11DeviceContext_OMSetBlendState(p->ctx, NULL, NULL,
                                        D3D11_DEFAULT_SAMPLE_MASK);

    ID3D11DeviceContext_Draw(p->ctx, 6, 0);

    ID3D11DeviceContext_PSSetShaderResources(p->ctx, 0, 1,
        &(ID3D11ShaderResourceView *) { NULL });
    ID3D11DeviceContext_PSSetSamplers(p->ctx, 0, 1,
        &(ID3D11SamplerState *) { NULL });
    ID3D11DeviceContext_OMSetRenderTargets(p->ctx, 0, NULL, NULL);
}

static void blit(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                 struct mp_rect *dst_rc_ptr, struct mp_rect *src_rc_ptr)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_tex *dst_p = dst->priv;
    struct d3d_tex *src_p = src->priv;
    struct mp_rect dst_rc = *dst_rc_ptr;
    struct mp_rect src_rc = *src_rc_ptr;

    assert(dst->params.dimensions == 2);
    assert(src->params.dimensions == 2);

    // A zero-sized target rectangle is a no-op
    if (!mp_rect_w(dst_rc) || !mp_rect_h(dst_rc))
        return;

    // ra.h seems to imply that both dst_rc and src_rc can be flipped, but it's
    // easier for blit_rpass() if only src_rc can be flipped, so unflip dst_rc.
    if (dst_rc.x0 > dst_rc.x1) {
        MPSWAP(int, dst_rc.x0, dst_rc.x1);
        MPSWAP(int, src_rc.x0, src_rc.x1);
    }
    if (dst_rc.y0 > dst_rc.y1) {
        MPSWAP(int, dst_rc.y0, dst_rc.y1);
        MPSWAP(int, src_rc.y0, src_rc.y1);
    }

    // If format conversion, stretching or flipping is required, a renderpass
    // must be used
    if (dst->params.format != src->params.format ||
        mp_rect_w(dst_rc) != mp_rect_w(src_rc) ||
        mp_rect_h(dst_rc) != mp_rect_h(src_rc))
    {
        blit_rpass(ra, dst, src, &dst_rc, &src_rc);
    } else {
        int dst_sr = dst_p->array_slice >= 0 ? dst_p->array_slice : 0;
        int src_sr = src_p->array_slice >= 0 ? src_p->array_slice : 0;
        ID3D11DeviceContext_CopySubresourceRegion(p->ctx, dst_p->res, dst_sr,
            dst_rc.x0, dst_rc.y0, 0, src_p->res, src_sr, (&(D3D11_BOX) {
                .left = src_rc.x0,
                .top = src_rc.y0,
                .front = 0,
                .right = src_rc.x1,
                .bottom = src_rc.y1,
                .back = 1,
            }));
    }
}

static int desc_namespace(enum ra_vartype type)
{
    // Images and SSBOs both use UAV bindings
    if (type == RA_VARTYPE_IMG_W)
        type = RA_VARTYPE_BUF_RW;
    return type;
}

static bool compile_glsl(struct ra *ra, enum glsl_shader type,
                         const char *glsl, ID3DBlob **out)
{
    struct ra_d3d11 *p = ra->priv;
    struct spirv_compiler *spirv = p->spirv;
    void *ta_ctx = talloc_new(NULL);
    crossc_compiler *cross = NULL;
    const char *hlsl = NULL;
    ID3DBlob *errors = NULL;
    bool success = false;
    HRESULT hr;

    int cross_shader_model;
    if (p->fl >= D3D_FEATURE_LEVEL_11_0) {
        cross_shader_model = 50;
    } else if (p->fl >= D3D_FEATURE_LEVEL_10_1) {
        cross_shader_model = 41;
    } else {
        cross_shader_model = 40;
    }

    int64_t start_us = mp_time_us();

    bstr spv_module;
    if (!spirv->fns->compile_glsl(spirv, ta_ctx, type, glsl, &spv_module))
        goto done;

    int64_t shaderc_us = mp_time_us();

    cross = crossc_hlsl_create((uint32_t*)spv_module.start,
                               spv_module.len / sizeof(uint32_t));

    crossc_hlsl_set_shader_model(cross, cross_shader_model);
    crossc_set_flip_vert_y(cross, type == GLSL_SHADER_VERTEX);

    hlsl = crossc_compile(cross);
    if (!hlsl) {
        MP_ERR(ra, "SPIRV-Cross failed: %s\n", crossc_strerror(cross));
        goto done;
    }

    int64_t cross_us = mp_time_us();

    hr = p->D3DCompile(hlsl, strlen(hlsl), NULL, NULL, NULL, "main",
        get_shader_target(ra, type), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, out,
        &errors);
    if (FAILED(hr)) {
        MP_ERR(ra, "D3DCompile failed: %s\n%.*s", mp_HRESULT_to_str(hr),
               (int)ID3D10Blob_GetBufferSize(errors),
               (char*)ID3D10Blob_GetBufferPointer(errors));
        goto done;
    }

    int64_t d3dcompile_us = mp_time_us();

    MP_VERBOSE(ra, "Compiled a %s shader in %lldus\n", shader_type_name(type),
               d3dcompile_us - start_us);
    MP_VERBOSE(ra, "shaderc: %lldus, SPIRV-Cross: %lldus, D3DCompile: %lldus\n",
               shaderc_us - start_us,
               cross_us - shaderc_us,
               d3dcompile_us - cross_us);

    success = true;
done:;
    int level = success ? MSGL_DEBUG : MSGL_ERR;
    MP_MSG(ra, level, "GLSL source:\n");
    mp_log_source(ra->log, level, glsl);
    if (hlsl) {
        MP_MSG(ra, level, "HLSL source:\n");
        mp_log_source(ra->log, level, hlsl);
    }
    SAFE_RELEASE(errors);
    crossc_destroy(cross);
    talloc_free(ta_ctx);
    return success;
}

static void renderpass_destroy(struct ra *ra, struct ra_renderpass *pass)
{
    if (!pass)
        return;
    struct d3d_rpass *pass_p = pass->priv;

    SAFE_RELEASE(pass_p->vs);
    SAFE_RELEASE(pass_p->ps);
    SAFE_RELEASE(pass_p->cs);
    SAFE_RELEASE(pass_p->layout);
    SAFE_RELEASE(pass_p->bstate);
    talloc_free(pass);
}

static D3D11_BLEND map_ra_blend(enum ra_blend blend)
{
    switch (blend) {
    default:
    case RA_BLEND_ZERO:                return D3D11_BLEND_ZERO;
    case RA_BLEND_ONE:                 return D3D11_BLEND_ONE;
    case RA_BLEND_SRC_ALPHA:           return D3D11_BLEND_SRC_ALPHA;
    case RA_BLEND_ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
    };
}

static size_t vbuf_upload(struct ra *ra, void *data, size_t size)
{
    struct ra_d3d11 *p = ra->priv;
    HRESULT hr;

    // Arbitrary size limit in case there is an insane number of vertices
    if (size > 1e9) {
        MP_ERR(ra, "Vertex buffer is too large\n");
        return -1;
    }

    // If the vertex data doesn't fit, realloc the vertex buffer
    if (size > p->vbuf_size) {
        size_t new_size = p->vbuf_size;
        // Arbitrary base size
        if (!new_size)
            new_size = 64 * 1024;
        while (new_size < size)
            new_size *= 2;

        ID3D11Buffer *new_buf;
        D3D11_BUFFER_DESC vbuf_desc = {
            .ByteWidth = new_size,
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };
        hr = ID3D11Device_CreateBuffer(p->dev, &vbuf_desc, NULL, &new_buf);
        if (FAILED(hr)) {
            MP_ERR(ra, "Failed to create vertex buffer: %s\n",
                   mp_HRESULT_to_str(hr));
            return -1;
        }

        SAFE_RELEASE(p->vbuf);
        p->vbuf = new_buf;
        p->vbuf_size = new_size;
        p->vbuf_used = 0;
    }

    bool discard = false;
    size_t offset = p->vbuf_used;
    if (offset + size > p->vbuf_size) {
        // We reached the end of the buffer, so discard and wrap around
        discard = true;
        offset = 0;
    }

    D3D11_MAPPED_SUBRESOURCE map = { 0 };
    hr = ID3D11DeviceContext_Map(p->ctx, (ID3D11Resource *)p->vbuf, 0,
        discard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE,
        0, &map);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to map vertex buffer: %s\n", mp_HRESULT_to_str(hr));
        return -1;
    }

    char *cdata = map.pData;
    memcpy(cdata + offset, data, size);

    ID3D11DeviceContext_Unmap(p->ctx, (ID3D11Resource *)p->vbuf, 0);

    p->vbuf_used = offset + size;
    return offset;
}

static const char cache_magic[4] = "RD11";
static const int cache_version = 2;

struct cache_header {
    char magic[sizeof(cache_magic)];
    int cache_version;
    char compiler[SPIRV_NAME_MAX_LEN];
    int spv_compiler_version;
    uint32_t cross_version;
    struct dll_version d3d_compiler_version;
    int feature_level;
    size_t vert_bytecode_len;
    size_t frag_bytecode_len;
    size_t comp_bytecode_len;
};

static void load_cached_program(struct ra *ra,
                                const struct ra_renderpass_params *params,
                                bstr *vert_bc,
                                bstr *frag_bc,
                                bstr *comp_bc)
{
    struct ra_d3d11 *p = ra->priv;
    struct spirv_compiler *spirv = p->spirv;
    bstr cache = params->cached_program;

    if (cache.len < sizeof(struct cache_header))
        return;

    struct cache_header *header = (struct cache_header *)cache.start;
    cache = bstr_cut(cache, sizeof(*header));

    if (strncmp(header->magic, cache_magic, sizeof(cache_magic)) != 0)
        return;
    if (header->cache_version != cache_version)
        return;
    if (strncmp(header->compiler, spirv->name, sizeof(header->compiler)) != 0)
        return;
    if (header->spv_compiler_version != spirv->compiler_version)
        return;
    if (header->cross_version != crossc_version())
        return;
    if (!dll_version_equal(header->d3d_compiler_version, p->d3d_compiler_ver))
        return;
    if (header->feature_level != p->fl)
        return;

    if (header->vert_bytecode_len && vert_bc) {
        *vert_bc = bstr_splice(cache, 0, header->vert_bytecode_len);
        MP_VERBOSE(ra, "Using cached vertex shader\n");
    }
    cache = bstr_cut(cache, header->vert_bytecode_len);

    if (header->frag_bytecode_len && frag_bc) {
        *frag_bc = bstr_splice(cache, 0, header->frag_bytecode_len);
        MP_VERBOSE(ra, "Using cached fragment shader\n");
    }
    cache = bstr_cut(cache, header->frag_bytecode_len);

    if (header->comp_bytecode_len && comp_bc) {
        *comp_bc = bstr_splice(cache, 0, header->comp_bytecode_len);
        MP_VERBOSE(ra, "Using cached compute shader\n");
    }
    cache = bstr_cut(cache, header->comp_bytecode_len);
}

static void save_cached_program(struct ra *ra, struct ra_renderpass *pass,
                                bstr vert_bc,
                                bstr frag_bc,
                                bstr comp_bc)
{
    struct ra_d3d11 *p = ra->priv;
    struct spirv_compiler *spirv = p->spirv;

    struct cache_header header = {
        .cache_version = cache_version,
        .spv_compiler_version = p->spirv->compiler_version,
        .cross_version = crossc_version(),
        .d3d_compiler_version = p->d3d_compiler_ver,
        .feature_level = p->fl,
        .vert_bytecode_len = vert_bc.len,
        .frag_bytecode_len = frag_bc.len,
        .comp_bytecode_len = comp_bc.len,
    };
    strncpy(header.magic, cache_magic, sizeof(header.magic));
    strncpy(header.compiler, spirv->name, sizeof(header.compiler));

    struct bstr *prog = &pass->params.cached_program;
    bstr_xappend(pass, prog, (bstr){ (char *) &header, sizeof(header) });
    bstr_xappend(pass, prog, vert_bc);
    bstr_xappend(pass, prog, frag_bc);
    bstr_xappend(pass, prog, comp_bc);
}

static struct ra_renderpass *renderpass_create_raster(struct ra *ra,
    struct ra_renderpass *pass, const struct ra_renderpass_params *params)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_rpass *pass_p = pass->priv;
    ID3DBlob *vs_blob = NULL;
    ID3DBlob *ps_blob = NULL;
    HRESULT hr;

    // load_cached_program will load compiled shader bytecode into vert_bc and
    // frag_bc if the cache is valid. If not, vert_bc/frag_bc will remain NULL.
    bstr vert_bc = {0};
    bstr frag_bc = {0};
    load_cached_program(ra, params, &vert_bc, &frag_bc, NULL);

    if (!vert_bc.start) {
        if (!compile_glsl(ra, GLSL_SHADER_VERTEX, params->vertex_shader,
                          &vs_blob))
            goto error;
        vert_bc = (bstr){
            ID3D10Blob_GetBufferPointer(vs_blob),
            ID3D10Blob_GetBufferSize(vs_blob),
        };
    }

    hr = ID3D11Device_CreateVertexShader(p->dev, vert_bc.start, vert_bc.len,
                                         NULL, &pass_p->vs);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create vertex shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    if (!frag_bc.start) {
        if (!compile_glsl(ra, GLSL_SHADER_FRAGMENT, params->frag_shader,
                          &ps_blob))
            goto error;
        frag_bc = (bstr){
            ID3D10Blob_GetBufferPointer(ps_blob),
            ID3D10Blob_GetBufferSize(ps_blob),
        };
    }

    hr = ID3D11Device_CreatePixelShader(p->dev, frag_bc.start, frag_bc.len,
                                        NULL, &pass_p->ps);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create pixel shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    D3D11_INPUT_ELEMENT_DESC *in_descs = talloc_array(pass,
        D3D11_INPUT_ELEMENT_DESC, params->num_vertex_attribs);
    for (int i = 0; i < params->num_vertex_attribs; i++) {
        struct ra_renderpass_input *inp = &params->vertex_attribs[i];

        DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
        switch (inp->type) {
        case RA_VARTYPE_FLOAT:
            switch (inp->dim_v) {
            case 1: fmt = DXGI_FORMAT_R32_FLOAT;          break;
            case 2: fmt = DXGI_FORMAT_R32G32_FLOAT;       break;
            case 3: fmt = DXGI_FORMAT_R32G32B32_FLOAT;    break;
            case 4: fmt = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
            }
            break;
        case RA_VARTYPE_BYTE_UNORM:
            switch (inp->dim_v) {
            case 1: fmt = DXGI_FORMAT_R8_UNORM;       break;
            case 2: fmt = DXGI_FORMAT_R8G8_UNORM;     break;
            // There is no 3-component 8-bit DXGI format
            case 4: fmt = DXGI_FORMAT_R8G8B8A8_UNORM; break;
            }
            break;
        }
        if (fmt == DXGI_FORMAT_UNKNOWN) {
            MP_ERR(ra, "Could not find suitable vertex input format\n");
            goto error;
        }

        in_descs[i] = (D3D11_INPUT_ELEMENT_DESC) {
            // The semantic name doesn't mean much and is just used to verify
            // the input description matches the shader. SPIRV-Cross always
            // uses TEXCOORD, so we should too.
            .SemanticName = "TEXCOORD",
            .SemanticIndex = i,
            .AlignedByteOffset = inp->offset,
            .Format = fmt,
        };
    }

    hr = ID3D11Device_CreateInputLayout(p->dev, in_descs,
        params->num_vertex_attribs, vert_bc.start, vert_bc.len,
        &pass_p->layout);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create IA layout: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }
    talloc_free(in_descs);
    in_descs = NULL;

    D3D11_BLEND_DESC bdesc = {
        .RenderTarget[0] = {
            .BlendEnable = params->enable_blend,
            .SrcBlend = map_ra_blend(params->blend_src_rgb),
            .DestBlend = map_ra_blend(params->blend_dst_rgb),
            .BlendOp = D3D11_BLEND_OP_ADD,
            .SrcBlendAlpha = map_ra_blend(params->blend_src_alpha),
            .DestBlendAlpha = map_ra_blend(params->blend_dst_alpha),
            .BlendOpAlpha = D3D11_BLEND_OP_ADD,
            .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        },
    };
    hr = ID3D11Device_CreateBlendState(p->dev, &bdesc, &pass_p->bstate);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create blend state: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }

    save_cached_program(ra, pass, vert_bc, frag_bc, (bstr){0});

    SAFE_RELEASE(vs_blob);
    SAFE_RELEASE(ps_blob);
    return pass;

error:
    renderpass_destroy(ra, pass);
    SAFE_RELEASE(vs_blob);
    SAFE_RELEASE(ps_blob);
    return NULL;
}

static struct ra_renderpass *renderpass_create_compute(struct ra *ra,
    struct ra_renderpass *pass, const struct ra_renderpass_params *params)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_rpass *pass_p = pass->priv;
    ID3DBlob *cs_blob = NULL;
    HRESULT hr;

    bstr comp_bc = {0};
    load_cached_program(ra, params, NULL, NULL, &comp_bc);

    if (!comp_bc.start) {
        if (!compile_glsl(ra, GLSL_SHADER_COMPUTE, params->compute_shader,
                          &cs_blob))
            goto error;
        comp_bc = (bstr){
            ID3D10Blob_GetBufferPointer(cs_blob),
            ID3D10Blob_GetBufferSize(cs_blob),
        };
    }
    hr = ID3D11Device_CreateComputeShader(p->dev, comp_bc.start, comp_bc.len,
                                          NULL, &pass_p->cs);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create compute shader: %s\n",
               mp_HRESULT_to_str(hr));
        goto error;
    }

    save_cached_program(ra, pass, (bstr){0}, (bstr){0}, comp_bc);

    SAFE_RELEASE(cs_blob);
    return pass;
error:
    renderpass_destroy(ra, pass);
    SAFE_RELEASE(cs_blob);
    return NULL;
}

static struct ra_renderpass *renderpass_create(struct ra *ra,
    const struct ra_renderpass_params *params)
{
    struct ra_renderpass *pass = talloc_zero(NULL, struct ra_renderpass);
    pass->params = *ra_renderpass_params_copy(pass, params);
    pass->params.cached_program = (bstr){0};
    pass->priv = talloc_zero(pass, struct d3d_rpass);

    if (params->type == RA_RENDERPASS_TYPE_COMPUTE) {
        return renderpass_create_compute(ra, pass, params);
    } else {
        return renderpass_create_raster(ra, pass, params);
    }
}

static void renderpass_run_raster(struct ra *ra,
                                  const struct ra_renderpass_run_params *params,
                                  ID3D11Buffer *ubos[], int ubos_len,
                                  ID3D11SamplerState *samplers[],
                                  ID3D11ShaderResourceView *srvs[],
                                  int samplers_len,
                                  ID3D11UnorderedAccessView *uavs[],
                                  int uavs_len)
{
    struct ra_d3d11 *p = ra->priv;
    struct ra_renderpass *pass = params->pass;
    struct d3d_rpass *pass_p = pass->priv;

    UINT vbuf_offset = vbuf_upload(ra, params->vertex_data,
        pass->params.vertex_stride * params->vertex_count);
    if (vbuf_offset == (UINT)-1)
        return;

    ID3D11DeviceContext_IASetInputLayout(p->ctx, pass_p->layout);
    ID3D11DeviceContext_IASetVertexBuffers(p->ctx, 0, 1, &p->vbuf,
        &pass->params.vertex_stride, &vbuf_offset);
    ID3D11DeviceContext_IASetPrimitiveTopology(p->ctx,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11DeviceContext_VSSetShader(p->ctx, pass_p->vs, NULL, 0);

    ID3D11DeviceContext_RSSetViewports(p->ctx, 1, (&(D3D11_VIEWPORT) {
        .TopLeftX = params->viewport.x0,
        .TopLeftY = params->viewport.y0,
        .Width = mp_rect_w(params->viewport),
        .Height = mp_rect_h(params->viewport),
        .MinDepth = 0,
        .MaxDepth = 1,
    }));
    ID3D11DeviceContext_RSSetScissorRects(p->ctx, 1, (&(D3D11_RECT) {
        .left = params->scissors.x0,
        .top = params->scissors.y0,
        .right = params->scissors.x1,
        .bottom = params->scissors.y1,
    }));
    ID3D11DeviceContext_PSSetShader(p->ctx, pass_p->ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(p->ctx, 0, ubos_len, ubos);
    ID3D11DeviceContext_PSSetShaderResources(p->ctx, 0, samplers_len, srvs);
    ID3D11DeviceContext_PSSetSamplers(p->ctx, 0, samplers_len, samplers);

    struct ra_tex *target = params->target;
    struct d3d_tex *target_p = target->priv;
    ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(p->ctx, 1,
        &target_p->rtv, NULL, 1, uavs_len, uavs, NULL);
    ID3D11DeviceContext_OMSetBlendState(p->ctx, pass_p->bstate, NULL,
                                        D3D11_DEFAULT_SAMPLE_MASK);

    ID3D11DeviceContext_Draw(p->ctx, params->vertex_count, 0);

    // Unbind everything. It's easier to do this than to actually track state,
    // and if we leave the RTV bound, it could trip up D3D's conflict checker.
    for (int i = 0; i < ubos_len; i++)
        ubos[i] = NULL;
    for (int i = 0; i < samplers_len; i++) {
        samplers[i] = NULL;
        srvs[i] = NULL;
    }
    for (int i = 0; i < uavs_len; i++)
        uavs[i] = NULL;
    ID3D11DeviceContext_PSSetConstantBuffers(p->ctx, 0, ubos_len, ubos);
    ID3D11DeviceContext_PSSetShaderResources(p->ctx, 0, samplers_len, srvs);
    ID3D11DeviceContext_PSSetSamplers(p->ctx, 0, samplers_len, samplers);
    ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(p->ctx, 0,
        NULL, NULL, 1, uavs_len, uavs, NULL);
}

static void renderpass_run_compute(struct ra *ra,
                                   const struct ra_renderpass_run_params *params,
                                   ID3D11Buffer *ubos[], int ubos_len,
                                   ID3D11SamplerState *samplers[],
                                   ID3D11ShaderResourceView *srvs[],
                                   int samplers_len,
                                   ID3D11UnorderedAccessView *uavs[],
                                   int uavs_len)
{
    struct ra_d3d11 *p = ra->priv;
    struct ra_renderpass *pass = params->pass;
    struct d3d_rpass *pass_p = pass->priv;

    ID3D11DeviceContext_CSSetShader(p->ctx, pass_p->cs, NULL, 0);
    ID3D11DeviceContext_CSSetConstantBuffers(p->ctx, 0, ubos_len, ubos);
    ID3D11DeviceContext_CSSetShaderResources(p->ctx, 0, samplers_len, srvs);
    ID3D11DeviceContext_CSSetSamplers(p->ctx, 0, samplers_len, samplers);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(p->ctx, 0, uavs_len, uavs,
                                                  NULL);

    ID3D11DeviceContext_Dispatch(p->ctx, params->compute_groups[0],
                                         params->compute_groups[1],
                                         params->compute_groups[2]);

    for (int i = 0; i < ubos_len; i++)
        ubos[i] = NULL;
    for (int i = 0; i < samplers_len; i++) {
        samplers[i] = NULL;
        srvs[i] = NULL;
    }
    for (int i = 0; i < uavs_len; i++)
        uavs[i] = NULL;
    ID3D11DeviceContext_CSSetConstantBuffers(p->ctx, 0, ubos_len, ubos);
    ID3D11DeviceContext_CSSetShaderResources(p->ctx, 0, samplers_len, srvs);
    ID3D11DeviceContext_CSSetSamplers(p->ctx, 0, samplers_len, samplers);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(p->ctx, 0, uavs_len, uavs,
                                                  NULL);
}

static void renderpass_run(struct ra *ra,
                           const struct ra_renderpass_run_params *params)
{
    struct ra_d3d11 *p = ra->priv;
    struct ra_renderpass *pass = params->pass;
    enum ra_renderpass_type type = pass->params.type;

    ID3D11Buffer *ubos[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    int ubos_len = 0;

    ID3D11SamplerState *samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    ID3D11ShaderResourceView *srvs[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    int samplers_len = 0;

    ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {0};
    int uavs_len = 0;

    // In a raster pass, one of the UAV slots is used by the runtime for the RTV
    int uavs_max = type == RA_RENDERPASS_TYPE_COMPUTE ? p->max_uavs
                                                      : p->max_uavs - 1;

    // Gather the input variables used in this pass. These will be mapped to
    // HLSL registers.
    for (int i = 0; i < params->num_values; i++) {
        struct ra_renderpass_input_val *val = &params->values[i];
        int binding = pass->params.inputs[val->index].binding;
        switch (pass->params.inputs[val->index].type) {
        case RA_VARTYPE_BUF_RO:
            if (binding > MP_ARRAY_SIZE(ubos)) {
                MP_ERR(ra, "Too many constant buffers in pass\n");
                return;
            }
            struct ra_buf *buf_ro = *(struct ra_buf **)val->data;
            buf_resolve(ra, buf_ro);
            struct d3d_buf *buf_ro_p = buf_ro->priv;
            ubos[binding] = buf_ro_p->buf;
            ubos_len = MPMAX(ubos_len, binding + 1);
            break;
        case RA_VARTYPE_BUF_RW:
            if (binding > uavs_max) {
                MP_ERR(ra, "Too many UAVs in pass\n");
                return;
            }
            struct ra_buf *buf_rw = *(struct ra_buf **)val->data;
            buf_resolve(ra, buf_rw);
            struct d3d_buf *buf_rw_p = buf_rw->priv;
            uavs[binding] = buf_rw_p->uav;
            uavs_len = MPMAX(uavs_len, binding + 1);
            break;
        case RA_VARTYPE_TEX:
            if (binding > MP_ARRAY_SIZE(samplers)) {
                MP_ERR(ra, "Too many textures in pass\n");
                return;
            }
            struct ra_tex *tex = *(struct ra_tex **)val->data;
            struct d3d_tex *tex_p = tex->priv;
            samplers[binding] = tex_p->sampler;
            srvs[binding] = tex_p->srv;
            samplers_len = MPMAX(samplers_len, binding + 1);
            break;
        case RA_VARTYPE_IMG_W:
            if (binding > uavs_max) {
                MP_ERR(ra, "Too many UAVs in pass\n");
                return;
            }
            struct ra_tex *img = *(struct ra_tex **)val->data;
            struct d3d_tex *img_p = img->priv;
            uavs[binding] = img_p->uav;
            uavs_len = MPMAX(uavs_len, binding + 1);
            break;
        }
    }

    if (type == RA_RENDERPASS_TYPE_COMPUTE) {
        renderpass_run_compute(ra, params, ubos, ubos_len, samplers, srvs,
                               samplers_len, uavs, uavs_len);
    } else {
        renderpass_run_raster(ra, params, ubos, ubos_len, samplers, srvs,
                              samplers_len, uavs, uavs_len);
    }
}

static void timer_destroy(struct ra *ra, ra_timer *ratimer)
{
    if (!ratimer)
        return;
    struct d3d_timer *timer = ratimer;

    SAFE_RELEASE(timer->ts_start);
    SAFE_RELEASE(timer->ts_end);
    SAFE_RELEASE(timer->disjoint);
    talloc_free(timer);
}

static ra_timer *timer_create(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    if (!p->has_timestamp_queries)
        return NULL;

    struct d3d_timer *timer = talloc_zero(NULL, struct d3d_timer);
    HRESULT hr;

    hr = ID3D11Device_CreateQuery(p->dev,
        &(D3D11_QUERY_DESC) { D3D11_QUERY_TIMESTAMP }, &timer->ts_start);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create start query: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }

    hr = ID3D11Device_CreateQuery(p->dev,
        &(D3D11_QUERY_DESC) { D3D11_QUERY_TIMESTAMP }, &timer->ts_end);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create end query: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }

    // Measuring duration in D3D11 requires three queries: start and end
    // timestamps, and a disjoint query containing a flag which says whether
    // the timestamps are usable or if a discontinuity occured between them,
    // like a change in power state or clock speed. The disjoint query also
    // contains the timer frequency, so the timestamps are useless without it.
    hr = ID3D11Device_CreateQuery(p->dev,
        &(D3D11_QUERY_DESC) { D3D11_QUERY_TIMESTAMP_DISJOINT }, &timer->disjoint);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create timer query: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }

    return timer;
error:
    timer_destroy(ra, timer);
    return NULL;
}

static uint64_t timestamp_to_ns(uint64_t timestamp, uint64_t freq)
{
    static const uint64_t ns_per_s = 1000000000llu;
    return timestamp / freq * ns_per_s + timestamp % freq * ns_per_s / freq;
}

static uint64_t timer_get_result(struct ra *ra, ra_timer *ratimer)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_timer *timer = ratimer;
    HRESULT hr;

    UINT64 start, end;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;

    hr = ID3D11DeviceContext_GetData(p->ctx,
        (ID3D11Asynchronous *)timer->ts_end, &end, sizeof(end),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (FAILED(hr) || hr == S_FALSE)
        return 0;
    hr = ID3D11DeviceContext_GetData(p->ctx,
        (ID3D11Asynchronous *)timer->ts_start, &start, sizeof(start),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (FAILED(hr) || hr == S_FALSE)
        return 0;
    hr = ID3D11DeviceContext_GetData(p->ctx,
        (ID3D11Asynchronous *)timer->disjoint, &dj, sizeof(dj),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (FAILED(hr) || hr == S_FALSE || dj.Disjoint || !dj.Frequency)
        return 0;

    return timestamp_to_ns(end - start, dj.Frequency);
}

static void timer_start(struct ra *ra, ra_timer *ratimer)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_timer *timer = ratimer;

    // Latch the last result of this ra_timer (returned by timer_stop)
    timer->result = timer_get_result(ra, ratimer);

    ID3D11DeviceContext_Begin(p->ctx, (ID3D11Asynchronous *)timer->disjoint);
    ID3D11DeviceContext_End(p->ctx, (ID3D11Asynchronous *)timer->ts_start);
}

static uint64_t timer_stop(struct ra *ra, ra_timer *ratimer)
{
    struct ra_d3d11 *p = ra->priv;
    struct d3d_timer *timer = ratimer;

    ID3D11DeviceContext_End(p->ctx, (ID3D11Asynchronous *)timer->ts_end);
    ID3D11DeviceContext_End(p->ctx, (ID3D11Asynchronous *)timer->disjoint);

    return timer->result;
}

static int map_msg_severity(D3D11_MESSAGE_SEVERITY sev)
{
    switch (sev) {
    case D3D11_MESSAGE_SEVERITY_CORRUPTION:
        return MSGL_FATAL;
    case D3D11_MESSAGE_SEVERITY_ERROR:
        return MSGL_ERR;
    case D3D11_MESSAGE_SEVERITY_WARNING:
        return MSGL_WARN;
    default:
    case D3D11_MESSAGE_SEVERITY_INFO:
    case D3D11_MESSAGE_SEVERITY_MESSAGE:
        return MSGL_DEBUG;
    }
}

static void debug_marker(struct ra *ra, const char *msg)
{
    struct ra_d3d11 *p = ra->priv;
    void *talloc_ctx = talloc_new(NULL);
    HRESULT hr;

    if (!p->iqueue)
        goto done;

    // Copy debug-layer messages to mpv's log output
    bool printed_header = false;
    uint64_t messages = ID3D11InfoQueue_GetNumStoredMessages(p->iqueue);
    for (uint64_t i = 0; i < messages; i++) {
        size_t len;
        hr = ID3D11InfoQueue_GetMessage(p->iqueue, i, NULL, &len);
        if (FAILED(hr) || !len)
            goto done;

        D3D11_MESSAGE *d3dmsg = talloc_size(talloc_ctx, len);
        hr = ID3D11InfoQueue_GetMessage(p->iqueue, i, d3dmsg, &len);
        if (FAILED(hr))
            goto done;

        int msgl = map_msg_severity(d3dmsg->Severity);
        if (mp_msg_test(ra->log, msgl)) {
            if (!printed_header)
                MP_INFO(ra, "%s:\n", msg);
            printed_header = true;

            MP_MSG(ra, msgl, "%d: %.*s\n", (int)d3dmsg->ID,
                (int)d3dmsg->DescriptionByteLength, d3dmsg->pDescription);
            talloc_free(d3dmsg);
        }
    }

    ID3D11InfoQueue_ClearStoredMessages(p->iqueue);
done:
    talloc_free(talloc_ctx);
}

static void destroy(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;

    // Release everything except the interfaces needed to perform leak checking
    SAFE_RELEASE(p->clear_ps);
    SAFE_RELEASE(p->clear_vs);
    SAFE_RELEASE(p->clear_layout);
    SAFE_RELEASE(p->clear_vbuf);
    SAFE_RELEASE(p->clear_cbuf);
    SAFE_RELEASE(p->blit_float_ps);
    SAFE_RELEASE(p->blit_vs);
    SAFE_RELEASE(p->blit_layout);
    SAFE_RELEASE(p->blit_vbuf);
    SAFE_RELEASE(p->blit_sampler);
    SAFE_RELEASE(p->vbuf);
    SAFE_RELEASE(p->ctx1);
    SAFE_RELEASE(p->dev1);
    SAFE_RELEASE(p->dev);

    if (p->debug && p->ctx) {
        // Destroy the device context synchronously so referenced objects don't
        // show up in the leak check
        ID3D11DeviceContext_ClearState(p->ctx);
        ID3D11DeviceContext_Flush(p->ctx);
    }
    SAFE_RELEASE(p->ctx);

    if (p->debug) {
        // Report any leaked objects
        debug_marker(ra, "after destroy");
        ID3D11Debug_ReportLiveDeviceObjects(p->debug, D3D11_RLDO_DETAIL);
        debug_marker(ra, "after leak check");
        ID3D11Debug_ReportLiveDeviceObjects(p->debug, D3D11_RLDO_SUMMARY);
        debug_marker(ra, "after leak summary");
    }
    SAFE_RELEASE(p->debug);
    SAFE_RELEASE(p->iqueue);

    talloc_free(ra);
}

static struct ra_fns ra_fns_d3d11 = {
    .destroy            = destroy,
    .tex_create         = tex_create,
    .tex_destroy        = tex_destroy,
    .tex_upload         = tex_upload,
    .tex_download       = tex_download,
    .buf_create         = buf_create,
    .buf_destroy        = buf_destroy,
    .buf_update         = buf_update,
    .clear              = clear,
    .blit               = blit,
    .uniform_layout     = std140_layout,
    .desc_namespace     = desc_namespace,
    .renderpass_create  = renderpass_create,
    .renderpass_destroy = renderpass_destroy,
    .renderpass_run     = renderpass_run,
    .timer_create       = timer_create,
    .timer_destroy      = timer_destroy,
    .timer_start        = timer_start,
    .timer_stop         = timer_stop,
    .debug_marker       = debug_marker,
};

void ra_d3d11_flush(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    ID3D11DeviceContext_Flush(p->ctx);
}

static void init_debug_layer(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    HRESULT hr;

    hr = ID3D11Device_QueryInterface(p->dev, &IID_ID3D11Debug,
                                     (void**)&p->debug);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to get debug device: %s\n", mp_HRESULT_to_str(hr));
        return;
    }

    hr = ID3D11Device_QueryInterface(p->dev, &IID_ID3D11InfoQueue,
                                     (void**)&p->iqueue);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to get info queue: %s\n", mp_HRESULT_to_str(hr));
        return;
    }

    // Store an unlimited amount of messages in the buffer. This is fine
    // because we flush stored messages regularly (in debug_marker.)
    ID3D11InfoQueue_SetMessageCountLimit(p->iqueue, -1);

    // Filter some annoying messages
    D3D11_MESSAGE_ID deny_ids[] = {
        // This error occurs during context creation when we try to figure out
        // the real maximum texture size by attempting to create a texture
        // larger than the current feature level allows.
        D3D11_MESSAGE_ID_CREATETEXTURE2D_INVALIDDIMENSIONS,

        // These are normal. The RA timer queue habitually reuses timer objects
        // without retrieving the results.
        D3D11_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
        D3D11_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,
    };
    D3D11_INFO_QUEUE_FILTER filter = {
        .DenyList = {
            .NumIDs = MP_ARRAY_SIZE(deny_ids),
            .pIDList = deny_ids,
        },
    };
    ID3D11InfoQueue_PushStorageFilter(p->iqueue, &filter);
}

static struct dll_version get_dll_version(HMODULE dll)
{
    void *ctx = talloc_new(NULL);
    struct dll_version ret = { 0 };

    HRSRC rsrc = FindResourceW(dll, MAKEINTRESOURCEW(VS_VERSION_INFO),
                               MAKEINTRESOURCEW(VS_FILE_INFO));
    if (!rsrc)
        goto done;
    DWORD size = SizeofResource(dll, rsrc);
    HGLOBAL res = LoadResource(dll, rsrc);
    if (!res)
        goto done;
    void *ptr = LockResource(res);
    if (!ptr)
        goto done;
    void *copy = talloc_memdup(ctx, ptr, size);

    VS_FIXEDFILEINFO *ffi;
    UINT ffi_len;
    if (!VerQueryValueW(copy, L"\\", (void**)&ffi, &ffi_len))
        goto done;
    if (ffi_len < sizeof(*ffi))
        goto done;

    ret.major = HIWORD(ffi->dwFileVersionMS);
    ret.minor = LOWORD(ffi->dwFileVersionMS);
    ret.build = HIWORD(ffi->dwFileVersionLS);
    ret.revision = LOWORD(ffi->dwFileVersionLS);

done:
    talloc_free(ctx);
    return ret;
}

static bool load_d3d_compiler(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    HMODULE d3dcompiler = NULL;

    // Try the inbox D3DCompiler first (Windows 8.1 and up)
    if (IsWindows8Point1OrGreater()) {
        d3dcompiler = LoadLibraryExW(L"d3dcompiler_47.dll", NULL,
                                     LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    // Check for a packaged version of d3dcompiler_47.dll
    if (!d3dcompiler)
        d3dcompiler = LoadLibraryW(L"d3dcompiler_47.dll");
    // Try d3dcompiler_46.dll from the Windows 8 SDK
    if (!d3dcompiler)
        d3dcompiler = LoadLibraryW(L"d3dcompiler_46.dll");
    // Try d3dcompiler_43.dll from the June 2010 DirectX SDK
    if (!d3dcompiler)
        d3dcompiler = LoadLibraryW(L"d3dcompiler_43.dll");
    // Can't find any compiler DLL, so give up
    if (!d3dcompiler)
        return false;

    p->d3d_compiler_ver = get_dll_version(d3dcompiler);

    p->D3DCompile = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");
    if (!p->D3DCompile)
        return false;
    return true;
}

static void find_max_texture_dimension(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;

    D3D11_TEXTURE2D_DESC desc = {
        .Width = ra->max_texture_wh,
        .Height = ra->max_texture_wh,
        .MipLevels = 1,
        .ArraySize = 1,
        .SampleDesc.Count = 1,
        .Format = DXGI_FORMAT_R8_UNORM,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    while (true) {
        desc.Height = desc.Width *= 2;
        if (desc.Width >= 0x8000000u)
            return;
        if (FAILED(ID3D11Device_CreateTexture2D(p->dev, &desc, NULL, NULL)))
            return;
        ra->max_texture_wh = desc.Width;
    }
}

struct ra *ra_d3d11_create(ID3D11Device *dev, struct mp_log *log,
                           struct spirv_compiler *spirv)
{
    HRESULT hr;

    struct ra *ra = talloc_zero(NULL, struct ra);
    ra->log = log;
    ra->fns = &ra_fns_d3d11;

    // Even Direct3D 10level9 supports 3D textures
    ra->caps = RA_CAP_TEX_3D | RA_CAP_DIRECT_UPLOAD | RA_CAP_BUF_RO |
               RA_CAP_BLIT | spirv->ra_caps;

    ra->glsl_version = spirv->glsl_version;
    ra->glsl_vulkan = true;

    struct ra_d3d11 *p = ra->priv = talloc_zero(ra, struct ra_d3d11);
    p->spirv = spirv;

    int minor = 0;
    ID3D11Device_AddRef(dev);
    p->dev = dev;
    ID3D11Device_GetImmediateContext(p->dev, &p->ctx);
    hr = ID3D11Device_QueryInterface(p->dev, &IID_ID3D11Device1,
                                     (void**)&p->dev1);
    if (SUCCEEDED(hr)) {
        minor = 1;
        ID3D11Device1_GetImmediateContext1(p->dev1, &p->ctx1);

        D3D11_FEATURE_DATA_D3D11_OPTIONS fopts = { 0 };
        hr = ID3D11Device_CheckFeatureSupport(p->dev,
            D3D11_FEATURE_D3D11_OPTIONS, &fopts, sizeof(fopts));
        if (SUCCEEDED(hr)) {
            p->has_clear_view = fopts.ClearView;
        }
    }

    MP_VERBOSE(ra, "Using Direct3D 11.%d runtime\n", minor);

    p->fl = ID3D11Device_GetFeatureLevel(p->dev);
    if (p->fl >= D3D_FEATURE_LEVEL_11_0) {
        ra->max_texture_wh = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    } else if (p->fl >= D3D_FEATURE_LEVEL_10_0) {
        ra->max_texture_wh = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    } else if (p->fl >= D3D_FEATURE_LEVEL_9_3) {
        ra->max_texture_wh = D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    } else {
        ra->max_texture_wh = D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    }

    if (p->fl >= D3D_FEATURE_LEVEL_11_0)
        ra->caps |= RA_CAP_GATHER;
    if (p->fl >= D3D_FEATURE_LEVEL_10_0)
        ra->caps |= RA_CAP_FRAGCOORD;

    // Some 10_0 hardware has compute shaders, but only 11_0 has image load/store
    if (p->fl >= D3D_FEATURE_LEVEL_11_0) {
        ra->caps |= RA_CAP_COMPUTE | RA_CAP_BUF_RW;
        ra->max_shmem = 32 * 1024;
    }

    if (p->fl >= D3D_FEATURE_LEVEL_11_1) {
        p->max_uavs = D3D11_1_UAV_SLOT_COUNT;
    } else {
        p->max_uavs = D3D11_PS_CS_UAV_REGISTER_COUNT;
    }

    if (ID3D11Device_GetCreationFlags(p->dev) & D3D11_CREATE_DEVICE_DEBUG)
        init_debug_layer(ra);

    // Some level 9_x devices don't have timestamp queries
    hr = ID3D11Device_CreateQuery(p->dev,
        &(D3D11_QUERY_DESC) { D3D11_QUERY_TIMESTAMP }, NULL);
    p->has_timestamp_queries = SUCCEEDED(hr);

    // According to MSDN, the above texture sizes are just minimums and drivers
    // may support larger textures. See:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476874.aspx
    find_max_texture_dimension(ra);
    MP_VERBOSE(ra, "Maximum Texture2D size: %dx%d\n", ra->max_texture_wh,
               ra->max_texture_wh);

    if (!load_d3d_compiler(ra)) {
        MP_FATAL(ra, "Could not find D3DCompiler DLL\n");
        goto error;
    }

    MP_VERBOSE(ra, "D3DCompiler version: %u.%u.%u.%u\n",
               p->d3d_compiler_ver.major, p->d3d_compiler_ver.minor,
               p->d3d_compiler_ver.build, p->d3d_compiler_ver.revision);

    setup_formats(ra);

    // The rasterizer state never changes, so set it up here
    ID3D11RasterizerState *rstate;
    D3D11_RASTERIZER_DESC rdesc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .FrontCounterClockwise = FALSE,
        .DepthClipEnable = TRUE, // Required for 10level9
        .ScissorEnable = TRUE,
    };
    hr = ID3D11Device_CreateRasterizerState(p->dev, &rdesc, &rstate);
    if (FAILED(hr)) {
        MP_ERR(ra, "Failed to create rasterizer state: %s\n", mp_HRESULT_to_str(hr));
        goto error;
    }
    ID3D11DeviceContext_RSSetState(p->ctx, rstate);
    SAFE_RELEASE(rstate);

    // If the device doesn't support ClearView, we have to set up a
    // shader-based clear() implementation
    if (!p->has_clear_view && !setup_clear_rpass(ra))
        goto error;

    if (!setup_blit_rpass(ra))
        goto error;

    return ra;

error:
    destroy(ra);
    return NULL;
}

ID3D11Device *ra_d3d11_get_device(struct ra *ra)
{
    struct ra_d3d11 *p = ra->priv;
    ID3D11Device_AddRef(p->dev);
    return p->dev;
}

bool ra_is_d3d11(struct ra *ra)
{
    return ra->fns == &ra_fns_d3d11;
}
