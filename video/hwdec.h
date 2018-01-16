#ifndef MP_HWDEC_H_
#define MP_HWDEC_H_

#include <libavutil/buffer.h>

#include "options/m_option.h"

struct mp_image_pool;

struct mp_hwdec_ctx {
    const char *driver_name; // NULL if unknown/not loaded

    // libavutil-wrapped context, if available.
    struct AVBufferRef *av_device_ref; // AVHWDeviceContext*

    // List of IMGFMT_s, terminated with 0. NULL if N/A.
    const int *supported_formats;
    // HW format for which above hw_subfmts are valid.
    int hw_imgfmt;
};

// Used to communicate hardware decoder device handles from VO to video decoder.
struct mp_hwdec_devices;

struct mp_hwdec_devices *hwdec_devices_create(void);
void hwdec_devices_destroy(struct mp_hwdec_devices *devs);

// Return the device context for the given API type. Returns NULL if none
// available. Logically, the returned pointer remains valid until VO
// uninitialization is started (all users of it must be uninitialized before).
// hwdec_devices_request() may be used before this to lazily load devices.
// Contains a wrapped AVHWDeviceContext.
// Beware that this creates a _new_ reference.
struct AVBufferRef *hwdec_devices_get_lavc(struct mp_hwdec_devices *devs,
                                           int av_hwdevice_type);

struct mp_hwdec_ctx *hwdec_devices_get_by_lavc(struct mp_hwdec_devices *devs,
                                               int av_hwdevice_type);

// For code which still strictly assumes there is 1 (or none) device.
struct mp_hwdec_ctx *hwdec_devices_get_first(struct mp_hwdec_devices *devs);

// Return the n-th device. NULL if none.
struct mp_hwdec_ctx *hwdec_devices_get_n(struct mp_hwdec_devices *devs, int n);

// Add this to the list of internal devices. Adding the same pointer twice must
// be avoided.
void hwdec_devices_add(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx);

// Remove this from the list of internal devices. Idempotent/ignores entries
// not added yet. This is not thread-safe.
void hwdec_devices_remove(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx);

// Can be used to enable lazy loading of an API with hwdec_devices_request().
// If used at all, this must be set/unset during initialization/uninitialization,
// as concurrent use with hwdec_devices_request() is a race condition.
void hwdec_devices_set_loader(struct mp_hwdec_devices *devs,
    void (*load_api)(void *ctx), void *load_api_ctx);

// Cause VO to lazily load all devices, and will block until this is done (even
// if not available).
void hwdec_devices_request_all(struct mp_hwdec_devices *devs);

// Return "," concatenated list (for introspection/debugging). Use talloc_free().
char *hwdec_devices_get_names(struct mp_hwdec_devices *devs);

struct mp_image;
struct mpv_global;

struct hwcontext_create_dev_params {
    bool probing;   // if true, don't log errors if unavailable
};

// Per AV_HWDEVICE_TYPE_* functions, queryable via hwdec_get_hwcontext_fns().
// All entries are strictly optional.
struct hwcontext_fns {
    int av_hwdevice_type;
    // Set any mp_image fields that require hwcontext specific code, such as
    // fields or flags not present in AVFrame or AVHWFramesContext in a
    // portable way. This is called directly after img is converted from an
    // AVFrame, with all other fields already set. img.hwctx will be set, and
    // use the correct AV_HWDEVICE_TYPE_.
    void (*complete_image_params)(struct mp_image *img);
    // Fill in special format-specific requirements.
    void (*refine_hwframes)(struct AVBufferRef *hw_frames_ctx);
    // Returns a AVHWDeviceContext*. Used for copy hwdecs.
    struct AVBufferRef *(*create_dev)(struct mpv_global *global,
                                      struct mp_log *log,
                                      struct hwcontext_create_dev_params *params);
    // Return whether this is using some sort of sub-optimal emulation layer.
    bool (*is_emulated)(struct AVBufferRef *hw_device_ctx);
};

// The parameter is of type enum AVHWDeviceType (as in int to avoid extensive
// recursive includes). May return NULL for unknown device types.
const struct hwcontext_fns *hwdec_get_hwcontext_fns(int av_hwdevice_type);

extern const struct hwcontext_fns hwcontext_fns_d3d11;
extern const struct hwcontext_fns hwcontext_fns_dxva2;
extern const struct hwcontext_fns hwcontext_fns_vaapi;
extern const struct hwcontext_fns hwcontext_fns_vdpau;

#endif
