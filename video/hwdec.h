#ifndef MP_HWDEC_H_
#define MP_HWDEC_H_

#include "options/m_option.h"

struct mp_image_pool;

// keep in sync with --hwdec option (see mp_hwdec_names)
enum hwdec_type {
    HWDEC_NONE = 0,
    HWDEC_AUTO,
    HWDEC_AUTO_COPY,
    HWDEC_VDPAU,
    HWDEC_VIDEOTOOLBOX,
    HWDEC_VIDEOTOOLBOX_COPY,
    HWDEC_VAAPI,
    HWDEC_VAAPI_COPY,
    HWDEC_DXVA2,
    HWDEC_DXVA2_COPY,
    HWDEC_D3D11VA,
    HWDEC_D3D11VA_COPY,
    HWDEC_RPI,
    HWDEC_RPI_COPY,
    HWDEC_MEDIACODEC,
    HWDEC_CUDA,
    HWDEC_CUDA_COPY,
};

#define HWDEC_IS_AUTO(x) ((x) == HWDEC_AUTO || (x) == HWDEC_AUTO_COPY)

// hwdec_type names (options.c)
extern const struct m_opt_choice_alternatives mp_hwdec_names[];

struct mp_hwdec_ctx {
    enum hwdec_type type; // (never HWDEC_NONE or HWDEC_IS_AUTO)
    const char *driver_name; // NULL if unknown/not loaded

    // This is never NULL. Its meaning depends on the .type field:
    //  HWDEC_VDPAU:            struct mp_vaapi_ctx*
    //  HWDEC_VIDEOTOOLBOX:     struct mp_vt_ctx*
    //  HWDEC_VAAPI:            struct mp_vaapi_ctx*
    //  HWDEC_D3D11VA:          ID3D11Device*
    //  HWDEC_DXVA2:            IDirect3DDevice9*
    //  HWDEC_DXVA2_COPY:       IDirect3DDevice9*
    void *ctx;

    // Optional.
    // Allocates a software image from the pool, downloads the hw image from
    // mpi, and returns it.
    // pool can be NULL (then just use straight allocation).
    // Return NULL on error or if mpi has the wrong format.
    struct mp_image *(*download_image)(struct mp_hwdec_ctx *ctx,
                                       struct mp_image *mpi,
                                       struct mp_image_pool *swpool);
};

struct mp_vt_ctx {
    void *priv;
    uint32_t (*get_vt_fmt)(struct mp_vt_ctx *ctx);
};

// Used to communicate hardware decoder device handles from VO to video decoder.
struct mp_hwdec_devices;

struct mp_hwdec_devices *hwdec_devices_create(void);
void hwdec_devices_destroy(struct mp_hwdec_devices *devs);

// Return the device context for the given API type. Returns NULL if none
// available. Logically, the returned pointer remains valid until VO
// uninitialization is started (all users of it must be uninitialized before).
// hwdec_devices_request() may be used before this to lazily load devices.
struct mp_hwdec_ctx *hwdec_devices_get(struct mp_hwdec_devices *devs,
                                       enum hwdec_type type);

// For code which still strictly assumes there is 1 (or none) device.
struct mp_hwdec_ctx *hwdec_devices_get_first(struct mp_hwdec_devices *devs);

// Add this to the list of internal devices. Adding the same pointer twice must
// be avoided.
void hwdec_devices_add(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx);

// Remove this from the list of internal devices. Idempotent/ignores entries
// not added yet.
void hwdec_devices_remove(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx);

// Can be used to enable lazy loading of an API with hwdec_devices_request().
// If used at all, this must be set/unset during initialization/uninitialization,
// as concurrent use with hwdec_devices_request() is a race condition.
void hwdec_devices_set_loader(struct mp_hwdec_devices *devs,
    void (*load_api)(void *ctx, enum hwdec_type type), void *load_api_ctx);

// Cause VO to lazily load the requested device, and will block until this is
// done (even if not available).
void hwdec_devices_request(struct mp_hwdec_devices *devs, enum hwdec_type type);

// Convenience function:
// - return NULL if devs==NULL
// - call hwdec_devices_request(devs, type)
// - call hwdec_devices_get(devs, type)
// - then return the mp_hwdec_ctx.ctx field
void *hwdec_devices_load(struct mp_hwdec_devices *devs, enum hwdec_type type);

#endif
