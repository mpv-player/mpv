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

#include <stdatomic.h>

#include <windows.h>
#include <d3d11.h>

#include <AMF/components/FRC.h>
#include <AMF/components/VQEnhancer.h>
#include <AMF/core/Context.h>
#include <AMF/core/Factory.h>
#include <AMF/core/Surface.h>

#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/hwcontext.h>

#include "filters/filter_internal.h"
#include "filters/filter.h"
#include "filters/user_filters.h"
#include "osdep/windows_utils.h"
#include "video/hwdec.h"
#include "video/mp_image.h"

#define ICALL(func, that, ...) (that)->pVtbl->func((that), ##__VA_ARGS__)

// Refcounting helpers
typedef atomic_uint_fast32_t mp_rc_t;
#define mp_rc_init(rc)  atomic_init(rc, 1)
#define mp_rc_ref(rc)   ((void) atomic_fetch_add_explicit(rc, 1, memory_order_acquire))
#define mp_rc_deref(rc) (atomic_fetch_sub_explicit(rc, 1, memory_order_release) == 1)
#define mp_rc_count(rc)  atomic_load(rc)

// Shared AMF runtime, context and component. Each AMF filter instantiates its
// own, but the surrounding scaffolding is identical.
struct amf_priv {
    mp_rc_t ref_count;
    HMODULE dll;
    AMFFactory *factory;
    AMFContext *ctx;
    AMFComponent *comp;
};

struct priv {
    void *opts;

    struct mp_image_params params;
    struct mp_image *ref_image;

    ID3D11Device *d3d_dev;
    ID3D11Texture2D *input_tex;

    // Set when the component still has a buffered output frame to emit without
    // submitting new input. Only the rate-changing amf_frc filter uses this.
    bool has_frame;

    AVBufferRef *av_device_ref;

    struct amf_priv *amf;
};

// Per-filter (re)initialization callback, called with the actual input texture
// once the first frame is available. Signature shared by amf_submit_image().
typedef int (*amf_init_fn)(struct mp_filter *f, struct mp_image *src, bool reinit);

static inline AMF_SURFACE_FORMAT imgfmt2amf(int fmt)
{
    switch (fmt) {
        case IMGFMT_NV12:
            return AMF_SURFACE_NV12;
        case IMGFMT_BGRA:
            return AMF_SURFACE_BGRA;
        case IMGFMT_RGBA:
            return AMF_SURFACE_RGBA;
        case IMGFMT_P010:
            return AMF_SURFACE_P010;
    default:
        return AMF_SURFACE_UNKNOWN;
    }
}

static void unload_amf_library(struct amf_priv *a)
{
    mp_assert(!a->comp && !a->ctx);
    if (a->dll) {
        FreeLibrary(a->dll);
        a->dll = NULL;
    }
    a->factory = NULL;
}

static int load_amf_library(struct amf_priv *a)
{
    a->dll = LoadLibraryW(AMF_DLL_NAME);
    if (!a->dll)
        return -1;

    AMFInit_Fn init_fn = (AMFInit_Fn)GetProcAddress(a->dll, AMF_INIT_FUNCTION_NAME);
    if (!init_fn)
        goto err;

    AMF_RESULT res = init_fn(AMF_FULL_VERSION, &a->factory);
    if (res != AMF_OK)
        goto err;

    return 0;

err:
    unload_amf_library(a);
    return -1;
}

static void flush(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (p->amf->comp)
        ICALL(Flush, p->amf->comp);
    p->has_frame = false;
    mp_image_unrefp(&p->ref_image);
}

static void destroy_amf(struct amf_priv *a)
{
    if (!mp_rc_deref(&a->ref_count))
        return;

    if (a->comp) {
        ICALL(Release, a->comp);
        a->comp = NULL;
    }

    if (a->ctx) {
        ICALL(Release, a->ctx);
        a->ctx = NULL;
    }

    unload_amf_library(a);
    talloc_free(a);
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    flush(f);

    if (a->comp)
        ICALL(Terminate, a->comp);

    if (a->ctx)
        ICALL(Terminate, a->ctx);

    SAFE_RELEASE(p->input_tex);
    SAFE_RELEASE(p->d3d_dev);
    av_buffer_unref(&p->av_device_ref);

    destroy_amf(a);
}

