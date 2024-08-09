#include <assert.h>

#include <libavutil/hwcontext.h>

#include "config.h"
#include "hwdec.h"
#include "osdep/threads.h"

struct mp_hwdec_devices {
    mp_mutex lock;

    struct mp_hwdec_ctx **hwctxs;
    int num_hwctxs;

    void (*load_api)(void *ctx,
                     struct hwdec_imgfmt_request *params);
    void *load_api_ctx;
};

struct mp_hwdec_devices *hwdec_devices_create(void)
{
    struct mp_hwdec_devices *devs = talloc_zero(NULL, struct mp_hwdec_devices);
    mp_mutex_init(&devs->lock);
    return devs;
}

void hwdec_devices_destroy(struct mp_hwdec_devices *devs)
{
    if (!devs)
        return;
    assert(!devs->num_hwctxs); // must have been hwdec_devices_remove()ed
    assert(!devs->load_api); // must have been unset
    mp_mutex_destroy(&devs->lock);
    talloc_free(devs);
}

struct mp_hwdec_ctx *hwdec_devices_get_by_imgfmt(struct mp_hwdec_devices *devs,
                                                 int hw_imgfmt)
{
    struct mp_hwdec_ctx *res = NULL;
    mp_mutex_lock(&devs->lock);
    for (int n = 0; n < devs->num_hwctxs; n++) {
        struct mp_hwdec_ctx *dev = devs->hwctxs[n];
        if (dev->hw_imgfmt == hw_imgfmt) {
            res = dev;
            break;
        }
    }
    mp_mutex_unlock(&devs->lock);
    return res;
}

struct mp_hwdec_ctx *hwdec_devices_get_first(struct mp_hwdec_devices *devs)
{
    return hwdec_devices_get_n(devs, 0);
}

struct mp_hwdec_ctx *hwdec_devices_get_n(struct mp_hwdec_devices *devs, int n)
{
    mp_mutex_lock(&devs->lock);
    struct mp_hwdec_ctx *res = n < devs->num_hwctxs ? devs->hwctxs[n] : NULL;
    mp_mutex_unlock(&devs->lock);
    return res;
}

void hwdec_devices_add(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx)
{
    mp_mutex_lock(&devs->lock);
    MP_TARRAY_APPEND(devs, devs->hwctxs, devs->num_hwctxs, ctx);
    mp_mutex_unlock(&devs->lock);
}

void hwdec_devices_remove(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx)
{
    mp_mutex_lock(&devs->lock);
    for (int n = 0; n < devs->num_hwctxs; n++) {
        if (devs->hwctxs[n] == ctx) {
            MP_TARRAY_REMOVE_AT(devs->hwctxs, devs->num_hwctxs, n);
            break;
        }
    }
    mp_mutex_unlock(&devs->lock);
}

void hwdec_devices_set_loader(struct mp_hwdec_devices *devs,
    void (*load_api)(void *ctx, struct hwdec_imgfmt_request *params),
    void *load_api_ctx)
{
    devs->load_api = load_api;
    devs->load_api_ctx = load_api_ctx;
}

void hwdec_devices_request_for_img_fmt(struct mp_hwdec_devices *devs,
                                       struct hwdec_imgfmt_request *params)
{
    if (devs->load_api)
        devs->load_api(devs->load_api_ctx, params);
}

char *hwdec_devices_get_names(struct mp_hwdec_devices *devs)
{
    char *res = NULL;
    for (int n = 0; n < devs->num_hwctxs; n++) {
        if (res)
            ta_xstrdup_append(&res, ",");
        ta_xstrdup_append(&res, devs->hwctxs[n]->driver_name);
    }
    return res;
}

static const struct hwcontext_fns *const hwcontext_fns[] = {
#if HAVE_CUDA_HWACCEL
    &hwcontext_fns_cuda,
#endif
#if HAVE_D3D_HWACCEL
    &hwcontext_fns_d3d11,
#endif
#if HAVE_D3D9_HWACCEL
    &hwcontext_fns_dxva2,
#endif
#if HAVE_DRM
    &hwcontext_fns_drmprime,
#endif
#if HAVE_V4L2REQUEST
    &hwcontext_fns_v4l2request,
#endif
#if HAVE_VAAPI
    &hwcontext_fns_vaapi,
#endif
#if HAVE_VDPAU
    &hwcontext_fns_vdpau,
#endif
    NULL,
};

const struct hwcontext_fns *hwdec_get_hwcontext_fns(int av_hwdevice_type)
{
    for (int n = 0; hwcontext_fns[n]; n++) {
        if (hwcontext_fns[n]->av_hwdevice_type == av_hwdevice_type)
            return hwcontext_fns[n];
    }
    return NULL;
}
