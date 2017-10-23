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
    HWDEC_VDPAU_COPY,
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
    HWDEC_MEDIACODEC_COPY,
    HWDEC_CUDA,
    HWDEC_CUDA_COPY,
    HWDEC_CRYSTALHD,
    HWDEC_RKMPP,
};

#define HWDEC_IS_AUTO(x) ((x) == HWDEC_AUTO || (x) == HWDEC_AUTO_COPY)

// hwdec_type names (options.c)
extern const struct m_opt_choice_alternatives mp_hwdec_names[];

struct mp_hwdec_ctx {
    enum hwdec_type type; // (never HWDEC_NONE or HWDEC_IS_AUTO)
    const char *driver_name; // NULL if unknown/not loaded

    // This is never NULL. Its meaning depends on the .type field:
    //  HWDEC_VDPAU:            struct mp_vdpau_ctx*
    //  HWDEC_VIDEOTOOLBOX:     non-NULL dummy pointer
    //  HWDEC_VAAPI:            struct mp_vaapi_ctx*
    //  HWDEC_D3D11VA:          ID3D11Device*
    //  HWDEC_DXVA2:            IDirect3DDevice9*
    //  HWDEC_CUDA:             CUcontext*
    void *ctx;

    // libavutil-wrapped context, if available.
    struct AVBufferRef *av_device_ref; // AVHWDeviceContext*

    // List of IMGFMT_s, terminated with 0. NULL if N/A.
    int *supported_formats;

    // Hint to generic code: it's using a wrapper API
    bool emulated;

    // Optional. Crap for vdpau. Makes sure preemption recovery is run if needed.
    void (*restore_device)(struct mp_hwdec_ctx *ctx);

    // Optional. Do not set for VO-bound devices.
    void (*destroy)(struct mp_hwdec_ctx *ctx);
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

struct mp_image;

// Per AV_HWDEVICE_TYPE_* functions, queryable via hwdec_get_hwcontext_fns().
// For now, all entries are strictly optional.
struct hwcontext_fns {
    int av_hwdevice_type;
    // Set any mp_image fields that require hwcontext specific code, such as
    // fields or flags not present in AVFrame or AVHWFramesContext in a
    // portable way. This is called directly after img is converted from an
    // AVFrame, with all other fields already set. img.hwctx will be set, and
    // use the correct AV_HWDEVICE_TYPE_.
    void (*complete_image_params)(struct mp_image *img);
};

// The parameter is of type enum AVHWDeviceType (as in int to avoid extensive
// recursive includes). May return NULL for unknown device types.
const struct hwcontext_fns *hwdec_get_hwcontext_fns(int av_hwdevice_type);

#endif