static int create_amf_context(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    AMF_RESULT res = ICALL(CreateContext, a->factory, &a->ctx);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to create AMF context: %d\n", res);
        goto err;
    }

    struct mp_stream_info *info = mp_filter_find_stream_info(f);
    if (!info || !info->hwdec_devs) {
        MP_ERR(f, "No hwdec devices available for AMF initialization.\n");
        goto err;
    }

    struct hwdec_imgfmt_request params = {
        .imgfmt = IMGFMT_D3D11,
        .probing = false,
    };
    hwdec_devices_request_for_img_fmt(info->hwdec_devs, &params);
    struct mp_hwdec_ctx *hwctx =
        hwdec_devices_get_by_imgfmt_and_type(info->hwdec_devs, IMGFMT_D3D11,
                                             AV_HWDEVICE_TYPE_D3D11VA);
    if (!hwctx || !hwctx->av_device_ref) {
        MP_ERR(f, "No suitable D3D11 hwdec device found for AMF initialization.\n");
        goto err;
    }

    p->av_device_ref = av_buffer_ref(hwctx->av_device_ref);
    MP_HANDLE_OOM(p->av_device_ref);
    AVHWDeviceContext *avhwctx = (void *)p->av_device_ref->data;
    AVD3D11VADeviceContext *d3dctx = avhwctx->hwctx;
    ID3D11Device_QueryInterface(d3dctx->device, &IID_ID3D11Device, (void **)&p->d3d_dev);

    // AMF uses its own separate D3D11 device. This avoids any conflicts with
    // with our presentation device, as we run it on different thread. Things
    // work well, until there is compute shader processing done on the same device.
    res = ICALL(InitDX11, a->ctx, NULL, AMF_DX11_1);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to initialize AMF context with D3D11: %d\n", res);
        goto err;
    }

    return 0;

err:
    // Cleanup is left to the caller's talloc_free(), which runs destroy() once
    // via the filter destructor. Calling destroy() here as well would free the
    // amf_priv twice.
    return -1;
}

static int create_amf_component(struct mp_filter *f, const wchar_t *name)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    mp_assert(!a->comp);
    AMF_RESULT res = ICALL(CreateComponent, a->factory, a->ctx, name, &a->comp);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to create AMF component: %d\n", res);
        return -1;
    }

    return 0;
}

// Validate the input and derive the AMF surface format and the texture size the
// component must be initialized with. Shared by the AMF filters.
static int amf_get_init_params(struct mp_filter *f, struct mp_image *src,
                               AMF_SURFACE_FORMAT *out_fmt, int *out_w, int *out_h)
{
    struct priv *p = f->priv;

    // Support only D3D11 input, while it's possible to input host memory, this
    // adds unnecessary complexity. We already have host memory upload filters,
    // that can be used, before calling this filter. Also using host memory here,
    // would return the processed frames also in host memory, which again forces
    // us to upload them back to GPU for presentation, duplicating work.
    if (p->params.imgfmt != IMGFMT_D3D11) {
        MP_ERR(f, "Input must be a D3D11 texture. "
                  "Use `--hwdec=d3d11va` or `--vf-pre=format=d3d11` to upload the data.\n");
        return -1;
    }

    AMF_SURFACE_FORMAT amf_fmt = imgfmt2amf(p->params.hw_subfmt);
    if (amf_fmt == AMF_SURFACE_UNKNOWN) {
        MP_ERR(f, "Unsupported input format: %s\n",
                   mp_imgfmt_to_name(p->params.hw_subfmt));
        return -1;
    }

    // AMF components expect the texture size here, not the coded image size.
    D3D11_TEXTURE2D_DESC desc = {0};
    ID3D11Texture2D_GetDesc((ID3D11Texture2D *)src->planes[0], &desc);

    *out_fmt = amf_fmt;
    *out_w = desc.Width;
    *out_h = desc.Height;
    return 0;
}

