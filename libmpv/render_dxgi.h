#ifndef MPV_CLIENT_API_RENDER_DXGI_H_
#define MPV_CLIENT_API_RENDER_DXGI_H_

#include "render.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mpv_dxgi_init_params {
    void *device;
    void *swapchain;
} mpv_dxgi_init_params;

#ifdef __cplusplus
}
#endif
#endif // MPV_CLIENT_API_RENDER_DXGI_H_
