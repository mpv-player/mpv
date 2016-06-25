#include <pthread.h>
#include <assert.h>

#include "hwdec.h"

struct mp_hwdec_devices {
    pthread_mutex_t lock;

    struct mp_hwdec_ctx *hwctx;

    void (*load_api)(void *ctx, enum hwdec_type type);
    void *load_api_ctx;
};

struct mp_hwdec_devices *hwdec_devices_create(void)
{
    struct mp_hwdec_devices *devs = talloc_zero(NULL, struct mp_hwdec_devices);
    pthread_mutex_init(&devs->lock, NULL);
    return devs;
}

void hwdec_devices_destroy(struct mp_hwdec_devices *devs)
{
    if (!devs)
        return;
    assert(!devs->hwctx); // must have been hwdec_devices_remove()ed
    assert(!devs->load_api); // must have been unset
    pthread_mutex_destroy(&devs->lock);
    talloc_free(devs);
}

struct mp_hwdec_ctx *hwdec_devices_get(struct mp_hwdec_devices *devs,
                                       enum hwdec_type type)
{
    struct mp_hwdec_ctx *res = NULL;
    pthread_mutex_lock(&devs->lock);
    if (devs->hwctx && devs->hwctx->type == type)
        res = devs->hwctx;
    pthread_mutex_unlock(&devs->lock);
    return res;
}

struct mp_hwdec_ctx *hwdec_devices_get_first(struct mp_hwdec_devices *devs)
{
    pthread_mutex_lock(&devs->lock);
    struct mp_hwdec_ctx *res = devs->hwctx;
    pthread_mutex_unlock(&devs->lock);
    return res;
}

void hwdec_devices_add(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx)
{
    pthread_mutex_lock(&devs->lock);
    // We support only 1 device; ignore the rest.
    if (!devs->hwctx)
        devs->hwctx = ctx;
    pthread_mutex_unlock(&devs->lock);
}

void hwdec_devices_remove(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx)
{
    pthread_mutex_lock(&devs->lock);
    if (devs->hwctx == ctx)
        devs->hwctx = NULL;
    pthread_mutex_unlock(&devs->lock);
}

void hwdec_devices_set_loader(struct mp_hwdec_devices *devs,
    void (*load_api)(void *ctx, enum hwdec_type type), void *load_api_ctx)
{
    devs->load_api = load_api;
    devs->load_api_ctx = load_api_ctx;
}

// Cause VO to lazily load the requested device, and will block until this is
// done (even if not available).
void hwdec_devices_request(struct mp_hwdec_devices *devs, enum hwdec_type type)
{
    if (devs->load_api && !hwdec_devices_get_first(devs))
        devs->load_api(devs->load_api_ctx, type);
}

void *hwdec_devices_load(struct mp_hwdec_devices *devs, enum hwdec_type type)
{
    if (!devs)
        return NULL;
    hwdec_devices_request(devs, type);
    struct mp_hwdec_ctx *hwctx = hwdec_devices_get(devs, type);
    return hwctx ? hwctx->ctx : NULL;
}