// Initialize the component for the given format and size. Any properties must be
// set before calling this.
static int amf_component_init(struct mp_filter *f, AMF_SURFACE_FORMAT fmt,
                              int width, int height)
{
    struct priv *p = f->priv;

    AMF_RESULT res = ICALL(Init, p->amf->comp, fmt, width, height);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to initialize AMF component: %d\n", res);
        return -1;
    }

    SAFE_RELEASE(p->input_tex);
    return 0;
}

struct amf_surface_free_ctx {
    struct amf_priv *a;
    AMFSurface *surface;
    ID3D11Texture2D *tex;
};

static void free_amf_surface(void *arg)
{
    struct amf_surface_free_ctx *ctx = arg;
    ICALL(Release, ctx->surface);
    ID3D11Texture2D_Release(ctx->tex); // shared texture
    destroy_amf(ctx->a);
    talloc_free(ctx);
}

static struct mp_image *mp_image_from_amf_surface(struct mp_filter *f, AMFSurface *src)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    mp_require(ICALL(GetMemoryType, src) == AMF_MEMORY_DX11);

    AMFPlane *plane = ICALL(GetPlane, src, AMF_PLANE_PACKED);
    if (!plane) {
        MP_ERR(f, "Failed to get plane from AMF surface.\n");
        return NULL;
    }

    ID3D11Texture2D *amf_tex = ICALL(GetNative, plane);
    if (!amf_tex) {
        MP_ERR(f, "Failed to get D3D11 texture from AMF plane.\n");
        return NULL;
    }

    ID3D11Device *amf_dev = ICALL(GetDX11Device, a->ctx, AMF_DX11_1);
    mp_require(amf_dev);

    ID3D11DeviceContext *ctx = NULL;
    ID3D11Device_GetImmediateContext(amf_dev, &ctx);
    mp_require(ctx);
    ID3D11DeviceContext_Flush(ctx);
    SAFE_RELEASE(ctx);

    IDXGIResource* amf_res;
    HRESULT hr = ID3D11Texture2D_QueryInterface(amf_tex, &IID_IDXGIResource, (void **)&amf_res);
    if (FAILED(hr)) {
        MP_ERR(f, "Failed to query IDXGIResource from AMF texture: %#lx\n", hr);
        return NULL;
    }

    HANDLE amf_shared;
    hr = IDXGIResource_GetSharedHandle(amf_res, &amf_shared);
    if (FAILED(hr)) {
        MP_ERR(f, "Failed to get shared handle from IDXGIResource: %#lx\n", hr);
        SAFE_RELEASE(amf_res);
        return NULL;
    }

    ID3D11Texture2D *tex;
    hr = ID3D11Device_OpenSharedResource(p->d3d_dev, amf_shared, &IID_ID3D11Texture2D, (void **)&tex);
    SAFE_RELEASE(amf_res);
    if (FAILED(hr)) {
        MP_ERR(f, "Failed to open shared resource: %#lx\n", hr);
        return NULL;
    }

    // At this point, we technically own a reference to a texture, but we have to
    // keep also AMFSurface alive, as the implementation seems to reuse the same
    // underlying texture for future frames.
    struct amf_surface_free_ctx *free_ctx = talloc(NULL, struct amf_surface_free_ctx);
    free_ctx->a = a;
    free_ctx->surface = src;
    free_ctx->tex = tex;
    struct mp_image *out = mp_image_new_custom_ref(p->ref_image, (void *)free_ctx, free_amf_surface);
    if (!out) {
        MP_ERR(f, "Unexpected error when creating mp_image.\n");
        free_amf_surface(free_ctx);
        return NULL;
    }
    mp_image_copy_attributes(out, p->ref_image);
    // hwctx is not copied by mp_image_copy_attributes
    out->hwctx = av_buffer_ref(p->ref_image->hwctx);
    out->planes[0] = (uint8_t *)tex;
    out->planes[1] = 0;
    out->pts = ICALL(GetPts, src) / (double)AMF_SECOND;

    // Keep context alive until all returned images are freed.
    mp_rc_ref(&a->ref_count);

    return out;
}

static int copy_tex(ID3D11DeviceContext *ctx, ID3D11Device *dev,
                    ID3D11Texture2D **dst, ID3D11Texture2D *src, int index)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(src, &desc);
    mp_require(desc.MipLevels == 1);

    if (!*dst) {
        desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        HRESULT hr = ID3D11Device_CreateTexture2D(dev, &desc, NULL, dst);
        if (FAILED(hr))
            return -1;
    }

    const D3D11_BOX srcBox = {0, 0, 0, desc.Width, desc.Height, 1};
    ID3D11DeviceContext_CopySubresourceRegion(ctx, (ID3D11Resource *)*dst,
                                              0, 0, 0, 0, (ID3D11Resource *)src,
                                              index, &srcBox);
    return 0;
}

static AMF_STD_CALL void amf_surface_dtor(AMFSurfaceObserver *observer, AMFSurface *surface)
{
    talloc_free(observer);
}

static const AMFSurfaceObserver amf_dtor = {
    .pVtbl = &(AMFSurfaceObserverVtbl){
        .OnSurfaceDataRelease = amf_surface_dtor,
    }
};

static AMFSurface *mp_image_to_amf_surface(struct mp_filter *f, struct mp_image *src)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;
    IDXGIResource* resource = NULL;
    ID3D11Texture2D *amf_tex = NULL;
    AMFSurfaceObserver *dtor = talloc_dup(NULL, (AMFSurfaceObserver *)&amf_dtor);
    talloc_steal(dtor, src); // release mp_image when no longer needed

    // The input format was already validated against IMGFMT_D3D11 in
    // amf_get_init_params() when the component was initialized.
    mp_require(src->imgfmt == IMGFMT_D3D11);

    ID3D11Texture2D *tex = (void *)src->planes[0];
    amf_int index = (intptr_t)src->planes[1];

    ID3D11DeviceContext *ctx = NULL;
    ID3D11Device_GetImmediateContext(p->d3d_dev, &ctx);
    mp_require(ctx);
    // Textures between AMF and our D3D11 device are shared using shared handles
    // `D3D11_RESOURCE_MISC_SHARED`, which is also set on textures returned
    // from AMF. These textures are implicitly synchronized, documentation states
    // that after a shared texture is updated, `ID3D11DeviceContext::Flush()`
    // must be called on the updating device. Flushing the command queue is
    // sufficient for the driver to synchronize access to the shared resource.
    // We intentionally do not use more explicit synchronization mechanisms such
    // as `IDXGIKeyedMutex` or `D3D11_RESOURCE_MISC_SHARED_NTHANDLE`, as they
    // are not supported by AMF, nor D3D11VA decoder and would require additional
    // intermediate copies.
    // However, resources created by a D3D11VA device are not implicitly
    // synchronized, so we perform a GPU-GPU copy before sharing the texture
    // with AMF. This ensures the video decoder has finished writing to the
    // texture. A copy is faster than synchronizing through the CPU, without copies.
    copy_tex(ctx, p->d3d_dev, &p->input_tex, tex, index);
    tex = p->input_tex;
    index = 0;
    ID3D11DeviceContext_Flush(ctx);
    SAFE_RELEASE(ctx);

    HRESULT hr = ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&resource);
    if (FAILED(hr)) {
        MP_ERR(f, "Failed to query IDXGIResource from texture: %#lx\n", hr);
        goto err;
    }

    HANDLE shared;
    hr = IDXGIResource_GetSharedHandle(resource, &shared);
    if (FAILED(hr)) {
        MP_ERR(f, "Failed to get shared handle from IDXGIResource: %#lx\n", hr);
        goto err;
    }

    ID3D11Device *amf_dev = ICALL(GetDX11Device, a->ctx, AMF_DX11_1); // not refcounted
    hr = ID3D11Device_OpenSharedResource(amf_dev, shared, &IID_ID3D11Texture2D, (void **)&amf_tex);
    SAFE_RELEASE(resource);
    if (FAILED(hr)) {
        MP_ERR(f, "Failed to open shared resource: %#lx\n", hr);
        goto err;
    }

    AMFSurface *out;
    AMF_RESULT res = ICALL(CreateSurfaceFromDX11Native, a->ctx, amf_tex, &out, dtor);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to create AMF surface from D3D11: %d\n", res);
        goto err;
    }

    // Copied from <https://github.com/GPUOpen-LibrariesAndSDKs/AMF/blob/afed28d37aca1938da2eedc50599bb3535a987ec/amf/public/src/components/Capture/MediaFoundation/MFCaptureImpl.cpp#L64>
    static const GUID AMFTextureArrayIndexGUID = {0x28115527, 0xe7c3, 0x4b66, {0x99, 0xd3, 0x4f, 0x2a, 0xe6, 0xb4, 0x7f, 0xaf}};
    ID3D11Resource_SetPrivateData(amf_tex, &AMFTextureArrayIndexGUID, sizeof(index), &index);
    SAFE_RELEASE(amf_tex);

    if (src->pts != MP_NOPTS_VALUE)
        ICALL(SetPts, out, (amf_pts)(src->pts * AMF_SECOND));
    else
        MP_WARN(f, "Warning: source image has no PTS.\n");

    return out;

err:
    SAFE_RELEASE(resource);
    SAFE_RELEASE(amf_tex);
    talloc_free(dtor);
    return NULL;
}

// Read the current input frame, (re)initialize the component on a parameter
// change and submit the frame to AMF. Returns AMF_OK/AMF_EOF when the frame was
// accepted (caller should query output), AMF_INPUT_FULL when the input queue is
// full, or AMF_FAIL on an already-logged fatal error.
static AMF_RESULT amf_submit_image(struct mp_filter *f, struct mp_image *in,
                                   amf_init_fn init)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    if (!mp_image_params_static_equal(&in->params, &p->params)) {
        MP_VERBOSE(f, "Input image parameters changed, reinitializing...\n");
        ICALL(Terminate, a->comp);
        p->params = in->params;
        if (init(f, in, false) < 0) {
            MP_ERR(f, "AMF component init failed\n");
            return AMF_FAIL;
        }
    }

    AMFSurface *surface = mp_image_to_amf_surface(f, in);
    if (!surface) {
        MP_WARN(f, "Failed to create AMF surface\n");
        return AMF_FAIL;
    }

    AMF_RESULT res = ICALL(SubmitInput, a->comp, (AMFData *)surface);
    // This shouldn't happen according to docs, but it does...
    if (res == AMF_EOF) {
        MP_ERR(f, "SubmitInput returned AMF_EOF, reinitializing...\n");
        if (init(f, p->ref_image, true) < 0) {
            MP_ERR(f, "AMF component reinit failed\n");
            ICALL(Release, surface);
            return AMF_FAIL;
        }
        res = ICALL(SubmitInput, a->comp, (AMFData *)surface);
    }

    ICALL(Release, surface);

    if (res != AMF_OK && res != AMF_EOF && res != AMF_INPUT_FULL) {
        MP_WARN(f, "SubmitInput failed: %d\n", res);
        return AMF_FAIL;
    }

    return res;
}

static struct mp_filter *amf_create(struct mp_filter *parent, void *options,
                                    const struct mp_filter_info *info,
                                    const wchar_t *component_name)
{
    struct mp_filter *f = mp_filter_create(parent, info);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    p->amf = talloc_zero(NULL, struct amf_priv);
    mp_rc_init(&p->amf->ref_count);

    if (load_amf_library(p->amf) < 0) {
        MP_ERR(f, "Failed to load AMF library\n");
        goto fail;
    }

    if (create_amf_context(f) < 0)
        goto fail;

    if (create_amf_component(f, component_name) < 0)
        goto fail;

    return f;

fail:
    talloc_free(f);
    return NULL;
}

// AMD Frame Rate Conversion (amf_frc)
//
// FRC runs in FRC_x2_PRESENT mode and emits two output frames per input: the
// first QueryOutput returns a frame with AMF_REPEAT (there is another one), the
// second returns it with AMF_OK. `has_frame` remembers that a second frame is
// pending so the next process() call drains it without submitting new input.

struct frc_opts {
    int profile;
    int mv_search_mode;
    bool fallback_blend;
    bool indicator;
    bool use_future_frame;
};

static int frc_init(struct mp_filter *f, struct mp_image *src, bool reinit)
{
    AMF_RESULT res;
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;
    struct frc_opts *opts = p->opts;

    mp_assert(a->comp);

    AMF_SURFACE_FORMAT amf_fmt;
    int width, height;
    if (amf_get_init_params(f, src, &amf_fmt, &width, &height) < 0)
        goto err;

    if (reinit)
        goto init;

    AMF_ASSIGN_PROPERTY_INT64(res, a->comp, AMF_FRC_ENGINE_TYPE, AMF_MEMORY_DX11);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set FRC engine type to DX11: %d\n", res);
        goto err;
    }

    AMF_ASSIGN_PROPERTY_INT64(res, a->comp, AMF_FRC_MODE, FRC_x2_PRESENT);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to set FRC mode to x2_PRESENT: %d\n", res);
        goto err;
    }

    int profile = opts->profile;
    if (profile == -1) {
        if (p->params.h >= 1440)
            profile = FRC_PROFILE_SUPER;
        else
            profile = FRC_PROFILE_HIGH;
    }
    AMF_ASSIGN_PROPERTY_INT64(res, a->comp, AMF_FRC_PROFILE, profile);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set FRC profile: %d\n", res);
        goto err;
    }

    AMF_ASSIGN_PROPERTY_INT64(res, a->comp, AMF_FRC_MV_SEARCH_MODE,
                              opts->mv_search_mode);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set FRC motion vector search mode: %d\n", res);
        goto err;
    }

    AMF_ASSIGN_PROPERTY_BOOL(res, a->comp, AMF_FRC_ENABLE_FALLBACK,
                             opts->fallback_blend);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set FRC fallback mode: %d\n", res);
        goto err;
    }

    AMF_ASSIGN_PROPERTY_BOOL(res, a->comp, AMF_FRC_INDICATOR, opts->indicator);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set FRC indicator: %d\n", res);
        goto err;
    }

    AMF_ASSIGN_PROPERTY_BOOL(res, a->comp, AMF_FRC_USE_FUTURE_FRAME,
                             opts->use_future_frame);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set FRC use_future_frame: %d\n", res);
        goto err;
    }

init:
    if (amf_component_init(f, amf_fmt, width, height) < 0)
        goto err;

    return 0;

err:
    ICALL(Terminate, a->comp);
    return -1;
}

static void frc_process(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    if (!p->has_frame && !mp_pin_out_request_data(f->ppins[0]))
        return;

    // AMF_REPEAT requested or draining, we likely have a frame to output
    if (p->has_frame)
        goto read_out;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (frame.type == MP_FRAME_NONE) {
        MP_WARN(f, "Needs a frame, but got MP_FRAME_NONE...\n");
        return;
    }

    if (frame.type == MP_FRAME_EOF) {
        ICALL(Drain, a->comp);
        goto read_out;
    }

    if (frame.type != MP_FRAME_VIDEO) {
        MP_ERR(f, "Unexpected frame type: %d\n", frame.type);
        mp_frame_unref(&frame);
        mp_filter_internal_mark_failed(f);
        return;
    }

    struct mp_image *in = frame.data;
    mp_image_unrefp(&p->ref_image);
    p->ref_image = mp_image_new_ref(in);

    AMF_RESULT res = amf_submit_image(f, in, frc_init);

    if (res == AMF_INPUT_FULL) {
        MP_WARN(f, "Input queue full...\n");
        frame = (struct mp_frame){MP_FRAME_VIDEO, mp_image_new_ref(p->ref_image)};
        mp_pin_out_unread(f->ppins[0], frame);
        return;
    }

    if (res == AMF_FAIL) {
        mp_filter_internal_mark_failed(f);
        return;
    }

read_out:;
    AMFSurface *amf_out = NULL;
    res = ICALL(QueryOutput, a->comp, (AMFData **)&amf_out);

    if (res == AMF_EOF) {
        p->has_frame = false;
        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_EOF, NULL));
        return;
    }

    if (res != AMF_OK && res != AMF_REPEAT) {
        MP_WARN(f, "QueryOutput returned: %d\n", res);
        mp_filter_internal_mark_failed(f);
        return;
    }

    if (!amf_out && res == AMF_REPEAT) {
        MP_WARN(f, "QueryOutput returned NULL surface with AMF_REPEAT\n");
        mp_filter_internal_mark_failed(f);
        return;
    }

    struct mp_image *out = mp_image_from_amf_surface(f, amf_out);
    if (!out) {
        MP_WARN(f, "Failed to create mp_image from AMF surface\n");
        ICALL(Release, amf_out);
        mp_filter_internal_mark_failed(f);
        return;
    }

    if (res == AMF_OK) {
        p->has_frame = false;
        mp_image_unrefp(&p->ref_image);
    } else if (res == AMF_REPEAT) {
        p->has_frame = true;
    } else {
        MP_ASSERT_UNREACHABLE();
    }

    mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, out));
}

static const struct mp_filter_info vf_amf_frc_filter = {
    .name = "amf_frc",
    .process = frc_process,
    .reset = flush,
    .destroy = destroy,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_amf_frc_create(struct mp_filter *parent,
                                           void *options)
{
    return amf_create(parent, options, &vf_amf_frc_filter, AMFFRC);
}

#define OPT_BASE_STRUCT struct frc_opts
static const m_option_t vf_frc_opts_fields[] = {
    {"profile", OPT_CHOICE(profile,
        {"auto", -1},
        {"low", FRC_PROFILE_LOW},
        {"high", FRC_PROFILE_HIGH},
        {"super", FRC_PROFILE_SUPER})},
    {"mv-search", OPT_CHOICE(mv_search_mode,
        {"native", FRC_MV_SEARCH_NATIVE},
        {"performance", FRC_MV_SEARCH_PERFORMANCE})},
    {"fallback", OPT_BOOL(fallback_blend)},
    {"indicator", OPT_BOOL(indicator)},
    {"future-frame", OPT_BOOL(use_future_frame)},
    {0}
};

const struct mp_user_filter_entry vf_amf_frc = {
    .desc = {
        .description = "AMD Frame Rate Conversion filter",
        .name = "amf_frc",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .profile = -1,
            .mv_search_mode = FRC_MV_SEARCH_NATIVE,
            .use_future_frame = true,
        },
        .options = vf_frc_opts_fields,
    },
    .create = vf_amf_frc_create,
};
#undef OPT_BASE_STRUCT

// AMD Video Quality Enhancer (amf_vqe)
//
// VQE is a plain one frame in, one frame out component: each SubmitInput yields
// at most one output frame, so unlike amf_frc it does not change the frame rate
// and does not use `has_frame`. A NULL output surface simply means more input is
// required and is not an error.

struct vqe_opts {
    double attenuation;
};

static int vqe_init(struct mp_filter *f, struct mp_image *src, bool reinit)
{
    AMF_RESULT res;
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;
    struct vqe_opts *opts = p->opts;

    mp_assert(a->comp);

    AMF_SURFACE_FORMAT amf_fmt;
    int width, height;
    if (amf_get_init_params(f, src, &amf_fmt, &width, &height) < 0)
        goto err;

    if (reinit)
        goto init;

    AMF_ASSIGN_PROPERTY_INT64(res, a->comp, AMF_VIDEO_ENHANCER_ENGINE_TYPE,
                              AMF_MEMORY_DX11);
    if (res != AMF_OK) {
        MP_WARN(f, "Failed to set VQE engine type to DX11: %d\n", res);
        goto err;
    }

    AMF_ASSIGN_PROPERTY_DOUBLE(res, a->comp, AMF_VE_FCR_ATTENUATION,
                               opts->attenuation);
    if (res != AMF_OK) {
        MP_ERR(f, "Failed to set VQE attenuation: %d\n", res);
        goto err;
    }

    // Documented as required for the enhancement to be performed, but not
    // exposed by every runtime version, in which case it is not needed.
    AMFSize size = AMFConstructSize(width, height);
    AMF_ASSIGN_PROPERTY_SIZE(res, a->comp, AMF_VIDEO_ENHANCER_OUTPUT_SIZE, size);
    if (res != AMF_OK)
        MP_VERBOSE(f, "Failed to set VQE output size: %d\n", res);

init:
    if (amf_component_init(f, amf_fmt, width, height) < 0)
        goto err;

    return 0;

err:
    ICALL(Terminate, a->comp);
    return -1;
}

static void vqe_process(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct amf_priv *a = p->amf;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    if (!mp_pin_out_request_data(f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (frame.type == MP_FRAME_NONE) {
        MP_WARN(f, "Needs a frame, but got MP_FRAME_NONE...\n");
        return;
    }

    if (frame.type == MP_FRAME_EOF) {
        ICALL(Drain, a->comp);
        goto read_out;
    }

    if (frame.type != MP_FRAME_VIDEO) {
        MP_ERR(f, "Unexpected frame type: %d\n", frame.type);
        mp_frame_unref(&frame);
        mp_filter_internal_mark_failed(f);
        return;
    }

    struct mp_image *in = frame.data;
    mp_image_unrefp(&p->ref_image);
    p->ref_image = mp_image_new_ref(in);

    AMF_RESULT res = amf_submit_image(f, in, vqe_init);

    if (res == AMF_INPUT_FULL) {
        // Queue full: put the frame back and drain one output below to free a
        // slot, otherwise we would re-read and re-submit the same frame.
        frame = (struct mp_frame){MP_FRAME_VIDEO, mp_image_new_ref(p->ref_image)};
        mp_pin_out_unread(f->ppins[0], frame);
    } else if (res == AMF_FAIL) {
        mp_filter_internal_mark_failed(f);
        return;
    }

read_out:;
    AMFSurface *amf_out = NULL;
    res = ICALL(QueryOutput, a->comp, (AMFData **)&amf_out);

    if (res == AMF_EOF) {
        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_EOF, NULL));
        return;
    }

    if (res != AMF_OK && res != AMF_REPEAT) {
        MP_WARN(f, "QueryOutput returned: %d\n", res);
        mp_filter_internal_mark_failed(f);
        return;
    }

    // No output yet: the component needs more input. Not an error.
    if (!amf_out)
        return;

    struct mp_image *out = mp_image_from_amf_surface(f, amf_out);
    if (!out) {
        MP_WARN(f, "Failed to create mp_image from AMF surface\n");
        ICALL(Release, amf_out);
        mp_filter_internal_mark_failed(f);
        return;
    }

    mp_image_unrefp(&p->ref_image);
    mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, out));
}

static const struct mp_filter_info vf_amf_vqe_filter = {
    .name = "amf_vqe",
    .process = vqe_process,
    .reset = flush,
    .destroy = destroy,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_amf_vqe_create(struct mp_filter *parent,
                                           void *options)
{
    return amf_create(parent, options, &vf_amf_vqe_filter, AMFVQEnhancer);
}

#define OPT_BASE_STRUCT struct vqe_opts
static const m_option_t vf_vqe_opts_fields[] = {
    {"attenuation", OPT_DOUBLE(attenuation), M_RANGE(0.02, 0.4)},
    {0}
};

const struct mp_user_filter_entry vf_amf_vqe = {
    .desc = {
        .description = "AMD Video Quality Enhancer filter",
        .name = "amf_vqe",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .attenuation = VE_FCR_DEFAULT_ATTENUATION,
        },
        .options = vf_vqe_opts_fields,
    },
    .create = vf_amf_vqe_create,
};
#undef OPT_BASE_STRUCT
